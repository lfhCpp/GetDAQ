#!/usr/bin/env python3
"""核对远程ECC阶段的来源、程序、证据、链接和依赖范围。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=root / "build-ice-compat/validation-remote-ecc")
    parser.add_argument("--historical-only", action="store_true")
    args = parser.parse_args()
    archive = args.archive.resolve()
    files = json.loads((archive / "archive-file-sha256.json").read_text())
    actual = {str(p.relative_to(archive)) for p in archive.rglob("*") if p.is_file() and p.name != "archive-file-sha256.json"}
    require(actual == set(files), "Archive file set differs")
    for name, value in files.items():
        require(not (archive / name).is_symlink() and sha(archive / name) == value, "Archive digest differs: " + name)
    sources = json.loads((archive / "source-sha256.json").read_text())
    for name, value in sources.items():
        require(sha(archive / "verified-sources" / name) == value, "Source snapshot differs: " + name)
        if not args.historical_only:
            require(sha(root / name) == value, "Current source changed after validation: " + name)
    binaries = json.loads((archive / "validated-binary-sha256.json").read_text())
    if not args.historical_only:
        for name, value in binaries.items():
            require(sha(Path(name)) == value["sha256"], "Tested program changed: " + name)
        for name, value in json.loads((archive / "native-dependency-sha256.json").read_text()).items():
            require(sha(root / name) == value, "Existing static dependency changed")
    expected = {"normal-0", "normal-1", "truncated", "start-ack-loss", "finish-ack-loss", "partial-start",
                "service-restart", "service-restart-paused", "data-only", "plan-mismatch", "audit-fault", "ownership"}
    for kind in ("normal", "sanitized"):
        for scenario in expected:
            evidence = json.loads((archive / (kind + "-" + scenario + "-root.json")).read_text())
            require(evidence["scenario"] == scenario and evidence["real_control_services"] == 2 and evidence["physical_hosts"] == 1 and not evidence["real_hardware"], "Case scope differs")
            require((archive / (kind + "-" + scenario) / "controller/snapshot.json").is_file(), "Parent snapshot missing")
            for node in ("node-0", "node-1"):
                require((archive / (kind + "-" + scenario) / node / "deployment/control/operations.jsonl").is_file(), "Actual service journal missing")
            require(not list((archive / (kind + "-" + scenario)).rglob("credential")), "Credential was archived")
    results = json.loads((archive / "sanitized-results/results.json").read_text())
    require(len(results) == 12 and {r["scenario"] for r in results} == expected and all(r["exit_code"] == 0 for r in results), "Sanitized result coverage differs")
    index = [json.loads(row) for row in (archive / "acquisition-index.jsonl").read_text().splitlines()]
    require(len(index) == 12 and all((archive / row["directory"] / "system-run.jsonl").is_file() for row in index), "Portable actual acquisition index differs")
    commands = json.loads((archive / "native-commands.json").read_text())
    require(all("-std=c++11" in row["command"] and "-pedantic-errors" in row["command"] and "-fsanitize=address,undefined" in row["command"] for row in commands), "Strict native sanitizer recipe differs")
    require("-std=c++11" in (archive / "service-sanitizer-flags.make").read_text() and "-fsanitize=address,undefined" in (archive / "service-sanitizer-link.txt").read_text(), "Service sanitizer recipe missing")
    links = 0
    if not args.historical_only:
        for page in [root / "README.md", root / "REWRITE_PROPOSAL.md", root / "third_party/README.md"] + list((root / "docs").glob("*.md")):
            text = page.read_text()
            for hit in re.finditer(r"\]\(", text):
                start = hit.end()
                if text[start:start+1] == "<":
                    target = text[start+1:text.index(">", start+1)]
                else:
                    depth, end = 1, start
                    while end < len(text) and depth:
                        if text[end] == "(" and text[end-1] != "\\":
                            depth += 1
                        if text[end] == ")" and text[end-1] != "\\":
                            depth -= 1
                        end += 1
                    require(depth == 0, "Malformed document link")
                    target = text[start:end-1].split(' "', 1)[0]
                if target.startswith(("http:", "https:", "mailto:", "#", "app:")):
                    continue
                target = unquote(target.split("#", 1)[0]).replace("\\(", "(").replace("\\)", ")")
                if target:
                    require((page.parent / target).exists(), "Broken file link: " + str(page) + " -> " + target)
                    links += 1
        dependencies = json.loads((root / "third_party/dependencies.json").read_text())
        require(dependencies["data_supervisor"]["remote_ecc_adapter_implemented"] and not dependencies["data_supervisor"]["remote_ecc_adapter_qt_dependency"], "Current adapter scope differs")
        for dependency in dependencies["dependencies"]:
            if "sha256" in dependency:
                path = root / "third_party" / dependency["local_path"]
                require(path.stat().st_size == dependency["size_bytes"] and sha(path) == dependency["sha256"], "Embedded archive changed")
            for name in dependency.get("license_files", []) + ([dependency["license_file"]] if "license_file" in dependency else []):
                require((root / "third_party" / name).is_file(), "License missing")
        require((root / "source/get/GetBench-software-release-20210208/COPYING").read_bytes() == (root / "third_party/licenses/getbench/COPYING").read_bytes(), "Original GET license differs")
        for binary, pattern in ((root / "build-ice-compat/native-ecc-remote-receiver-tests", r"libQt"),
                                (root / "build-receiver/getdaq-receiver", r"lib(?:Qt|Ice|yaml|curl)")):
            libraries = subprocess.run(["ldd", str(binary)], capture_output=True, text=True, check=True).stdout
            require(not re.search(pattern, libraries), "Native control/raw receiver dependency boundary differs")
    print("PASS", len(files), "archive file hashes;", len(sources), "sources;", len(binaries), "tested programs; 24 actual roots/12 acquisition phases;", links, "local document links")
    print("Scope:", "historical archive only" if args.historical_only else "current matched inputs and local-process evidence; no physical/hardware/throughput claim")


if __name__ == "__main__":
    main()
