#!/usr/bin/env python3
"""记录同版Qt控制端构建输入；可选严格C++11/ASan/UBSan，不插桩系统Qt。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--sanitized", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("Use 1..8 workers")
    project, build = args.project.resolve(), args.build_dir.resolve()
    project.relative_to(root)
    build.mkdir(parents=True, exist_ok=True)
    commands = [["qmake", str(project), "CONFIG+=strict_c++"]]
    if args.sanitized:
        commands[0] += ["QMAKE_CXXFLAGS_RELEASE=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors",
                        "QMAKE_LFLAGS+=-fsanitize=address,undefined"]
    subprocess.run(commands[0], cwd=build, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    makefile = (build / "Makefile").read_text()
    match = re.search(r"^SOURCES\s*=\s*(.*)$", makefile.replace("\\\n", " "), re.MULTILINE)
    if not match:
        raise RuntimeError("Expected one qmake application SOURCES list")
    sources = []
    for word in shlex.split(match.group(1).replace("\\\n", " ")):
        path = (build / word).resolve()
        if path.is_file() and root in path.parents and build not in path.parents:
            sources.append(path)
    tracked = set(sources + [project, Path(__file__).resolve()])
    for folder in ("apps/envoy", "common/config", "common/experiment", "common/hardware", "common/control", "common/http", "common/storage", "common/text",
                   "third_party/yaml-cpp/include", "third_party/yaml-cpp/src"):
        tracked.update((root / folder).rglob("*.h"))
        tracked.update((root / folder).glob("*.pri"))
    tracked.update((root / "apps/envoy").glob("*.ui"))
    tracked.add(root / "tests/receiver_failure_fixture.h")
    tracked.add(root / "tests/configuration_readback_fixture.h")
    tracked.add(root / "tests/hardware_state_fixture.h")
    before = {str(path.relative_to(root)): sha(path) for path in sorted(tracked)}
    previous = build / "build-receipt.json"
    recipe = sha(build / "Makefile")
    if not previous.exists() or json.loads(previous.read_text()).get("makefile_sha256") != recipe:
        commands.append(["make", "clean"])
        subprocess.run(commands[-1], cwd=build, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    commands.append(["make", "-j" + str(args.jobs)])
    with (build / "results-verified-build.txt").open("w") as log:
        result = subprocess.run(commands[-1], cwd=build, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit("Build failed; inspect " + str(build / "results-verified-build.txt"))
    after = {str(path.relative_to(root)): sha(path) for path in sorted(tracked)}
    if before != after:
        raise RuntimeError("Build inputs changed during compilation; evidence invalid")
    if args.sanitized and ("-std=c++11" not in makefile or "-fsanitize=address,undefined" not in makefile or "-pedantic-errors" not in makefile):
        raise RuntimeError("Strict sanitizer flags missing")
    (build / "source-sha256.json").write_text(json.dumps(after, indent=2) + "\n")
    generated = {path.name: sha(path) for pattern in ("moc_*.cpp", "ui_*.h") for path in build.glob(pattern)}
    (build / "generated-sha256.json").write_text(json.dumps(generated, indent=2) + "\n")
    target = re.search(r"^TARGET\s*=\s*(\S+)", makefile, re.MULTILINE).group(1)
    receipt = {"schema": 1, "project": str(project.relative_to(root)), "target": target, "program_sha256": sha(build / target),
               "sanitized": args.sanitized, "system_qt_instrumented": False, "commands": commands,
               "source_count": len(sources), "tracked_input_count": len(after), "makefile_sha256": recipe,
               "qt_version": subprocess.check_output(["qmake", "-query", "QT_VERSION"], text=True).strip()}
    (build / "build-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("PASS same-source Qt build", target, "sources", len(sources), "tracked inputs", len(after), "sanitized", args.sanitized)


if __name__ == "__main__":
    main()
