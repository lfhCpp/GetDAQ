#!/usr/bin/env python3
"""归档已验证ECC网络证据；排除认证文件/值，不改旧归档，不提交或推送。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_file(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--receipt-pattern", default="results-ecc-*.txt", help="Receipt basename pattern for this phase")
    parser.add_argument("--acquisition-index", type=Path, help="Explicit combined reference index for this phase")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    archive, build = args.output.resolve(), args.sanitizer_build_dir.resolve()
    if archive.exists(): raise RuntimeError("Archive already exists; preserve it")
    subprocess.run(["python3", str(root / "tests/check_ecc_network_validation.py"), str(args.normal.resolve()), str(args.sanitized.resolve()), "--sanitizer-build-dir", str(build)], check=True)
    sources = json.loads((build / "source-sha256.json").read_text())
    tokens = []
    for evidence in (args.normal.resolve(), args.sanitized.resolve()):
        for name in ("credential", "ecc-credential"):
            tokens.extend(path.read_bytes().strip() for path in evidence.rglob(name))
    archive.mkdir()
    for kind, evidence in (("normal", args.normal.resolve()), ("sanitized", args.sanitized.resolve())):
        shutil.copytree(evidence, archive / kind, ignore=shutil.ignore_patterns("credential", "ecc-credential"))
        for name, program in json.loads((evidence / "program-sha256.json").read_text()).items():
            path = Path(program["path"])
            if sha(path) != program["sha256"]: raise RuntimeError("Tested program changed during archival")
            copy_file(path, archive / "programs" / kind / name)
    for name, value in sources.items():
        if sha(root / name) != value: raise RuntimeError("Compiled source changed during archival: " + name)
        copy_file(root / name, archive / "verified-sources" / name)
    extra = [root / "README.md", root / "third_party/dependencies.json", root / "tests/check_ecc_network_validation.py", Path(__file__).resolve(),
             root / "tests/run_ecc_network_service_tests.py", root / "tests/run_ecc_service_smoke.py", root / "apps/ecc/native/CMakeLists.txt", root / "apps/ecc/service/CMakeLists.txt",
             root / "config/ecc/service.example.json"] + list((root / "docs").glob("*.md"))
    extra += [root / "common/control/CMakeLists.txt", root / "common/control/control.pri", root / "apps/envoy/core.pri", root / "tests/ecc_protocol_test.cpp"]
    for pattern in ("eccsystemclient.*", "httpclient.*"):
        extra += list((root / "apps/envoy").glob(pattern))
    for name in ("ecc_system_client.pro", "test_ecc_system_client.cpp", "ecc_system_client_probe.pro", "ecc_system_client_probe.cpp", "run_ecc_gui_probe.py"):
        extra.append(root / "tests" / name)
    for path in extra: copy_file(path, archive / "verified-sources" / path.relative_to(root))
    for name in ("source-sha256.json", "dependency-sha256.json", "generated-sha256.json", "commands.json"):
        copy_file(build / name, archive / "sanitizer-build" / name)
    for name in json.loads((build / "generated-sha256.json").read_text()): copy_file(Path(name), archive / "generated" / Path(name).name)
    for path in (root / "build-ice-compat").glob(args.receipt_pattern): copy_file(path, archive / "receipts" / path.name)
    reference = args.acquisition_index.resolve() if args.acquisition_index else root / "build-ice-compat/ecc-network-acquisition-index-final.jsonl"
    copy_file(reference, archive / "receipts" / reference.name)
    portable = []
    for kind in ("normal", "sanitized"):
        for scenario in ("normal-0", "normal-1"):
            for row in (archive / kind / scenario / "acquisition-index.jsonl").read_text().splitlines():
                record = json.loads(row)
                old = (args.normal if kind == "normal" else args.sanitized).resolve() / scenario
                record["directory"] = str(Path(kind) / scenario / Path(record["directory"]).relative_to(old))
                portable.append(record)
    (archive / "acquisition-index.jsonl").write_text("".join(json.dumps(row) + "\n" for row in portable))
    all_sources = {str(path.relative_to(archive / "verified-sources")): sha(path) for path in (archive / "verified-sources").rglob("*") if path.is_file()}
    (archive / "all-source-sha256.json").write_text(json.dumps(all_sources, indent=2) + "\n")
    for path in archive.rglob("*"):
        if path.is_file() and any(token in path.read_bytes() for token in tokens):
            raise RuntimeError("Credential value detected; archive incomplete: " + str(path.relative_to(archive)))
    files = {str(path.relative_to(archive)): sha(path) for path in archive.rglob("*") if path.is_file()}
    (archive / "files-sha256.json").write_text(json.dumps(files, indent=2) + "\n")
    print("PASS archived", len(files), "files,", len(all_sources), "source snapshots and 14 actual roots; credentials excluded:", archive)


if __name__ == "__main__":
    main()
