#!/usr/bin/env python3
"""归档已经通过的远程接收控制证据；排除凭据，不覆盖以前的归档。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def evidence(log, count):
    candidates = [Path(row[len("Evidence: "):]) for row in log.read_text().splitlines() if row.startswith("Evidence: ")]
    if len(candidates) != 1:
        raise RuntimeError("Need one evidence root: " + str(log))
    results = json.loads((candidates[0] / "results.json").read_text())
    if len(results) != count or any(row["exit_code"] != 0 for row in results):
        raise RuntimeError("Evidence is not fully successful: " + str(log))
    return candidates[0]


def main():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitizer-dir", type=Path, default=Path("/tmp/getdaq-supervisor-control-asan"))
    parser.add_argument("--receiver-sanitizer-dir", type=Path, default=Path("/tmp/getdaq-receiver-supervisor-asan"))
    parser.add_argument("--archive", type=Path, default=project / "build-data-supervisor/validation-supervisor-control-final")
    args = parser.parse_args()
    build, native = project / "build-data-supervisor", project / "build-ice-compat"
    archive = args.archive.resolve()
    if archive.exists():
        raise RuntimeError("Archive already exists; preserve completed evidence")
    normal = evidence(build / "results-remote-control-service-confirmed.txt", 14)
    sanitized = evidence(build / "results-remote-control-service-sanitizers-confirmed.txt", 14)
    processes = evidence(build / "results-remote-control-process-sanitizer-regression.txt", 14)
    receipts = {build / "results-remote-control-cli-ctest-final.txt": "0 tests failed out of 13",
                project / "build-suite-tests/results-remote-control-qt-final.txt": "52 passed, 0 failed",
                native / "results-remote-control-native-regression.txt": "0 tests failed out of 2"}
    for path, marker in receipts.items():
        if marker not in path.read_text():
            raise RuntimeError("Missing successful regression: " + str(path))
    # 记录的实际源码/头文件和开发入口；不把历史归档的输入一致性当成新代码验证。
    tracked = set()
    for folder in ("apps/datasupervisor", "common/experiment", "common/http", "common/text", "common/storage",
                   "apps/datareceiver", "apps/ecc/native", "apps/hardware", "apps/ecc"):
        base = project / folder
        for pattern in ("*.cpp", "*.h", "CMakeLists.txt"):
            tracked.update(base.glob(pattern))
    tracked.update((project / "apps/datasupervisor/service").glob("*"))
    tracked.update(project / name for name in (
        "tests/supervisor_control_test.cpp", "tests/run_supervisor_control_tests.py", "tests/data_supervisor_test.cpp",
        "tests/run_data_supervisor_tests.py", "tests/native_ecc_coordinator_test.cpp", "tests/native_ecc_receiver_test.cpp",
        "tests/system_acquisition_fixture.h", "tests/data/cobo-hardware.json", "tests/data/mutant-hardware.json",
        "tests/test_getdaq.cpp", "tests/tests.pro", "tests/build_native_ecc_sanitizers.py",
        "tests/archive_supervisor_control_validation.py", "tests/check_supervisor_control_validation.py",
        "tools/build-linux.sh", "config/receiver/supervisor-service.example.json", "third_party/dependencies.json",
        "README.md", "docs/REMOTE_RECEIVER_CONTROL.md", "docs/RECEIVER_PROCESS_MANAGEMENT.md",
        "docs/ARCHITECTURE.md", "docs/REFACTOR_STATUS.md", "docs/VALIDATION.md", "docs/DATA_SERVICE.md", "docs/ECC_JOURNAL.md", "third_party/README.md"))
    tracked = sorted(path for path in tracked if path.is_file())
    source_hashes = {str(path.relative_to(project)): digest(path) for path in tracked}
    binary_scopes = {build / "getdaq-supervisor": "11 real service/receiver scenarios",
        build / "getdaq-supervisor-query": "identity/status/operation/submit and scope refusal in normal scenario",
        build / "supervisor-control-tests": "codec/credentials and 8 native HTTP cases",
        build / "data-supervisor-tests": "14 process scenarios within ordinary CTest regression",
        project / "build-receiver/getdaq-receiver": "ordinary actual process/service scenarios",
        project / "build-suite-tests/getdaq-tests": "52 Qt test passes",
        native / "native-ecc-coordinator-tests": "storage regression only in this phase",
        native / "native-ecc-receiver-tests": "normal Ice 1.1 real receiver lifecycle only in this phase",
        args.sanitizer_dir / "getdaq-supervisor": "11 sanitized service scenarios; service crash is deliberate SIGKILL",
        args.sanitizer_dir / "getdaq-supervisor-query": "sanitized identity/status/operation/submit and scope refusal",
        args.sanitizer_dir / "supervisor-control-tests": "sanitized codec/credentials and 8 HTTP cases",
        args.sanitizer_dir / "data-supervisor-tests": "14 sanitized process scenarios",
        args.receiver_sanitizer_dir / "getdaq-receiver": "sanitized actual receiver; previously built, receiver source unchanged"}
    binaries = {str(path.resolve()): {"sha256": digest(path), "verified_scope": scope} for path, scope in binary_scopes.items()}
    archive.mkdir()
    for name in source_hashes:
        target = archive / "verified-sources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(project / name, target)
    (archive / "source-sha256.json").write_text(json.dumps(source_hashes, indent=2) + "\n")
    (archive / "validated-binaries.json").write_text(json.dumps(binaries, indent=2) + "\n")
    # 凭据仅在内存中用于排除秘密。不会复制凭据文件、写其内容或记录它的摘要。
    secrets = []
    for folder in (normal, sanitized):
        secrets.extend(path.read_bytes().strip() for path in folder.rglob("credential"))
    def private(path, relative):
        return path.is_symlink() or path.name in ("credential", "token", "token.old", "link") or "credential-checks" in relative.parts
    for label, folder in (("normal-service", normal), ("sanitized-service", sanitized), ("sanitized-process-regression", processes)):
        for path in folder.rglob("*"):
            relative = path.relative_to(folder)
            if not path.is_file() or private(path, relative):
                continue
            content = path.read_bytes()
            if any(secret and secret in content for secret in secrets):
                raise RuntimeError("Secret exclusion check failed; contents not printed")
            target = archive / label / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    for label, folder, patterns in (("supervisor", build, ("results-remote-control*.txt",)),
                                    ("native", native, ("results-remote-control*.txt",)),
                                    ("qt", project / "build-suite-tests", ("results-remote-control*.txt",))):
        for pattern in patterns:
            for path in folder.glob(pattern):
                shutil.copy2(path, archive / (label + "-" + path.name))
    for label, folder, targets in (("normal", build, ("getdaq-supervisor-protocol", "getdaq-data-supervisor-core", "getdaq-supervisor-query", "supervisor-control-tests", "data-supervisor-tests")),
                                   ("sanitized", args.sanitizer_dir, ("getdaq-supervisor-protocol", "getdaq-data-supervisor-core", "getdaq-supervisor-query", "supervisor-control-tests", "data-supervisor-tests")),
                                   ("receiver-sanitized", args.receiver_sanitizer_dir, ("getdaq-receiver",))):
        for target in targets:
            for name in ("flags.make", "link.txt"):
                path = folder / "CMakeFiles" / (target + ".dir") / name
                shutil.copy2(path, archive / (label + "-" + target + "-" + name))
    for label, folder in (("normal", build), ("sanitized", args.sanitizer_dir)):
        for name in ("flags.make", "link.txt"):
            path = folder / "service/CMakeFiles/getdaq-supervisor.dir" / name
            shutil.copy2(path, archive / (label + "-service-" + name))
    index = native / "native-ecc-receiver-normal-1-audits.jsonl"
    native_receivers = json.loads(Path(str(index) + ".root.json").read_text())
    if native_receivers["scenario"] != "normal-1" or not native_receivers["real_receiver_executed"] or native_receivers["ecc_unknown"]:
        raise RuntimeError("Native real receiver regression scope differs")
    shutil.copytree(Path(native_receivers["root"]), archive / "native-receiver-regression")
    shutil.copy2(index, archive / "native-receiver-regression-index.jsonl")
    shutil.copy2(Path(str(index) + ".root.json"), archive / "native-receiver-regression-root.json")
    summary = {"scope": "localhost actual processes and synthetic raw TCP; not hardware, physical multihost, platform or throughput validation",
               "service_scenarios_each": 11, "transport_cases_each": 8, "ordinary_ctest_registered_and_run": 13,
               "qt_passes": 52, "native_regressions": 2, "sanitized_process_scenarios": 14,
               "credentials_excluded": True, "sources": len(source_hashes), "tested_binaries": len(binaries)}
    (archive / "scope.json").write_text(json.dumps(summary, indent=2) + "\n")
    files = {str(path.relative_to(archive)): digest(path) for path in sorted(archive.rglob("*")) if path.is_file()}
    (archive / "archive-file-sha256.json").write_text(json.dumps(files, indent=2) + "\n")
    print("PASS created immutable evidence archive; credentials excluded:", archive)


if __name__ == "__main__":
    main()
