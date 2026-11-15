#!/usr/bin/env python3
"""只读核对 UNKNOWN/实际 Qt 告警：原失败、唯一停止、原接收会话和未完成 Run。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_preparation_stop_validation import require, sha, rows

SCENARIOS = {"alarm-unknown-prepare-0", "alarm-unknown-prepare-1", "alarm-unknown-start", "qt-alarm-0", "qt-alarm-1"}


def read(path):
    return json.loads(path.read_text())


def check_run(root, snapshots=None):
    results = read(root / "results.json")
    require(len(results) == 5 and {r["scenario"] for r in results} == SCENARIOS and all(r["exit_code"] == 0 for r in results), "Alarm UNKNOWN/Qt coverage or exit differs")
    programs = read(root / "program-sha256.json")
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service", "qt_probe"}, "Executed program set differs")
    for name, record in programs.items():
        require(sha(snapshots / name if snapshots else Path(record["path"])) == record["sha256"], "Executed program changed: " + name)
    for result in results:
        scene = result["scenario"]
        case = root / scene
        qt = scene.startswith("qt-")
        prepare = "prepare" in scene
        scope = read(case / "scope.json")
        require(scope["passed"] and scope["real_ecc_network_service"] and scope["real_alarm_policy"] and scope["real_data_services"] == 2 and
                scope["real_receivers"] == (not prepare) and scope["real_qt_runcontrol"] == qt and scope["standalone_cobo_mode"] and scope["physical_hosts"] == 1 and
                not any(scope[k] for k in ("full_ecc_workflow_tested", "real_data_catalog", "real_hardware", "throughput_verified")), "Alarm scope overclaims")
        identity = read(case / "identity.json")
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"] and sha(case / "controller/experiment.json") == identity["artifact_sha256"], "Parent binding differs")
        final = read(case / "final-status.json")
        require(final["instance_id"] == identity["instance_id"] and final["snapshot_sha256"] == identity["snapshot_sha256"] and final["control_needs_reconciliation"] and
                not final["operation_active"] and final["controller"]["state"] == (2 if prepare else 4) and final["controller"]["outcome_unknown"] == (not qt), "Original UNKNOWN/hold cleared")
        events = rows(case / "controller/network/operations.jsonl")
        require([e["sequence"] for e in events] == list(range(len(events))), "Server journal sequence differs")
        accepted = [e for e in events if e["event"] == "accepted"]
        requests = {e["operation_id"]: json.loads(e["request"]) for e in accepted}
        require(len(requests) == len(accepted), "Accepted ID repeated")
        expected = Counter(describe=1, prepare=1)
        if not prepare:
            expected.update(configure=1, start=1)
        expected.update({"stop" if qt else "stop-production-after-failure": 1})
        require(Counter(r["command"] for r in requests.values()) == expected, "Repeated/new command or unintended Breakup")
        for event in accepted:
            request = requests[event["operation_id"]]
            require(request["instance_id"] == identity["instance_id"] and request["snapshot_sha256"] == identity["snapshot_sha256"] and request["client_epoch"] == "a" * 64,
                    "Original instance/owner differs")
            require((case / "controller/network/operations" / event["operation_id"] / "request.json").read_text() == event["request"] and
                    hashlib.sha256(("getdaq.ecc-control-request:1\n" + event["request"]).encode()).hexdigest() == event["request_key"], "Original request bytes/key differ")
        internal = [r for r in requests.values() if r["operation_id"].startswith("internal-alarm-")]
        holds = [e for e in events if e["event"] == "alarm-held"]
        require(len(internal) == len(holds) == 1, "Alarm stop/hold not unique")
        operation_id = internal[0]["operation_id"]
        hold = holds[0]
        require(hold["operation_id"] == operation_id and hashlib.sha256(("getdaq.ecc-alarm-observation:1\n" + hold["evidence"]).encode()).hexdigest() == hold["evidence_sha256"], "Alarm evidence hash/ID differs")
        terminal = [e for e in events if e["event"] in {"complete", "failed", "unknown"}]
        require(len(terminal) == len(accepted) and Counter(e["operation_id"] for e in terminal) == Counter(requests.keys()), "Missing/repeated terminal")
        stop = next(e for e in terminal if e["operation_id"] == operation_id)
        require(stop["event"] == "complete", "Automatic stop did not complete")
        if qt:
            run = case / "gui-runs/hardware/NativeIntegration/run_0000"
            journal = read(run / "journal.json")
            alarm = json.loads(journal["native_alarm_cache"])
            require(journal["phase"] != "Complete" and not any((run / name).exists() for name in ("run.csv", "receiver-status.json", "data-catalogs.json")) and
                    not (run.parent / "run_0001").exists(), "Alarm fabricated Run completion/number advance/seal")
            require(json.loads(journal["native_identity"]) == identity and Counter(json.loads(s)["command"] for s in journal["native_operations"].values()) == {"start": 1}, "GUI Run intent/identity differs")
            client = rows(case / "gui-client-records/observations.jsonl")
            intents = [json.loads(e["payload"]) for e in client if e["kind"] == "intent"]
            require(Counter(r["command"] for r in intents) == {"describe": 1, "prepare": 1, "configure": 1, "start": 1} and all(requests[r["operation_id"]] == r for r in intents), "GUI sent duplicate/automatic stop")
            require("PASS actual Qt alarm is visible" in (case / "gui-probe.log").read_text(), "Actual Qt alarm path not reached")
        else:
            alarm = read(case / "alarm-status.json")
            original = read(case / "operation-original-failure.json")
            require(original["phase"] == "failed" and original["result"]["outcome_unknown"] and original["request"]["operation_id"] == final["controller"]["operation_id"] == ("prepare" if prepare else "start"), "Original failure replaced by automatic stop")
            parent = rows(case / "controller/operations" / operation_id / "operation.jsonl")
            require([e["phase"] for e in parent] == ["intent", "backend_complete"] and all(e["ecc_unknown_preserved"] and not e["sender_changed"] for e in parent) and
                    parent[0]["retained_object"] == ("preparation" if prepare else "acquisition"), "Negative stop lost original object/context")
        require(alarm["instance_id"] == identity["instance_id"] and alarm["snapshot_sha256"] == identity["snapshot_sha256"] and alarm["latched"] and alarm["enabled"] and
                alarm["phase"] == "complete" and alarm["operation_id"] == operation_id and alarm["observation"] == json.loads(hold["evidence"]) and alarm["observation"]["stop_requested"], "GUI/original held observation differs")
        for index in range(2):
            data = rows(case / ("node-" + str(index)) / "deployment/control/operations.jsonl")
            data_requests = {e["operation_id"]: json.loads(e["request"]) for e in data if e["event"] == "accepted"}
            counts = Counter(r["command"] for r in data_requests.values())
            require(counts["start"] == int(not prepare) and counts["finish"] == counts["catalog"] == 0, "Receiver restarted/finished/cataloged")
            lifecycle = final["data"]["nodes"][index]["lifecycle"]
            if prepare:
                require(lifecycle["state"] == "fresh" and not lifecycle["receivers"], "Prepare failure launched receiver")
            else:
                start_id = next(k for k, r in data_requests.items() if r["command"] == "start")
                started = json.loads(next(e["result"] for e in data if e["event"] == "complete" and e["operation_id"] == start_id))
                require(lifecycle["state"] == started["state"] == "ready" and lifecycle["session_id"] == started["session_id"] and len(lifecycle["receivers"]) == len(started["receivers"]) == 1, "Receiver session changed")
                receiver, initial = lifecycle["receivers"][0], started["receivers"][0]
                require(all(receiver[k] == initial[k] for k in ("pid", "session_id", "output_directory")) and receiver["pid"] > 0 and not receiver["reaped"] and not receiver["seal_verified"], "Alarm ended receiver/claimed seal")
        credentials = {p for name in ("credential", "ecc-credential") for p in case.rglob(name)}
        require(not snapshots or not credentials, "Archive contains credential")
        values = [p.read_bytes().strip() for p in credentials]
        for p in case.rglob("*"):
            if p.is_file() and p not in credentials:
                require(not any(v in p.read_bytes() for v in values), "Credential exposed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--qt-build-dirs", type=Path, nargs="+", default=[])
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--historical-only", action="store_true")
    args = parser.parse_args()
    require(not args.historical_only or args.archive, "Historical mode requires archive")
    archive = args.archive.resolve() if args.archive else None
    if archive:
        manifest = read(archive / "files-sha256.json")
        require(set(manifest) == {str(p.relative_to(archive)) for p in archive.rglob("*") if p.is_file() and p != archive / "files-sha256.json"}, "Archive file set differs")
        for name, digest in manifest.items():
            require(sha(archive / name) == digest, "Archived bytes changed")
    for kind, directory in (("normal", args.normal), ("sanitized", args.sanitized)):
        check_run(directory.resolve(), archive / "programs" / kind if args.historical_only else None)
    sources = archive / "verified-sources" if args.historical_only else Path(__file__).resolve().parent.parent
    native = archive / "sanitizer-build" if args.historical_only else args.sanitizer_build_dir.resolve()
    qt_builds = sorted((archive / "qt-build").iterdir()) if args.historical_only else args.qt_build_dirs
    targets = Counter()
    for build in [native] + qt_builds:
        for name, digest in read(build / "source-sha256.json").items():
            require(sha(sources / name) == digest, "Compiled source changed: " + name)
        if build == native:
            compiles = [e if isinstance(e, list) else e["command"] for e in read(build / "commands.json")]
            require(all(all(f in c for f in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined")) for c in compiles if "-c" in c), "Native strict flags missing")
            continue
        receipt = read(build / "build-receipt.json")
        target = receipt["target"]
        targets[(target, receipt["sanitized"])] += 1
        require(not receipt["system_qt_instrumented"] and sha(build / target) == receipt["program_sha256"] and sha(build / "Makefile") == receipt["makefile_sha256"], "Qt program/recipe/coverage claim differs")
        if receipt["sanitized"]:
            makefile = (build / "Makefile").read_text()
            require(all(flag in makefile for flag in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined")), "Qt strict sanitizer flags missing")
        for name, digest in read(build / "generated-sha256.json").items():
            require(sha(build / name) == digest, "Qt generated source changed")
        if target != "native-gui-probe":
            expected = {"native-gui-tests": 11, "ecc-system-client-tests": 17, "getdaq-tests": 53}[target]
            text = (build / "results-alarm-gui-tests-current.txt").read_text()
            require(re.search(r"Totals: " + str(expected) + r" passed, 0 failed, 0 skipped", text) and not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:|DEADLYSIGNAL", text), "Qt test result differs")
    expected_targets = Counter({(t, s): 1 for t in ("native-gui-probe", "native-gui-tests", "ecc-system-client-tests") for s in (False, True)})
    expected_targets[("getdaq-tests", False)] = 1
    require(targets == expected_targets, "Qt build/test coverage differs")
    if not args.historical_only:
        for kind, directory in (("normal", args.normal), ("sanitized", args.sanitized)):
            for name, record in read(directory / "program-sha256.json").items():
                linked = subprocess.check_output(["ldd", record["path"]], text=True)
                require(("libasan" in linked) == (kind == "sanitized"), "Executed sanitizer coverage differs")
                pattern = {"receiver": r"lib(?:Qt|Ice|yaml|curl)", "fixture": r"libQt", "qt_probe": r"lib(?:Ice|curl)"}.get(name)
                require(not pattern or not re.search(pattern, linked), "Raw/control dependency boundary differs")
    print("PASS ten actual UNKNOWN/Qt alarm cases, seven Qt builds, original hold/stop/Run/receiver bindings, source/flags/credentials; historical", args.historical_only)


if __name__ == "__main__":
    main()
