#!/usr/bin/env python3
"""独立复核 Prepare 故障停止的实际网络请求、原 UNKNOWN、子阶段和未启动接收器。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


SCENARIOS = {"prepare-failure-0", "prepare-failure-1", "prepare-stop-loss"}
STAGES = {(node, stage) for node in ("C0", "C1") for stage in
          ("cobo-stop-pulser", "cobo-stop-firmware", "cobo-stop-interrupts")}


def check_run(root, snapshots=None):
    results = json.loads((root / "results.json").read_text())
    require(len(results) == 3 and {r["scenario"] for r in results} == SCENARIOS and
            all(r["exit_code"] == 0 for r in results), "Preparation case set/exit differs")
    programs = json.loads((root / "program-sha256.json").read_text())
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service"}, "Program set differs")
    for name, record in programs.items():
        require(sha(snapshots / name if snapshots else Path(record["path"])) == record["sha256"], "Executed program changed: " + name)
    for result in results:
        case = root / result["scenario"]
        loss = result["scenario"] == "prepare-stop-loss"
        scope = json.loads((case / "scope.json").read_text())
        require(scope["passed"] and scope["real_ecc_network_service"] and scope["real_data_services"] == 2 and
                scope["physical_hosts"] == 1 and scope["standalone_cobo_mode"] and not any(scope[k] for k in
                ("real_receivers", "full_ecc_workflow_tested", "real_qt_runcontrol", "real_data_catalog", "real_hardware", "throughput_verified")), "Scope overclaims")
        identity = json.loads((case / "identity.json").read_text())
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"], "Snapshot differs")
        require(sha(case / "controller/experiment.json") == identity["artifact_sha256"], "Artifact differs")
        events = rows(case / "controller/network/operations.jsonl")
        accepted = [e for e in events if e["event"] == "accepted"]
        require(len(accepted) == 4 and len({e["operation_id"] for e in accepted}) == 4, "Accepted ID replayed")
        requests = {e["operation_id"]: json.loads(e["request"]) for e in accepted}
        require(Counter(r["command"] for r in requests.values()) == {"describe": 1, "prepare": 1, "stop-production-after-failure": 2}, "Unexpected command accepted")
        for event in accepted:
            request = requests[event["operation_id"]]
            require((case / "controller/network/operations" / event["operation_id"] / "request.json").read_text() == event["request"] and
                    hashlib.sha256(("getdaq.ecc-control-request:1\n" + event["request"]).encode()).hexdigest() == event["request_key"], "Request intent/key differs")
            require(request["instance_id"] == identity["instance_id"] and request["snapshot_sha256"] == identity["snapshot_sha256"], "Request parent binding differs")
        parent = rows(case / "controller/ecc.jsonl")
        require([r["id"] for r in parent if r["phase"] == "intent"] == ["describe", "prepare"] and
                parent[-1]["phase"] == "unknown" and parent[-1]["id"] == "prepare", "Negative stop committed/replayed ECC state")
        for index, operation in enumerate(("negative-prepare", "negative-prepare-again")):
            directory = case / "controller/operations" / operation
            audit = rows(directory / "operation.jsonl")
            require(audit[0]["failed_operation_id"] == "prepare" and audit[0]["retained_object"] == "preparation" and
                    audit[0]["ecc_unknown_preserved"] and not audit[0]["sender_changed"] and
                    audit[0]["snapshot_sha256"] == identity["snapshot_sha256"], "Negative audit binding differs")
            require(audit[-1]["phase"] == ("failed" if loss else "backend_complete"), "Negative terminal audit differs")
            child = rows(directory / "stop-production/system-release.jsonl")
            require(child[1]["phase"] == "negative_stop_context" and child[1]["parent_ecc_unknown_confirmed"] and
                    child[1]["previous_preparation_state"] == 5, "Retained preparation Failed was lost")
            require(child[-1]["phase"] == ("failed" if loss else "complete"), "Child stop outcome differs")
            observed = [r for r in child if r["phase"] == ("step_begin" if index == 0 else "stop_not_repeated")]
            require(len(observed) == 6 and {(r["id"], r["stage"]) for r in observed} == STAGES, "Independent stop coverage differs")
            if index:
                require(not any(r["phase"] == "step_begin" for r in child) and not list((directory / "stop-production").glob("step-*.jsonl")), "New ID repeated hardware stage")
                require(all(r["previously_confirmed"] == (not loss or (r["id"], r["stage"]) != ("C0", "cobo-stop-firmware")) for r in observed), "Restoration cleared unknown stage")
            else:
                require(sum(r["phase"] == "step_complete" for r in child) == (5 if loss else 6), "Independent stage confirmations differ")
            network = json.loads((case / ("operation-" + operation + ".json")).read_text())
            require(network["phase"] == ("failed" if loss else "complete") and network["request"]["operation_id"] == operation, "Network terminal differs")
        final = json.loads((case / "final-status.json").read_text())
        require(final["controller"]["state"] == 2 and final["controller"]["outcome_unknown"] and
                final["controller"]["operation_id"] == "prepare" and final["control_needs_reconciliation"] and not final["operation_active"], "Final gate cleared")
        require(len(final["data"]["nodes"]) == 2, "Data node coverage differs")
        for i, node in enumerate(final["data"]["nodes"]):
            require(node["lifecycle"]["state"] == "fresh" and not node["lifecycle"]["receivers"], "Receiver unexpectedly started")
            data_events = rows(case / ("node-" + str(i)) / "deployment/control/operations.jsonl")
            require(all(json.loads(e["request"])["command"] in {"validate", "verify-stopped"} for e in data_events if e["event"] == "accepted"), "Preparation stop mutated data service")
        credential_files = {p for name in ("credential", "ecc-credential") for p in case.rglob(name)}
        require(not snapshots or not credential_files, "Archive contains credential file")
        values = [p.read_bytes().strip() for p in credential_files]
        for path in case.rglob("*"):
            if path.is_file() and path not in credential_files:
                require(not any(v in path.read_bytes() for v in values), "Evidence contains credential value")


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
    source_root = archive / "verified-sources" if args.historical_only else root
    for name, digest in json.loads((build / "source-sha256.json").read_text()).items():
        require(sha(source_root / name) == digest, "Instrumented source changed: " + name)
    recipes = [r if isinstance(r, list) else r["command"] for r in json.loads((build / "commands.json").read_text())]
    compiles = [c for c in recipes if "-c" in c]
    require(compiles and all(all(flag in c for flag in ("-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined")) for c in compiles), "Strict sanitizer compile flags missing")
    if not args.historical_only:
        for name, digest in json.loads((build / "dependency-sha256.json").read_text()).items():
            require(sha(root / name) == digest, "Old dependency changed since build")
        for directory in (args.normal, args.sanitized):
            programs = json.loads((directory / "program-sha256.json").read_text())
            for name in ("fixture", "ecc_service"):
                linked = subprocess.check_output(["ldd", programs[name]["path"]], text=True)
                if name == "fixture":
                    require("libQt" not in linked, "Native fixture acquired Qt dependency")
        for name in ("fixture", "ecc_service"):
            record = json.loads((args.sanitized / "program-sha256.json").read_text())[name]
            require("libasan" in subprocess.check_output(["ldd", record["path"]], text=True), "Executed sanitizer program lacks ASan")
    print("PASS six actual preparation-failure ECC cases: original requests, independent stops, no new-ID replay, UNKNOWN retained, no receiver launch, source/flags/credentials; historical", args.historical_only)


if __name__ == "__main__":
    main()
