#!/usr/bin/env python3
"""核对运行前UNKNOWN的实际ECC/receiver联调；不将监听/连接当成跑数或封存。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
from check_preparation_stop_validation import require, sha, rows

SCENARIOS = {"pre-run-configured-0", "pre-run-configured-1", "pre-run-ready-0", "pre-run-ready-1"}
STAGES = {(node, stage) for node in ("C0", "C1") for stage in
          ("cobo-stop-pulser", "cobo-stop-firmware", "cobo-stop-interrupts")}


def check_run(root, snapshots=None):
    results = json.loads((root / "results.json").read_text())
    require(len(results) == 4 and {r["scenario"] for r in results} == SCENARIOS and all(r["exit_code"] == 0 for r in results), "Pre-run case set/exit differs")
    programs = json.loads((root / "program-sha256.json").read_text())
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service"}, "Program set differs")
    for name, record in programs.items():
        require(sha(snapshots / name if snapshots else Path(record["path"])) == record["sha256"], "Executed program changed: " + name)
    for result in results:
        case = root / result["scenario"]
        configured = "configured" in result["scenario"]
        scope = json.loads((case / "scope.json").read_text())
        require(scope["passed"] and scope["real_ecc_network_service"] and scope["real_data_services"] == 2 and scope["real_receivers"] and
                scope["physical_hosts"] == 1 and scope["standalone_cobo_mode"] and not any(scope[k] for k in
                ("full_ecc_workflow_tested", "real_qt_runcontrol", "real_data_catalog", "real_hardware", "throughput_verified")), "Pre-run scope overclaims")
        identity = json.loads((case / "identity.json").read_text())
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"] and
                sha(case / "controller/experiment.json") == identity["artifact_sha256"], "Fixed parent source binding differs")
        events = rows(case / "controller/network/operations.jsonl")
        accepted = [e for e in events if e["event"] == "accepted"]
        require(len(accepted) == 5 and len({e["operation_id"] for e in accepted}) == 5, "Original request replayed or positive command accepted")
        requests = {e["operation_id"]: json.loads(e["request"]) for e in accepted}
        require(Counter(r["command"] for r in requests.values()) == {"describe": 1, "prepare": 1, "configure": 1, "stop-production-after-failure": 2}, "Command counts differ")
        for event in accepted:
            require((case / "controller/network/operations" / event["operation_id"] / "request.json").read_text() == event["request"] and
                    hashlib.sha256(("getdaq.ecc-control-request:1\n" + event["request"]).encode()).hexdigest() == event["request_key"], "Request intent/key differs")
            request = requests[event["operation_id"]]
            require(request["instance_id"] == identity["instance_id"] and request["snapshot_sha256"] == identity["snapshot_sha256"], "Request parent binding differs")
        parent = rows(case / "controller/ecc.jsonl")
        require([r["id"] for r in parent if r["phase"] == "intent"] == ["describe", "prepare", "configure"] and
                parent[-1]["phase"] == "unknown" and parent[-1]["id"] == "configure", "Negative stop committed/replayed parent state")
        before = json.loads((case / "before-stop-status.json").read_text())
        final = json.loads((case / "final-status.json").read_text())
        for status in (before, final):
            require(status["controller"]["state"] == 3 and status["controller"]["outcome_unknown"] and status["controller"]["operation_id"] == "configure" and
                    status["control_needs_reconciliation"] and not status["operation_active"] and len(status["data"]["nodes"]) == 2, "Pre-run parent gate cleared")
        for i, (a, b) in enumerate(zip(before["data"]["nodes"], final["data"]["nodes"])):
            first, last = a["lifecycle"], b["lifecycle"]
            require(first["state"] == last["state"] == "ready" and first["session_id"] == last["session_id"] and
                    first["plan_sha256"] == last["plan_sha256"] and len(first["receivers"]) == len(last["receivers"]) == 1, "Receiver epoch/coverage changed")
            old, new = first["receivers"][0], last["receivers"][0]
            require(old["pid"] == new["pid"] and new["pid"] > 0 and old["output_directory"] == new["output_directory"] and
                    not new["reaped"] and not new["seal_verified"] and new["frames"] == "0" and new["received_bytes"] == "0", "Receiver stopped/sealed/ran payload during negative stop")
            data_events = rows(case / ("node-" + str(i)) / "deployment/control/operations.jsonl")
            counts = Counter(json.loads(e["request"])["command"] for e in data_events if e["event"] == "accepted")
            require(counts["start"] == 1 and counts["finish"] == counts["catalog"] == 0 and set(counts) <= {"start", "validate", "verify-stopped", "verify-ready"}, "Data lifecycle mutated/replayed")
        for index, operation in enumerate(("negative-pre-run", "negative-pre-run-again")):
            directory = case / "controller/operations" / operation
            audit = rows(directory / "operation.jsonl")
            require(audit[0]["failed_operation_id"] == "configure" and audit[0]["retained_object"] == "acquisition" and
                    audit[0]["ecc_unknown_preserved"] and not audit[0]["sender_changed"] and audit[-1]["phase"] == "backend_complete", "Stop bypassed original acquisition or parent failure")
            child = rows(directory / "stop-production/system-run.jsonl")
            context = [r for r in child if r["phase"] == "negative_stop_context"]
            expected_state = (0 if configured else 2) if index == 0 else 12
            require(len(context) == 1 and context[0]["parent_ecc_unknown_confirmed"] and context[0]["previous_run_state"] == expected_state,
                    "Pre-run/Failed acquisition stage differs")
            observed = [r for r in child if r["phase"] == ("step_begin" if index == 0 else "stop_not_repeated")]
            require(len(observed) == 6 and {(r["id"], r["stage"]) for r in observed} == STAGES, "Independent producer stop coverage differs")
            if index:
                require(not any(r["phase"] == "step_begin" for r in child) and not list((directory / "stop-production").glob("step-*.jsonl")) and
                        all(r["previously_confirmed"] for r in observed), "New ID repeated or lost confirmed producer stop")
            terminal = json.loads((case / ("operation-" + operation + ".json")).read_text())
            require(terminal["phase"] == "complete" and terminal["request"]["operation_id"] == operation, "Network stop terminal differs")
        credentials = {p for name in ("credential", "ecc-credential") for p in case.rglob(name)}
        require(not snapshots or not credentials, "Archive contains credentials")
        values = [p.read_bytes().strip() for p in credentials]
        for path in case.rglob("*"):
            if path.is_file() and path not in credentials:
                require(not any(v in path.read_bytes() for v in values), "Evidence exposes credential value")


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
        require(sha(sources / name) == digest, "Instrumented source changed: " + name)
    recipes = [r if isinstance(r, list) else r["command"] for r in json.loads((build / "commands.json").read_text())]
    compiles = [c for c in recipes if "-c" in c]
    require(compiles and all(all(f in c for f in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined")) for c in compiles), "Strict sanitizer flags missing")
    if not args.historical_only:
        programs = json.loads((args.sanitized / "program-sha256.json").read_text())
        for name in ("fixture", "ecc_service", "receiver", "data_service"):
            linked = subprocess.check_output(["ldd", programs[name]["path"]], text=True)
            require("libasan" in linked, "Executed sanitizer program lacks ASan")
            if name in ("fixture", "receiver"):
                require("libQt" not in linked, "Native component acquired Qt")
    print("PASS eight actual pre-run ECC cases: retained acquisition/receiver epochs, original requests, independent stops, no replay/Ready/EOF assertion, source/flags/credentials; historical", args.historical_only)


if __name__ == "__main__":
    main()
