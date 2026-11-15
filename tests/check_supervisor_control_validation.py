#!/usr/bin/env python3
"""检查新控制阶段证据；historical-only只检查归档，不宣称当前源码或程序仍相同。"""
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
    parser.add_argument("--archive", type=Path, default=root / "build-data-supervisor/validation-supervisor-control-final")
    parser.add_argument("--historical-only", action="store_true")
    args = parser.parse_args()
    archive = args.archive.resolve()
    index = json.loads((archive / "archive-file-sha256.json").read_text())
    actual = {str(path.relative_to(archive)) for path in archive.rglob("*") if path.is_file() and path.name != "archive-file-sha256.json"}
    require(set(index) == actual, "Archive file set differs")
    for name, expected in index.items():
        path = archive / name
        require(path.is_file() and sha(path) == expected, "Archive digest differs: " + name)
        require(not path.is_symlink(), "Archive contains a symbolic file")
    sources = json.loads((archive / "source-sha256.json").read_text())
    for name, expected in sources.items():
        require(sha(archive / "verified-sources" / name) == expected, "Source snapshot differs: " + name)
        if not args.historical_only:
            require(sha(root / name) == expected, "Current source changed after validation: " + name)
    binaries = json.loads((archive / "validated-binaries.json").read_text())
    if not args.historical_only:
        for name, record in binaries.items():
            require(sha(Path(name)) == record["sha256"], "Current tested binary changed: " + name)
    expected = {"normal", "disconnect", "timeout", "ownership", "journal-fault", "request-fault", "restart", "restart-unknown",
                "credential-fault", "partial-start", "preconditions", "native-transport", "codec", "credential"}
    for folder in ("normal-service", "sanitized-service"):
        results = json.loads((archive / folder / "results.json").read_text())
        require(len(results) == 14 and {row["scenario"] for row in results} == expected and all(row["exit_code"] == 0 for row in results), "Service case coverage differs")
        transport = json.loads((archive / folder / "native-transport/results.json").read_text())
        require(len(transport) == 8 and len({row["case"] for row in transport}) == 8 and all(row["connections"] == 1 and (row["exit_code"] == 0) == row["expected_success"] for row in transport), "Transport coverage or retry policy differs")
        require(not list((archive / folder).rglob("credential")) and not (archive / folder / "credential-checks").exists(), "Credential was archived")
    process = json.loads((archive / "sanitized-process-regression/results.json").read_text())
    require(len(process) == 14 and all(row["exit_code"] == 0 for row in process), "Process regression differs")
    require("-std=c++11" in (archive / "sanitized-service-flags.make").read_text() and "-pedantic-errors" in (archive / "sanitized-service-flags.make").read_text(), "Strict service language flags missing")
    require("-fsanitize=address,undefined" in (archive / "sanitized-service-link.txt").read_text(), "Service sanitizer link missing")
    links = 0
    if not args.historical_only:
        for relative in ("README.md", "REWRITE_PROPOSAL.md", "docs", "third_party/README.md"):
            base = root / relative
            for page in ([base] if base.is_file() else base.rglob("*.md")):
                text = page.read_text()
                for hit in re.finditer(r"\]\(", text):
                    start = hit.end()
                    if text[start:start+1] == "<":
                        end = text.index(">", start+1)
                        target = text[start+1:end]
                    else:
                        depth, end = 1, start
                        while end < len(text) and depth:
                            if text[end] == "(" and text[end-1] != "\\":
                                depth += 1
                            if text[end] == ")" and text[end-1] != "\\":
                                depth -= 1
                            end += 1
                        require(depth == 0, "Incomplete document link")
                        target = text[start:end-1].split(' "', 1)[0]
                    if target.startswith(("https:", "http:", "mailto:", "#", "app:")):
                        continue
                    target = unquote(target.split("#", 1)[0]).replace("\\(", "(").replace("\\)", ")")
                    if not target:
                        continue
                    require((page.parent / target).exists(), "Broken document link: " + str(page) + " -> " + target)
                    links += 1
        dependencies = json.loads((root / "third_party/dependencies.json").read_text())
        require(dependencies["data_supervisor"]["remote_supervisor_service"] and not dependencies["data_supervisor"]["remote_ecc_adapter_implemented"], "Dependency scope differs")
        for dependency in dependencies["dependencies"]:
            if "sha256" in dependency:
                path = root / "third_party" / dependency["local_path"]
                require(path.stat().st_size == dependency["size_bytes"] and sha(path) == dependency["sha256"], "Embedded dependency archive differs")
            licenses = dependency.get("license_files", []) + ([dependency["license_file"]] if "license_file" in dependency else [])
            for name in licenses:
                require((root / "third_party" / name).is_file(), "Dependency license missing")
        require((root / "source/get/GetBench-software-release-20210208/COPYING").read_bytes() == (root / "third_party/licenses/getbench/COPYING").read_bytes(), "Original GET license differs")
        for binary in (root / "build-receiver/getdaq-receiver", root / "build-data-supervisor/getdaq-supervisor-query", Path("/tmp/getdaq-supervisor-control-noqt/getdaq-supervisor-query")):
            result = subprocess.run(["ldd", str(binary)], capture_output=True, text=True, check=True)
            require(not re.search(r"lib(?:Qt|Ice|yaml|curl)", result.stdout), "Native dependency boundary differs")
        tests = subprocess.run(["ctest", "-N"], cwd=root / "build-ice-compat", capture_output=True, text=True, check=True)
        require("Total Tests: 54" in tests.stdout, "Test registration changed after this phase")
    print("PASS", len(index), "archive files;", len(sources), "source snapshots;", len(binaries), "tested binary digests;", links, "document links")
    print("Scope:", "historical archive only" if args.historical_only else "current matched inputs, archive evidence and native dependency boundary")


if __name__ == "__main__":
    main()
