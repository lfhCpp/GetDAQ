#!/usr/bin/env python3
"""只读核对16个实际告警策略场景：一次意图/停止、保持门槛、原连接和重开不重放。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
from check_preparation_stop_validation import require, sha, rows

SCENARIOS = {"alarm-running-0", "alarm-running-1", "alarm-paused", "alarm-stop-loss", "alarm-ready", "alarm-disabled", "alarm-restart", "alarm-during-stop"}


def check_run(root, snapshots=None):
    results = json.loads((root / "results.json").read_text())
    require(len(results) == 8 and {r["scenario"] for r in results} == SCENARIOS and all(r["exit_code"] == 0 for r in results), "Alarm case coverage/exit differs")
    programs = json.loads((root / "program-sha256.json").read_text())
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service"}, "Alarm program set differs")
    for name, record in programs.items():
        require(sha(snapshots / name if snapshots else Path(record["path"])) == record["sha256"], "Executed program changed: " + name)
    for result in results:
        scene = result["scenario"]
        case = root / scene
        enabled = scene != "alarm-disabled"
        no_action = scene in {"alarm-ready", "alarm-during-stop"}
        failed = scene == "alarm-stop-loss"
        scope = json.loads((case / "scope.json").read_text())
        require(scope["passed"] and scope["real_alarm_policy"] and scope["real_ecc_network_service"] and scope["real_data_services"] == 2 and scope["real_receivers"] and
                scope["standalone_cobo_mode"] and scope["physical_hosts"] == 1 and not any(scope[k] for k in
                ("full_ecc_workflow_tested", "real_qt_runcontrol", "real_data_catalog", "real_hardware", "throughput_verified")), "Alarm scope overclaims")
        identity = json.loads((case / "identity.json").read_text())
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"] and sha(case / "controller/experiment.json") == identity["artifact_sha256"], "Alarm parent binding differs")
        alarm = json.loads((case / "alarm-status.json").read_text())
        require(alarm["enabled"] == enabled and alarm["latched"] == enabled and alarm["instance_id"] == identity["instance_id"] and alarm["snapshot_sha256"] == identity["snapshot_sha256"], "Alarm cache identity/hold differs")
        events = rows(case / "controller/network/operations.jsonl")
        holds = [e for e in events if e["event"] == "alarm-held"]
        accepted = [e for e in events if e["event"] == "accepted"]
        require(len(holds) == int(enabled) and len(accepted) == len({e["operation_id"] for e in accepted}), "Repeated alarm hold/accepted ID")
        requests = {e["operation_id"]: json.loads(e["request"]) for e in accepted}
        counts = Counter(r["command"] for r in requests.values())
        expected = Counter({"describe": 1, "prepare": 1, "configure": 1})
        if scene != "alarm-ready":
            expected.update(start=1, stop=1)
        if scene == "alarm-paused":
            expected.update(pause=1)
        require(counts == expected, "Alarm changed command counts or restarted production")
        for event in accepted:
            request = requests[event["operation_id"]]
            require((case / "controller/network/operations" / event["operation_id"] / "request.json").read_text() == event["request"] and
                    hashlib.sha256(("getdaq.ecc-control-request:1\n" + event["request"]).encode()).hexdigest() == event["request_key"], "Alarm original request/key differs")
            require(request["instance_id"] == identity["instance_id"] and request["snapshot_sha256"] == identity["snapshot_sha256"] and request["client_epoch"] == "a" * 64, "Automatic action changed instance/owner")
        internal = [r for r in requests.values() if r["operation_id"].startswith("internal-alarm-")]
        require(len(internal) == int(enabled and not no_action), "Automatic action queued/repeated outside active Run")
        if enabled:
            hold = holds[0]
            require(hashlib.sha256(("getdaq.ecc-alarm-observation:1\n" + hold["evidence"]).encode()).hexdigest() == hold["evidence_sha256"] and
                    json.loads(hold["evidence"]) == alarm["observation"] and alarm["observation"]["stop_requested"], "Alarm evidence hash/immutable cache differs")
            require(hold["operation_id"] == alarm["operation_id"], "Alarm hold not bound to original action")
            if not no_action:
                operation = json.loads((case / "operation-automatic-stop.json").read_text())
                require(operation["request"]["operation_id"] == alarm["operation_id"] and operation["request"]["command"] == "stop" and
                        operation["phase"] == ("failed" if failed else "complete"), "Automatic stop terminal differs")
            else:
                require(alarm["phase"] == "held-no-run" and not alarm["operation_id"], "Known Ready sent a new Stop")
        final = json.loads((case / "final-status.json").read_text())
        require(final["control_needs_reconciliation"] == enabled and final["controller"]["state"] == (5 if failed else 4) and
                final["controller"]["outcome_unknown"] == failed and not final["operation_active"], "Alarm hold cleared or failed stop fabricated Ready")
        before = json.loads((case / "before-alarm-status.json").read_text())
        require(len(before["data"]["nodes"]) == 2, "Original receiver coverage differs")
        for index, node in enumerate(before["data"]["nodes"]):
            initial = node["lifecycle"]
            require(initial["state"] == "ready" and len(initial["receivers"]) == 1 and initial["receivers"][0]["pid"] > 0, "Actual receiver not started")
            data_events = rows(case / ("node-" + str(index)) / "deployment/control/operations.jsonl")
            commands = Counter(json.loads(e["request"])["command"] for e in data_events if e["event"] == "accepted")
            require(commands["start"] == 1 and commands["finish"] == commands["catalog"] == 0 and set(commands) <= {"start", "validate", "verify-stopped", "verify-ready"}, "Alarm stopped/restarted/cataloged receiver")
            if scene != "alarm-restart":
                last = final["data"]["nodes"][index]["lifecycle"]
                require(last["state"] == "ready" and last["session_id"] == initial["session_id"] and last["receivers"][0]["pid"] == initial["receivers"][0]["pid"] and
                        not last["receivers"][0]["reaped"] and not last["receivers"][0]["seal_verified"], "Alarm altered receiver epoch/claimed EOF")
        if scene == "alarm-restart":
            require(final["instance_id"] != identity["instance_id"] and final["controller"]["needs_reconciliation"], "Reopen lost startup gate/instance boundary")
            require(sum(e["event"] == "opened" for e in events) == 2, "Actual restart not recorded")
        credentials = {p for name in ("credential", "ecc-credential") for p in case.rglob(name)}
        require(not snapshots or not credentials, "Archive contains credentials")
        values = [p.read_bytes().strip() for p in credentials]
        for path in case.rglob("*"):
            if path.is_file() and path not in credentials:
                require(not any(v in path.read_bytes() for v in values), "Evidence exposed credential value")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--historical-only", action="store_true")
    args = parser.parse_args()
    require(not args.historical_only or args.archive, "Historical checking requires archive")
    archive = args.archive.resolve() if args.archive else None
    if archive:
        manifest = json.loads((archive / "files-sha256.json").read_text())
        require(set(manifest) == {str(p.relative_to(archive)) for p in archive.rglob("*") if p.is_file() and p != archive / "files-sha256.json"}, "Archive file set differs")
        for name, digest in manifest.items():
            require(sha(archive / name) == digest, "Archived bytes changed")
    for kind, directory in (("normal", args.normal), ("sanitized", args.sanitized)):
        check_run(directory.resolve(), archive / "programs" / kind if args.historical_only else None)
    root = Path(__file__).resolve().parent.parent
    build = archive / "sanitizer-build" if args.historical_only else args.sanitizer_build_dir.resolve()
    sources = archive / "verified-sources" if args.historical_only else root
    for name, digest in json.loads((build / "source-sha256.json").read_text()).items():
        require(sha(sources / name) == digest, "Verified source changed: " + name)
    recipes = [r if isinstance(r, list) else r["command"] for r in json.loads((build / "commands.json").read_text())]
    compiles = [c for c in recipes if "-c" in c]
    require(compiles and all(all(f in c for f in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined")) for c in compiles), "Strict sanitizer flags missing")
    if not args.historical_only:
        programs = json.loads((args.sanitized / "program-sha256.json").read_text())
        for name in ("fixture", "ecc_service", "receiver", "data_service"):
            linked = subprocess.check_output(["ldd", programs[name]["path"]], text=True)
            require("libasan" in linked, "Executed program lacks ASan")
            if name in {"fixture", "receiver"}:
                require("libQt" not in linked, "Native component acquired Qt")
    print("PASS sixteen actual alarm-policy cases: one bound hold/stop, no retry/takeover, retained receiver lifecycle, failed/reopened gates, source/flags/credentials; historical", args.historical_only)


if __name__ == "__main__":
    main()
