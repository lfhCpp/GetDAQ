#!/usr/bin/env python3
"""归档远程ECC的实际跨进程证据；排除凭据，保留历史版本。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024*1024), b""):
            value.update(block)
    return value.hexdigest()


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize-dir", type=Path, default=Path("/tmp/getdaq-native-remote-ecc-asan"))
    parser.add_argument("--service-sanitize-dir", type=Path, default=Path("/tmp/getdaq-supervisor-control-asan"))
    parser.add_argument("--receiver-sanitize-dir", type=Path, default=Path("/tmp/getdaq-receiver-supervisor-asan"))
    args = parser.parse_args()
    build = root / "build-ice-compat"
    archive = build / "validation-remote-ecc"
    if archive.exists():
        raise RuntimeError("Archive exists; preserve completed evidence")
    ordinary = build / "results-remote-ecc-all-final.txt"
    sanitized = build / "results-remote-ecc-all-sanitizers-final.txt"
    receipts = {ordinary: "0 tests failed out of 12", sanitized: "PASS 12 native ECC",
                build / "results-remote-ecc-original-reference-final.txt": "48 comparisons",
                build / "results-remote-ecc-native-regressions.txt": "0 tests failed out of 3",
                build / "results-remote-ecc-coordinator-regressions.txt": "0 tests failed out of 3"}
    for path, marker in receipts.items():
        if marker not in path.read_text():
            raise RuntimeError("Final successful receipt missing: " + str(path))
    candidates = [Path(line[len("Evidence: "):]) for line in sanitized.read_text().splitlines() if line.startswith("Evidence: ")]
    if len(candidates) != 1:
        raise RuntimeError("Need exactly one sanitized evidence root")
    results = json.loads((candidates[0] / "results.json").read_text())
    if len(results) != 12 or any(row["exit_code"] != 0 for row in results):
        raise RuntimeError("Sanitized cases are not fully successful")
    compiled = json.loads((args.sanitize_dir / "source-sha256.json").read_text())
    for name, expected in compiled.items():
        if sha(root / name) != expected:
            raise RuntimeError("Source changed after sanitizer build: " + name)
    tracked = {root / name for name in compiled}
    for folder in ("apps/datasupervisor/service", "common/http", "apps/datareceiver", "common/text", "common/storage"):
        for pattern in ("*.cpp", "*.h", "CMakeLists.txt"):
            tracked.update((root / folder).glob(pattern))
    tracked.update(root / name for name in ("apps/ecc/native/CMakeLists.txt", "tests/run_native_ecc_remote_receiver_tests.py",
        "tests/archive_remote_ecc_validation.py", "tests/check_remote_ecc_validation.py", "third_party/dependencies.json", "third_party/README.md",
        "README.md", "docs/REMOTE_ECC_DATA_LIFECYCLE.md", "docs/REMOTE_RECEIVER_CONTROL.md", "docs/RECEIVER_PROCESS_MANAGEMENT.md",
        "docs/NATIVE_ECC_COORDINATOR.md", "docs/ECC_JOURNAL.md", "docs/DATA_SERVICE.md", "docs/ARCHITECTURE.md", "docs/REFACTOR_STATUS.md", "docs/VALIDATION.md"))
    sources = {str(path.relative_to(root)): sha(path) for path in sorted(tracked)}
    tested = {build / "native-ecc-remote-receiver-tests": "ordinary 12 remote cases",
              build / "native-ecc-coordinator-tests": "five existing coordinator regression scenarios across two commands",
              build / "native-ecc-receiver-tests": "existing local receiver normal Ice1.1 regression",
              build / "getdaq-supervisor": "ordinary actual node services in 12 cases",
              root / "build-receiver/getdaq-receiver": "ordinary actual raw receiver",
              args.sanitize_dir / "native-ecc-remote-receiver-tests": "12 sanitized remote cases",
              args.service_sanitize_dir / "getdaq-supervisor": "sanitized actual services; explicit crashes exclude leak exit checks",
              args.receiver_sanitize_dir / "getdaq-receiver": "sanitized actual receivers"}
    binaries = {str(path.resolve()): {"sha256": sha(path), "scope": scope} for path, scope in tested.items()}
    archive.mkdir()
    for name in sources:
        target = archive / "verified-sources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / name, target)
    (archive / "source-sha256.json").write_text(json.dumps(sources, indent=2) + "\n")
    (archive / "validated-binary-sha256.json").write_text(json.dumps(binaries, indent=2) + "\n")
    for name in ("source-sha256.json", "dependency-sha256.json", "commands.json"):
        shutil.copy2(args.sanitize_dir / name, archive / ("native-" + name))
    for path in build.glob("results-remote-ecc*.txt"):
        shutil.copy2(path, archive / path.name)
    for path in candidates[0].glob("*"):
        if path.is_file():
            target = archive / "sanitized-results" / path.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    portable = []
    for kind, folder in (("normal", build), ("sanitized", candidates[0])):
        for scenario in [row["scenario"] for row in results]:
            name = "native-ecc-remote-receiver-" + scenario if kind == "normal" else scenario
            index = folder / (name + "-audits.jsonl")
            context = json.loads(Path(str(index) + ".root.json").read_text())
            if context["scenario"] != scenario or context["real_control_services"] != 2 or context["physical_hosts"] != 1 or context["real_hardware"]:
                raise RuntimeError("Remote case scope differs")
            original = Path(context["root"])
            destination = archive / (kind + "-" + scenario)
            secrets = [path.read_bytes().strip() for path in original.rglob("credential")]
            for path in original.rglob("*"):
                if not path.is_file() or path.is_symlink() or path.name == "credential":
                    continue
                data = path.read_bytes()
                if any(secret and secret in data for secret in secrets):
                    raise RuntimeError("Credential exclusion check failed; contents not shown")
                target = destination / path.relative_to(original)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
            shutil.copy2(index, archive / (kind + "-" + scenario + "-audits.jsonl"))
            shutil.copy2(Path(str(index) + ".root.json"), archive / (kind + "-" + scenario + "-root.json"))
            if scenario in ("normal-0", "normal-1"):
                rows = [json.loads(line) for line in index.read_text().splitlines()]
                if len(rows) != 3:
                    raise RuntimeError("Successful run lacks three actual acquisition phases")
                for row in rows:
                    row["directory"] = str((destination / Path(row["directory"]).relative_to(original)).relative_to(archive))
                    portable.append(row)
    (archive / "acquisition-index.jsonl").write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in portable))
    for label, folder, target in (("service", args.service_sanitize_dir / "service", "getdaq-supervisor"),
                                  ("receiver", args.receiver_sanitize_dir, "getdaq-receiver")):
        for name in ("flags.make", "link.txt"):
            shutil.copy2(folder / "CMakeFiles" / (target + ".dir") / name, archive / (label + "-sanitizer-" + name))
    scope = {"ordinary_cases": 12, "sanitized_cases": 12, "physical_hosts": 1, "real_hardware": False,
             "continuous_throughput_tested": False, "source_snapshots": len(sources), "tested_binaries": len(binaries),
             "portable_acquisition_phases": len(portable), "credentials_excluded": True,
             "sanitizer_scope": "new native sources and real service/receiver; old Ice/GET/bzip/yaml and system Qt libraries not instrumented"}
    (archive / "scope.json").write_text(json.dumps(scope, indent=2) + "\n")
    files = {str(path.relative_to(archive)): sha(path) for path in sorted(archive.rglob("*")) if path.is_file()}
    (archive / "archive-file-sha256.json").write_text(json.dumps(files, indent=2) + "\n")
    print("PASS new remote ECC evidence archive; credentials excluded:", archive)


if __name__ == "__main__":
    main()
