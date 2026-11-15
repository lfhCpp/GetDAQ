#!/usr/bin/env python3
"""复核实际ECC网络联调的最终证据、编译输入和依赖；只读，不推断实机性能。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote

SCENARIOS = {"normal-0", "normal-1", "start-ack-loss", "truncated", "ecc-restart", "ecc-interrupted", "request-fault"}
COMPLETE = {"normal-0", "normal-1", "start-ack-loss"}
COUNTS = {"describe": 1, "prepare": 1, "configure": 2, "start": 3, "stop": 3, "pause": 1, "resume": 1, "breakup": 2, "reset": 2}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_run(root, snapshots=None):
    results = json.loads((root / "results.json").read_text())
    require(len(results) == 7 and {r["scenario"] for r in results} == SCENARIOS and all(r["exit_code"] == 0 for r in results), "Final case coverage/exit differs")
    programs = json.loads((root / "program-sha256.json").read_text())
    require(set(programs) == {"fixture", "receiver", "data_service", "ecc_service"}, "Actual executable set differs")
    for name, program in programs.items():
        path = snapshots / name if snapshots else Path(program["path"])
        require(sha(path) == program["sha256"], "Actual tested executable changed")
    for scenario in SCENARIOS:
        case = root / scenario
        scope = json.loads((case / "scope.json").read_text())
        require(scope["passed"] and scope["scenario"] == scenario and scope["real_ecc_network_service"] and scope["real_data_services"] == 2 and
                scope["physical_hosts"] == 1 and scope["standalone_cobo_mode"] and not scope["real_hardware"] and not scope["throughput_verified"], "Case scope differs")
        require(scope["real_receivers"] == (scenario not in {"ecc-interrupted", "request-fault"}) and scope["full_ecc_workflow_tested"] == (scenario in COMPLETE), "Case claims unsupported receiver/full command coverage")
        identity = json.loads((case / "identity.json").read_text())
        snapshot = json.loads((case / "controller/snapshot.json").read_text())
        require(sha(case / "controller/snapshot.json") == identity["snapshot_sha256"] and
                sha(case / "controller/experiment.json") == identity["artifact_sha256"] and
                snapshot["deployment_source_sha256"] == identity["input_binding_sha256"], "Input/snapshot/artifact binding differs")
        events = [json.loads(row) for row in (case / "controller/network/operations.jsonl").read_text().splitlines()]
        require([row["sequence"] for row in events] == list(range(len(events))), "Network journal sequence differs")
        accepted = [row for row in events if row["event"] == "accepted"]
        require(len({row["operation_id"] for row in accepted}) == len(accepted), "Duplicate accepted intent/replay")
        counts = {}
        for row in accepted:
            request = row["request"]
            require(sha(case / ("controller/network/operations/" + row["operation_id"] + "/request.json")) == hashlib.sha256(request.encode()).hexdigest(), "Persisted request differs from accepted intent")
            require(hashlib.sha256(("getdaq.ecc-control-request:1\n" + request).encode()).hexdigest() == row["request_key"], "Accepted request key differs")
            command = json.loads(request)["command"]
            counts[command] = counts.get(command, 0) + 1
        final = json.loads((case / "final-status.json").read_text())
        if scenario in COMPLETE:
            require(counts == COUNTS and final["controller"]["state"] == 1 and not final["controller"]["outcome_unknown"] and not final["control_needs_reconciliation"], "Nine-command/two-cycle evidence differs")
            seals = list((case / "outputs").rglob(".getdaq-receiver-seal.json"))
            require(len(seals) == 4, "Need two receivers and two sealed cycles")
            sizes = []
            for path in seals:
                seal = json.loads(path.read_text())
                require(seal["complete"] and seal["durable"] and seal["sender_eof_confirmed"] and seal["connections"] == 1 and
                        seal["configuration_sha256"] == identity["configuration_sha256"] and seal["artifact_sha256"] == identity["artifact_sha256"], "Receiver seal scope/EOF differs")
                total = 0
                for file in seal["files"]:
                    require(Path(file["name"]).name == file["name"], "Seal file path not a single name")
                    size = (path.parent / file["name"]).stat().st_size
                    require(size == int(file["bytes"]), "Sealed file size differs")
                    total += size
                require(total == int(seal["received_bytes"]), "Sealed byte total differs")
                sizes.append(total)
            require(sorted(sizes) == [8192, 8192, 24576, 24576], "Actual synthetic six/two-frame cycles differ")
        else:
            require(final["control_needs_reconciliation"], "Fault/restart gate cleared")
        if scenario == "ecc-interrupted":
            require(not any(row["event"] == "complete" and row.get("operation_id") == "prepare" for row in events), "Interrupted Prepare fabricated completion")
        credentials = [case / "ecc-credential", case / "node-0/credential", case / "node-1/credential"]
        if snapshots:
            require(not any(path.exists() for path in credentials), "Credential copied into archive")
        values = [path.read_bytes().strip() for path in credentials if path.exists()]
        for path in case.rglob("*"):
            if path.is_file() and path not in credentials:
                content = path.read_bytes()
                require(not any(value in content for value in values), "Credential exposed in public test material: " + str(path.relative_to(case)))
    return len(programs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--archive", type=Path, help="Verify an immutable evidence archive")
    parser.add_argument("--historical-only", action="store_true", help="Use archived source/executable copies instead of requiring unchanged current files")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    require(not args.historical_only or args.archive, "Historical checking requires an archive")
    archive = args.archive.resolve() if args.archive else None
    if archive:
        files = json.loads((archive / "files-sha256.json").read_text())
        actual = {str(path.relative_to(archive)) for path in archive.rglob("*") if path.is_file() and path.name != "files-sha256.json"}
        require(actual == set(files), "Archive file set differs")
        for name, value in files.items(): require(sha(archive / name) == value, "Archive file changed: " + name)
    programs = check_run(args.normal.resolve(), archive / "programs/normal" if args.historical_only else None)
    programs += check_run(args.sanitized.resolve(), archive / "programs/sanitized" if args.historical_only else None)
    build = args.sanitizer_build_dir.resolve()
    sources = json.loads((build / "source-sha256.json").read_text())
    for name, value in sources.items():
        require(sha((archive / "verified-sources" if args.historical_only else root) / name) == value, "Source changed after sanitizer compilation: " + name)
    if not args.historical_only:
        for name, value in json.loads((build / "dependency-sha256.json").read_text()).items():
            require(sha(root / name) == value, "Linked embedded static library changed")
    for name, value in json.loads((build / "generated-sha256.json").read_text()).items():
        require(sha(archive / "generated" / Path(name).name if args.historical_only else Path(name)) == value, "Compiled Qt moc output changed")
    commands = json.loads((build / "commands.json").read_text())
    compiled = [row["command"] for row in commands if "-c" in row["command"]]
    require(compiled and all(all(flag in command for flag in ["-std=c++11", "-pedantic-errors", "-fsanitize=address,undefined"]) for command in compiled), "Strict sanitizer compilation missing")
    require(any(any(word.endswith("/apps/ecc/service/eccservice.cpp") for word in command) for command in compiled), "ECC HTTP frontend was not instrumented")
    if args.historical_only:
        print("PASS historical archive file hashes, 14 actual ECC network cases,", programs, "program snapshots and", len(sources), "compiled source snapshots; no current deployment/platform claim")
        return
    dependencies = json.loads((root / "third_party/dependencies.json").read_text())
    require(dependencies["ecc_network_service"]["fixed_input_binding"] and not dependencies["ecc_network_service"]["raw_data_passes_through_service"] and
            not dependencies["native_ecc_coordinator"]["qt_dependency"], "Dependency/control separation metadata differs")
    for dependency in dependencies["dependencies"]:
        if "sha256" in dependency:
            path = root / "third_party" / dependency["local_path"]
            require(path.stat().st_size == dependency["size_bytes"] and sha(path) == dependency["sha256"], "Embedded dependency changed")
        for name in dependency.get("license_files", []) + ([dependency["license_file"]] if "license_file" in dependency else []):
            require((root / "third_party" / name).is_file(), "License missing")
    for binary, pattern in [(Path(json.loads((args.normal / "program-sha256.json").read_text())["receiver"]["path"]), r"lib(?:Qt|Ice|yaml|curl)"),
                            (Path(json.loads((args.normal / "program-sha256.json").read_text())["fixture"]["path"]), r"libQt")]:
        libraries = subprocess.check_output(["ldd", str(binary)], text=True)
        require(not re.search(pattern, libraries), "Native/raw dependency boundary differs")
    links = 0
    for page in [root / "README.md", root / "REWRITE_PROPOSAL.md", root / "third_party/README.md"] + list((root / "docs").glob("*.md")):
        text = page.read_text()
        for hit in re.finditer(r"\]\(", text):
            start = hit.end()
            if text[start:start+1] == "<":
                target = text[start+1:text.index(">", start+1)]
            else:
                depth, end = 1, start
                while end < len(text) and depth:
                    if text[end] == "(" and text[end-1] != "\\": depth += 1
                    if text[end] == ")" and text[end-1] != "\\": depth -= 1
                    end += 1
                require(depth == 0, "Malformed document link")
                target = text[start:end-1].split(' "', 1)[0]
            if target.startswith(("http:", "https:", "mailto:", "#", "app:")): continue
            target = unquote(target.split("#", 1)[0]).replace("\\(", "(").replace("\\)", ")")
            if target:
                require((page.parent / target).exists(), "Broken document file link: " + str(page) + " -> " + target)
                links += 1
    print("PASS 14 actual ECC network scenarios;", programs, "executed program records;", len(sources), "current sanitizer inputs;", links, "document links; request/snapshot/seal/dependency and credential boundaries")
    print("Scope: synthetic localhost standalone CoBo, no real hardware/target platform/throughput claim; private credentials remain excluded from public material")


if __name__ == "__main__":
    main()
