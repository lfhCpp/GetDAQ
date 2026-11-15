#!/usr/bin/env python3
"""独立归档整系统GUI证明；拒绝覆盖旧归档，排除凭据，不提交或推送。"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import os
from check_ecc_network_validation import sha, require


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal", type=Path)
    parser.add_argument("sanitized", type=Path)
    parser.add_argument("--qt-build-dirs", type=Path, nargs="+", required=True)
    parser.add_argument("--native-sanitizer-build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--checker",type=Path,default=root/"tests/check_native_gui_validation.py")
    args = parser.parse_args()
    output = args.output.resolve()
    require(not output.exists(), "Archive exists; preserve it")
    command = ["python3", str(args.checker.resolve()), str(args.normal.resolve()), str(args.sanitized.resolve()),
               "--native-sanitizer-build-dir", str(args.native_sanitizer_build_dir.resolve()), "--qt-build-dirs"] + [str(p.resolve()) for p in args.qt_build_dirs]
    subprocess.run(command, check=True)
    tokens = []
    for directory in (args.normal, args.sanitized):
        for pattern in ("credential", "ecc-credential"):
            tokens.extend(path.read_bytes().strip() for path in directory.rglob(pattern))
    output.mkdir()
    for kind, directory in (("normal", args.normal.resolve()), ("sanitized", args.sanitized.resolve())):
        copied_inodes={}
        def evidence_copy(source,destination):
            info=Path(source).stat(); identity=(info.st_dev,info.st_ino)
            if info.st_nlink>1 and identity in copied_inodes:
                os.link(copied_inodes[identity],destination); return destination
            result=shutil.copy2(source,destination)
            if info.st_nlink>1: copied_inodes[identity]=destination
            return result
        shutil.copytree(directory, output / kind, ignore=shutil.ignore_patterns("credential", "ecc-credential"),copy_function=evidence_copy)
        for name, record in json.loads((directory / "program-sha256.json").read_text()).items():
            program = Path(record["path"])
            require(sha(program) == record["sha256"], "Tested program changed during archive")
            copy(program, output / "programs" / kind / name)
    builds = [("native-build", args.native_sanitizer_build_dir.resolve())]
    builds += [("qt-build/" + p.resolve().name, p.resolve()) for p in args.qt_build_dirs]
    for name, build in builds:
        for source, digest in json.loads((build / "source-sha256.json").read_text()).items():
            require(sha(root / source) == digest, "Source changed during archive")
            copy(root / source, output / "verified-sources" / source)
        for pattern in ("*.json", "*.txt", "moc*.cpp", "ui_*.h", "Makefile"):
            for path in build.glob(pattern): copy(path, output / name / path.name)
        if name.startswith("qt-build/"):
            if (build/"build-receipt.json").exists():
                receipt = json.loads((build / "build-receipt.json").read_text())
                copy(build / receipt["target"], output / name / receipt["target"])
            else:
                receipt=json.loads((build/"catalog-build-receipt.json").read_text())
                for target,digest in receipt["programs"].items():
                    require(sha(build/target)==digest,"Catalog CMake program changed during archive")
                    copy(build/target,output/name/target)
    for path in list((root / "docs").glob("*.md")) + [root / "README.md", root / "third_party/dependencies.json", root / "config/envoy.native.example.yml"]:
        copy(path, output / "verified-sources" / path.relative_to(root))
    for name in ("archive_native_gui_validation.py", "check_native_gui_validation.py", "check_catalog_validation.py", "check_ecc_network_validation.py", "run_ecc_network_service_tests.py"):
        copy(root / "tests" / name, output / "verified-sources/tests" / name)
    for path in output.rglob("*"):
        if path.is_file(): require(not any(value in path.read_bytes() for value in tokens), "Credential value copied into archive")
    manifest = {str(p.relative_to(output)): sha(p) for p in output.rglob("*") if p.is_file()}
    (output / "files-sha256.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("PASS archived", len(manifest), "files and actual Qt workflow roots; internal hardlinks preserved, credential files/values excluded:", output)


if __name__ == "__main__":
    main()
