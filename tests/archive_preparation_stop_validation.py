#!/usr/bin/env python3
"""保存独立 Prepare 故障停止证据；拒绝覆盖，排除认证文件和值，不提交/推送。"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from check_preparation_stop_validation import require, sha


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--checker", type=Path, default=root / "tests/check_preparation_stop_validation.py")
    parser.add_argument("--result-prefix", default="preparation-negative")
    parser.add_argument("--results", nargs="+", help="Explicit result filenames from build-ice-compat")
    parser.add_argument("--qt-build-dirs", type=Path, nargs="+", default=[], help="Optional verified Qt builds for a checker that supports them")
    args = parser.parse_args()
    output = args.output.resolve()
    build = args.sanitizer_build_dir.resolve()
    checker = args.checker.resolve()
    require(args.result_prefix and all(c in "abcdefghijklmnopqrstuvwxyz-" for c in args.result_prefix), "Invalid result prefix")
    require(not output.exists(), "Archive exists; preserve it")
    command = ["python3", str(checker), str(args.normal.resolve()), str(args.sanitized.resolve()),
               "--sanitizer-build-dir", str(build)]
    if args.qt_build_dirs:
        command += ["--qt-build-dirs"] + [str(p.resolve()) for p in args.qt_build_dirs]
    subprocess.run(command, check=True)
    credentials = [p for directory in (args.normal, args.sanitized) for name in
                   ("credential", "ecc-credential") for p in directory.rglob(name)]
    tokens = [p.read_bytes().strip() for p in credentials]
    require(tokens and all(len(t) >= 32 for t in tokens), "Missing/invalid credential scan scope")
    output.mkdir()

    def copy(source, destination):
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    for kind, directory in (("normal", args.normal.resolve()), ("sanitized", args.sanitized.resolve())):
        shutil.copytree(directory, output / kind, ignore=shutil.ignore_patterns("credential", "ecc-credential"))
        for name, record in json.loads((directory / "program-sha256.json").read_text()).items():
            program = Path(record["path"])
            require(sha(program) == record["sha256"], "Executed program changed during archive")
            copy(program, output / "programs" / kind / name)
    for source, digest in json.loads((build / "source-sha256.json").read_text()).items():
        require(sha(root / source) == digest, "Source changed during archive")
        copy(root / source, output / "verified-sources" / source)
    for pattern in ("*.json", "*.txt", "moc*.cpp"):
        for path in build.glob(pattern):
            copy(path, output / "sanitizer-build" / path.name)
    for index, directory in enumerate(args.qt_build_dirs):
        directory = directory.resolve()
        destination = output / "qt-build" / str(index)
        receipt = json.loads((directory / "build-receipt.json").read_text())
        require(Path(receipt["target"]).name == receipt["target"] and sha(directory / receipt["target"]) == receipt["program_sha256"], "Qt program changed during archive")
        copy(directory / receipt["target"], destination / receipt["target"])
        for name, digest in json.loads((directory / "source-sha256.json").read_text()).items():
            require(sha(root / name) == digest, "Qt source changed during archive")
            existing = output / "verified-sources" / name
            require(not existing.exists() or sha(existing) == digest, "Different source versions in archive")
            copy(root / name, existing)
        for pattern in ("*.json", "moc_*.cpp", "ui_*.h", "Makefile", "results-verified-build.txt", "results-" + args.result_prefix + "-*.txt"):
            for path in directory.glob(pattern):
                copy(path, destination / path.name)
    for path in list((root / "docs").glob("*.md")) + [root / "README.md", root / "third_party/dependencies.json"]:
        copy(path, output / "verified-sources" / path.relative_to(root))
    for name in ("check_preparation_stop_validation.py", "check_pre_run_stop_validation.py", "check_alarm_stop_validation.py", "check_alarm_gui_validation.py", "archive_preparation_stop_validation.py", "run_ecc_network_service_tests.py", "run_native_ecc_sanitizers.py"):
        copy(root / "tests" / name, output / "verified-sources/tests" / name)
    copy(checker, output / "verified-sources/tests" / checker.name)
    names = args.results or ["results-" + args.result_prefix + "-" + suffix + ".txt" for suffix in
                            ("build", "network-build", "native-current", "native-regression", "native-sanitizers-current", "network-current", "network-sanitizers-current", "sanitizers-build")]
    for name in names:
        require(Path(name).name == name and name.endswith('.txt'), "Result filename must be a local text receipt")
        copy(root / "build-ice-compat" / name, output / "results" / name)
    for path in output.rglob("*"):
        if path.is_file():
            require(not any(t in path.read_bytes() for t in tokens), "Credential value copied into archive")
    manifest = {str(p.relative_to(output)): sha(p) for p in output.rglob("*") if p.is_file()}
    with (output / "files-sha256.json").open("x") as stream:
        stream.write(json.dumps(manifest, indent=2) + "\n")
    print("PASS archived", len(manifest), args.result_prefix, "evidence files; credentials excluded:", output)


if __name__ == "__main__":
    main()
