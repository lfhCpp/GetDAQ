#!/usr/bin/env python3
"""只归档已通过的进程管理证据；保留历史归档，不修改原始日志。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b""):
            sha.update(chunk)
    return sha.hexdigest()

def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize-dir", type=Path, default=Path("/tmp/getdaq-native-supervisor-asan"))
    parser.add_argument("--supervisor-sanitize-dir", type=Path, default=Path("/tmp/getdaq-data-supervisor-asan"))
    parser.add_argument("--receiver-sanitize-dir", type=Path, default=Path("/tmp/getdaq-receiver-supervisor-asan"))
    args = parser.parse_args()
    build = root / "build-ice-compat"
    standalone = root / "build-data-supervisor"
    archive = build / "validation-data-supervisor"
    if archive.exists():
        raise RuntimeError("Archive exists; preserve completed evidence")
    checks = {build / "results-data-supervisor-ecc-receiver-final-three.txt": "0 tests failed out of 3",
              build / "results-data-supervisor-ecc-receiver-strengthened.txt": "0 tests failed out of 1",
              build / "results-data-supervisor-ecc-receiver-sanitizers-final.txt": "PASS 4 native ECC",
              build / "results-data-supervisor-ecc-sanitizer-regressions.txt": "PASS all native ECC fixture scenarios",
              standalone / "results-process-lifecycle-sanitizers-final.txt": "PASS 14 actual process",
              standalone / "results-final-tests.txt": "0 tests failed out of 11"}
    for path, marker in checks.items():
        if marker not in path.read_text():
            raise RuntimeError("Missing successful final run: " + str(path))
    compiled = json.loads((args.sanitize_dir / "source-sha256.json").read_text())
    for name, expected in compiled.items():
        if digest(root / name) != expected:
            raise RuntimeError("Input changed after native sanitizer build: " + name)
    tracked = {root / name for name in compiled}
    for folder in ("apps/datareceiver", "apps/datasupervisor", "common/storage", "common/text"):
        tracked.update((root / folder).glob("*.cpp")); tracked.update((root / folder).glob("*.h"))
    tracked.add(root / "apps/datasupervisor/CMakeLists.txt")
    tracked.update(root / name for name in ("tests/data_supervisor_test.cpp", "tests/receiver_core_test.cpp", "tests/run_data_supervisor_tests.py",
        "tests/run_native_ecc_receiver_tests.py", "tests/receiver_integration.py", "tests/archive_data_supervisor_validation.py", "tests/check_data_supervisor_validation.py"))
    hashes = {str(path.relative_to(root)): digest(path) for path in sorted(tracked)}
    archive.mkdir()
    for name in hashes:
        dst = archive / "verified-sources" / name; dst.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(root / name, dst)
    (archive / "source-sha256.json").write_text(json.dumps(hashes, indent=2) + "\n")
    for name in ("source-sha256.json", "dependency-sha256.json", "commands.json"):
        shutil.copy2(args.sanitize_dir / name, archive / ("native-" + name))
    for folder, prefix in ((build, "native"), (standalone, "supervisor")):
        for path in folder.glob("results-*supervisor*.txt"):
            shutil.copy2(path, archive / (prefix + "-" + path.name))
    for name in ("results-process-lifecycle-sanitizers.txt", "results-process-lifecycle-sanitizers-final.txt", "results-process-lifecycle-first.txt", "results-final-tests.txt", "results-existing-receiver-integration.txt", "results-sanitizer-build.txt", "results-receiver-sanitizer-build.txt"):
        shutil.copy2(standalone / name, archive / name)
    binary_paths = [build / "native-ecc-receiver-tests", build / "native-ecc-coordinator-tests", standalone / "data-supervisor-tests", root / "build-receiver/getdaq-receiver",
                    args.sanitize_dir / "native-ecc-receiver-tests", args.sanitize_dir / "native-ecc-coordinator-tests",
                    args.supervisor_sanitize_dir / "data-supervisor-tests", args.receiver_sanitize_dir / "getdaq-receiver"]
    (archive / "validated-binary-sha256.json").write_text(json.dumps({str(p.resolve()): digest(p) for p in binary_paths}, indent=2) + "\n")
    # 接收器/纯管理核心通过独立CMake插桩；保存其真实生成的编译参数。
    for folder, target, prefix in ((args.supervisor_sanitize_dir, "getdaq-supervisor-protocol", "supervisor-protocol"),
                                   (args.supervisor_sanitize_dir, "getdaq-data-supervisor-core", "supervisor"),
                                   (args.receiver_sanitize_dir, "getdaq-receiver", "receiver")):
        shutil.copy2(folder / "CMakeFiles" / (target + ".dir") / "flags.make", archive / (prefix + "-sanitizer-flags.make"))
    def completed_run(log):
        lines = log.read_text().splitlines()
        folders = [Path(line[len("Evidence: "):]) for line in lines if line.startswith("Evidence: ")]
        if len(folders) != 1:
            raise RuntimeError("Need one evidence directory: " + str(log))
        results = json.loads((folders[0] / "results.json").read_text())
        if any(r["exit_code"] != 0 for r in results):
            raise RuntimeError("Evidence contains an unsuccessful scenario")
        return folders[0]
    core_san = completed_run(standalone / "results-process-lifecycle-sanitizers-final.txt")
    shutil.copytree(core_san, archive / "sanitized-processes")
    # 最新一次独立11项CTest包含14组真实进程；用其成功results.json定位，保留全部目录。
    core_normal = sorted((standalone / "data-supervisor-evidence").glob("receiver-process-*/results.json"), key=lambda p:p.stat().st_mtime)[-1].parent
    if any(r["exit_code"] != 0 for r in json.loads((core_normal / "results.json").read_text())):
        raise RuntimeError("Latest ordinary process suite did not pass")
    shutil.copytree(core_normal, archive / "normal-processes")
    native_san = completed_run(build / "results-data-supervisor-ecc-receiver-sanitizers-final.txt")
    shutil.copytree(native_san, archive / "sanitized-native-results")
    roots = []
    portable = []
    for kind, folder in (("normal", build), ("sanitized", native_san)):
        for scenario in ("normal-0", "normal-1", "truncated", "disconnect-ack-loss"):
            base = "native-ecc-receiver-" + scenario if kind == "normal" else scenario
            index = folder / (base + "-audits.jsonl")
            evidence = json.loads(Path(str(index) + ".root.json").read_text())
            if not evidence["real_receiver_executed"] or evidence["data_lifecycle_test_double"] or evidence["ecc_unknown"] != (scenario in ("truncated", "disconnect-ack-loss")):
                raise RuntimeError("Invalid actual receiver scope/outcome record")
            original = Path(evidence["root"]); destination = archive / (kind + "-native") / scenario
            shutil.copytree(original, destination)
            roots.append({"kind": kind, "scenario": scenario, "original": str(original), "directory": str(destination.relative_to(archive))})
            if scenario.startswith("normal-"):
                for line in index.read_text().splitlines():
                    record = json.loads(line); record["directory"] = str((destination / Path(record["directory"]).relative_to(original)).relative_to(archive)); portable.append(record)
    if len(portable) != 12:
        raise RuntimeError("Expected twelve actual normal acquisition phases across both build types")
    (archive / "acquisition-index.jsonl").write_text("".join(json.dumps(record) + "\n" for record in portable))
    (archive / "fixture-roots.json").write_text(json.dumps(roots, indent=2) + "\n")
    # 文件完整性清单只描述归档，不写回原始运行证据。
    files = {str(p.relative_to(archive)): digest(p) for p in sorted(archive.rglob("*")) if p.is_file()}
    (archive / "archive-sha256.json").write_text(json.dumps(files, indent=2) + "\n")
    print("PASS archived", len(hashes), "source/header/helper inputs, 8 tested binary hashes, 28 process scenarios, 8 native receiver roots and 12 portable phases")

if __name__ == "__main__":
    main()
