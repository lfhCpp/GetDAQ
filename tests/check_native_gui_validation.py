#!/usr/bin/env python3
"""独立复核实际Qt→ECC→receiver的Run绑定与封存，严格区分历史和当前源码。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_ecc_network_validation import require, sha

SCENARIOS = {"qt-gui-0", "qt-gui-1"}
COUNTS = {"describe": 1, "prepare": 1, "configure": 1, "start": 1, "stop": 1, "pause": 1, "resume": 1, "breakup": 1, "reset": 2}


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def check_run(root, snapshots=None):
    results = json.loads((root / "results.json").read_text())
    require(len(results) == 2 and {r["scenario"] for r in results} == SCENARIOS and all(r["exit_code"] == 0 for r in results), "Actual Qt scenario coverage/exit differs")
    programs = json.loads((root / "program-sha256.json").read_text())
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service", "qt_probe"}, "Qt actual program set differs")
    for name, record in programs.items():
        require(sha(snapshots / name if snapshots else Path(record["path"])) == record["sha256"], "Tested program changed: " + name)
    for result in results:
        case = root / result["scenario"]
        scope = json.loads((case / "scope.json").read_text())
        require(scope["passed"] and scope["real_qt_runcontrol"] and scope["real_ecc_network_service"] and scope["real_data_services"] == 2 and
                scope["real_receivers"] and scope["full_ecc_workflow_tested"] and scope["standalone_cobo_mode"] and scope["physical_hosts"] == 1 and
                not scope["real_hardware"] and not scope["throughput_verified"], "Qt case scope differs")
        identity = json.loads((case / "identity.json").read_text())
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"] and sha(case / "controller/experiment.json") == identity["artifact_sha256"], "Parent input binding differs")
        events = rows(case / "controller/network/operations.jsonl")
        require([e["sequence"] for e in events] == list(range(len(events))), "Server journal sequence differs")
        accepted = {e["operation_id"]: e for e in events if e["event"] == "accepted"}
        require(len(accepted) == sum(e["event"] == "accepted" for e in events), "Repeated accepted operation ID")
        commands = Counter()
        original = {}
        for key, event in accepted.items():
            request = json.loads(event["request"])
            require(request["operation_id"] == key and request["instance_id"] == identity["instance_id"] and request["snapshot_sha256"] == identity["snapshot_sha256"], "Accepted request scope differs")
            require((case / "controller/network/operations" / key / "request.json").read_text() == event["request"], "Persisted server intent differs")
            require(hashlib.sha256(("getdaq.ecc-control-request:1\n" + event["request"]).encode()).hexdigest() == event["request_key"], "Accepted request digest differs")
            commands[request["command"]] += 1
            original[key] = event["request"]
        require(dict(commands) == COUNTS, "GUI duplicated/skipped whole-system commands")
        terminal = [e for e in events if e["event"] == "complete"]
        require(len(terminal) == len(accepted) and {e["operation_id"] for e in terminal} == set(accepted) and not any(e["event"] in {"failed", "unknown"} for e in events), "Actual server command results differ")
        client_rows = rows(case / "gui-client-records/observations.jsonl")
        require([r["sequence"] for r in client_rows] == list(range(len(client_rows))), "Client journal sequence differs")
        intents = [r for r in client_rows if r["kind"] == "intent"]
        require(len(intents) == len(accepted), "Client command intent count differs")
        observed = set()
        for row in client_rows:
            payload = json.loads(row["payload"])
            if row["kind"] == "intent":
                key = payload["operation_id"]
                require(row["payload"] == original[key] and (case / "gui-client-records/requests" / (key + ".json")).read_text() == original[key], "Client/server original intent differs")
            else:
                require(row["kind"] == "observation" and payload["request"] == json.loads(original[payload["request"]["operation_id"]]), "Client result request differs")
                if payload["phase"] == "complete": observed.add(payload["request"]["operation_id"])
        require(observed == set(accepted), "GUI lacks terminal evidence")
        run = case / "gui-runs/hardware/NativeIntegration/run_0000"
        journal = json.loads((run / "journal.json").read_text())
        require(journal["phase"] == "Complete" and (run / "run.csv").is_file() and not (run / "configuration").exists(), "Run completion or claimed local configuration differs")
        require(json.loads(journal["native_identity"]) == identity and json.loads((run / "ecc-identity.json").read_text()) == identity, "Run identity differs")
        run_requests = journal["native_operations"]
        require(Counter(json.loads(s)["command"] for s in run_requests.values()) == {"start": 1, "pause": 1, "resume": 1, "stop": 1, "breakup": 1} and
                all(original[key] == value for key, value in run_requests.items()), "Run-owned operation binding differs")
        ready = json.loads(journal["receiver_ready"])
        stopped = json.loads((run / "receiver-status.json").read_text())
        require(ready["controller"]["state"] == 4 and stopped["controller"]["state"] == 3 and not stopped["operation_active"] and
                ready["snapshot_sha256"] == stopped["snapshot_sha256"] == identity["snapshot_sha256"] and
                ready["instance_id"] == stopped["instance_id"] == identity["instance_id"], "Run Ready/Prepared proof differs")
        initial = {r["id"]: r for n in ready["data"]["nodes"] for r in n["lifecycle"]["receivers"]}
        final = {r["id"]: r for n in stopped["data"]["nodes"] for r in n["lifecycle"]["receivers"]}
        require(set(initial) == set(final) == {"C0", "C1"}, "Run receiver coverage differs")
        for name, receiver in final.items():
            require(all(initial[name][key] == receiver[key] for key in ("pid", "session_id", "output_directory")) and receiver["pid"] > 0 and receiver["reaped"] and receiver["exit_code"] == 0 and receiver["seal_verified"] and
                    receiver["received_bytes"] == "16384" and receiver["frames"] == "4" and receiver["files"] == "1", "Run receiver session/EOF/counters differ")
            relative = Path(receiver["output_directory"]).relative_to(Path(result["command"][6]))
            output = case / relative
            seal = json.loads((output / ".getdaq-receiver-seal.json").read_text())
            require(seal["sender_id"] == name and seal["pid"] == receiver["pid"] and seal["session_id"] == receiver["session_id"] and
                    seal["complete"] and seal["durable"] and seal["sender_eof_confirmed"] and seal["connections"] == 1 and
                    seal["configuration_sha256"] == identity["configuration_sha256"] and seal["artifact_sha256"] == identity["artifact_sha256"], "Actual receiver seal differs from GUI record")
            require(sum((output / f["name"]).stat().st_size for f in seal["files"]) == int(receiver["received_bytes"]) == int(seal["received_bytes"]), "Actual data size differs")
        final_status = json.loads((case / "final-status.json").read_text())
        require(final_status["controller"]["state"] == 1 and not final_status["control_needs_reconciliation"], "Reset did not return to Idle")
        credentials = {p for pattern in ("credential", "ecc-credential") for p in case.rglob(pattern)}
        require(not snapshots or not credentials, "Credential archived")
        values = [p.read_bytes().strip() for p in credentials]
        for path in case.rglob("*"):
            if path.is_file() and path not in credentials:
                require(not any(value in path.read_bytes() for value in values), "Credential exposed outside private file")
    return len(programs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--qt-build-dirs", type=Path, nargs="+", required=True)
    parser.add_argument("--native-sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--historical-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    require(not args.historical_only or args.archive, "Historical mode requires archive")
    archive = args.archive.resolve() if args.archive else None
    if archive:
        manifest = json.loads((archive / "files-sha256.json").read_text())
        require(set(manifest) == {str(p.relative_to(archive)) for p in archive.rglob("*") if p.is_file() and p.name != "files-sha256.json"}, "Archive file set differs")
        for name, digest in manifest.items(): require(sha(archive / name) == digest, "Archive bytes changed")
    count = 0
    for kind, directory in (("normal", args.normal), ("sanitized", args.sanitized)):
        count += check_run(directory.resolve(), archive / "programs" / kind if args.historical_only else None)
    source_root = archive / "verified-sources" if args.historical_only else root
    builds = args.qt_build_dirs + [args.native_sanitizer_build_dir]
    for build in builds:
        for name, digest in json.loads((build / "source-sha256.json").read_text()).items():
            require(sha(source_root / name) == digest, "Current compiled source changed: " + name)
    for build in args.qt_build_dirs:
        receipt = json.loads((build / "build-receipt.json").read_text())
        if not args.historical_only: require(sha(build / receipt["target"]) == receipt["program_sha256"], "Qt built program changed")
        require(not receipt["system_qt_instrumented"], "Wrong Qt instrumentation claim")
        for name, digest in json.loads((build / "generated-sha256.json").read_text()).items():
            require(sha(build / name) == digest, "Qt generated input changed")
        target = receipt["target"]
        if target in {"native-gui-tests", "ecc-system-client-tests", "getdaq-tests"}:
            expected = {"native-gui-tests": 9, "ecc-system-client-tests": 15, "getdaq-tests": 53}[target]
            result_name = "results-native-gui-full-qt-final.txt" if target == "getdaq-tests" else "results-native-gui-final.txt"
            require(re.search(r"Totals: " + str(expected) + r" passed, 0 failed, 0 skipped", (build / result_name).read_text()), "Qt final case coverage differs")
            if receipt["sanitized"]:
                errors = (build / "results-native-gui-final-stderr.txt").read_text()
                require(not re.search(r"(?:AddressSanitizer|LeakSanitizer|runtime error:|DEADLYSIGNAL)", errors), "Qt sanitizer runtime error")
    native_commands = json.loads((args.native_sanitizer_build_dir / "commands.json").read_text())
    require(all(all(flag in row["command"] for flag in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined"))
                for row in native_commands if "-c" in row["command"]), "Native strict sanitizer flags missing")
    if not args.historical_only:
        programs = json.loads((args.normal / "program-sha256.json").read_text())
        for name, pattern in (("receiver", r"lib(?:Qt|Ice|yaml|curl)"), ("fixture", r"libQt"), ("qt_probe", r"lib(?:Ice|curl)")):
            libraries = subprocess.check_output(["ldd", programs[name]["path"]], text=True)
            require(not re.search(pattern, libraries), "GUI/raw dependency boundary differs")
    print("PASS", 4, "actual Qt whole-system scenarios;", count, "executed program records; same-instance Run intent/session/EOF/seal/counter bindings and source hashes")
    print("Scope: standalone CoBo, synthetic localhost; no hardware, target platform, power-loss or throughput claim; historical mode", args.historical_only)


if __name__ == "__main__":
    main()
