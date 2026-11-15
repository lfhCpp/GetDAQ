#!/usr/bin/env python3
"""核对本阶段源码/程序/依赖/文档与便携证据边界；不重跑现场或数据吞吐。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize-dir", type=Path, default=Path("/tmp/getdaq-native-ecc-asan"))
    args = parser.parse_args()
    archive = root / "build-ice-compat/validation-native-ecc-sanitizers"
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    sources = json.loads((archive / "source-sha256.json").read_text())
    for name, expected in sources.items():
        assert digest(root / name) == expected, name
        assert digest(archive / "verified-sources" / name) == expected, name
    for name, expected in json.loads((archive / "validated-binary-sha256.json").read_text()).items():
        assert digest(args.sanitize_dir / name) == expected, name
    for name, expected in json.loads((archive / "dependency-sha256.json").read_text()).items():
        assert digest(root / name) == expected, name
    for name, expected in json.loads((archive / "helper-sha256.json").read_text()).items():
        assert digest(root / name) == expected, name
    print("PASS current and archived verified sources/headers/scripts:", len(sources), "; tested binaries: 2; existing dependency hashes")
    links = 0
    paths = [root / "README.md", root / "REWRITE_PROPOSAL.md", root / "third_party/README.md"] + list((root / "docs").glob("*.md"))
    for path in paths:
        text = path.read_text()
        for hit in re.finditer(r"\]\(", text):
            start = hit.end()
            if text[start:start + 1] == "<":
                end = text.find(">", start + 1)
                assert end != -1, (path, start)
                target = text[start + 1:end]
            else:
                depth, end = 1, start
                while end < len(text) and depth:
                    char = text[end]
                    if char == "(" and (end == 0 or text[end - 1] != "\\"):
                        depth += 1
                    if char == ")" and (end == 0 or text[end - 1] != "\\"):
                        depth -= 1
                    end += 1
                assert depth == 0, (path, start)
                target = text[start:end - 1].split(' "', 1)[0]
            if target.startswith(("http:", "https:", "mailto:", "#", "app:")):
                continue
            target = unquote(target.split("#", 1)[0]).replace("\\(", "(").replace("\\)", ")")
            if target:
                assert (path.parent / target).exists(), (path, target)
                links += 1
    print("PASS local Markdown file links:", links)
    manifest = json.loads((root / "third_party/dependencies.json").read_text())
    archives = licenses = 0
    for dependency in manifest["dependencies"]:
        if "sha256" in dependency:
            path = root / "third_party" / dependency["local_path"]
            assert path.stat().st_size == dependency["size_bytes"] and digest(path) == dependency["sha256"]
            archives += 1
        for name in dependency.get("license_files", []) + ([dependency["license_file"]] if "license_file" in dependency else []):
            assert (root / "third_party" / name).is_file()
            licenses += 1
    assert (root / "source/get/GetBench-software-release-20210208/COPYING").read_bytes() == (root / "third_party/licenses/getbench/COPYING").read_bytes()
    native = manifest["native_ecc_coordinator"]
    assert native["new_third_party_dependencies"] == [] and native["persistent_ecc_state_commit"] is True
    for flag in ("qt_dependency", "raw_data_passes_through_client", "production_network_server", "global_hardware_lock",
                 "receiver_process_management", "real_receiver_validation_by_controller", "actual_loaded_configuration_verified",
                 "unknown_recovery_implemented", "prepare_failure_production_stop_implemented"):
        assert native[flag] is False, flag
    assert manifest["native_data_service"]["third_party_runtime_libraries"] == []
    print("PASS fixed dependency archives/size:", archives, "; license paths:", licenses, "; scope flags")
    for name, banned in (("build-ice-compat/native-ecc-coordinator-tests", ("Qt",)), ("build-receiver/getdaq-receiver", ("Qt", "Ice", "yaml"))):
        linked = subprocess.check_output(["ldd", str(root / name)], text=True)
        assert not any(word in linked for word in banned), (name, linked)
        print("PASS dynamic dependency boundary:", name)
    registered = subprocess.check_output(["ctest", "-N"], cwd=root / "build-ice-compat", text=True)
    assert "Total Tests: 47" in registered
    for kind in ("normal", "sanitized", "existing"):
        index = archive / (kind + "-acquisition-index.jsonl")
        rows = [json.loads(line) for line in index.read_text().splitlines()]
        assert len(rows) == 12
        for row in rows:
            assert (archive / row["directory"] / "system-run.jsonl").is_file()
    for path in archive.glob("*.compile.txt"):
        assert "error:" not in path.read_text() and "warning:" not in path.read_text(), path
    print("PASS 47 registered tests; this phase 9 new + 6 selected regressions, not a single 47-test run; 36 portable phase records")


if __name__ == "__main__":
    main()
