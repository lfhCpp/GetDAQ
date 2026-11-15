#!/usr/bin/env python3
"""归档已结束的ECC夹具与便携旧函数对照；不修改历史阶段归档或原日志。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize-dir", type=Path, default=Path("/tmp/getdaq-native-ecc-asan"))
    args = parser.parse_args()
    sanitized = args.sanitize_dir.resolve()
    build = root / "build-ice-compat"
    archive = build / "validation-native-ecc-sanitizers"
    if archive.exists():
        raise RuntimeError("Archive already exists; preserve it and inspect before another run")
    ordinary_result = (build / "results-native-ecc-tests-final.txt").read_text()
    sanitizer_result = (build / "results-native-ecc-sanitizers.txt").read_text()
    if "100% tests passed, 0 tests failed out of 9" not in ordinary_result or "PASS all native ECC fixture scenarios" not in sanitizer_result:
        raise RuntimeError("Need completed successful ordinary and sanitized runs before archiving")
    hashes = json.loads((sanitized / "source-sha256.json").read_text())
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    for name, expected in hashes.items():
        if digest(root / name) != expected:
            raise RuntimeError("Source changed after sanitizer build: " + name)
    archive.mkdir()
    for name in ("source-sha256.json", "dependency-sha256.json", "commands.json"):
        shutil.copy2(sanitized / name, archive / name)
    for name in hashes:
        destination = archive / "verified-sources" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / name, destination)
    for pattern in ("*.compile.txt", "*.link.txt", "results-*.txt"):
        for path in sanitized.glob(pattern):
            shutil.copy2(path, archive / path.name)
    for path in build.glob("results-native-ecc-*.txt"):
        shutil.copy2(path, archive / path.name)
    binaries = {name: digest(sanitized / name) for name in ("native-ecc-coordinator-tests", "hardware-system-acquisition-tests")}
    (archive / "validated-binary-sha256.json").write_text(json.dumps(binaries, indent=2) + "\n")
    helpers = [root / "tests/run_native_ecc_sanitizers.py", Path(__file__).resolve(), root / "tests/check_native_ecc_validation.py"]
    (archive / "helper-sha256.json").write_text(json.dumps({str(p.relative_to(root)): digest(p) for p in helpers}, indent=2) + "\n")
    for helper in helpers:
        destination = archive / "verified-sources" / helper.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(helper, destination)
    scenarios = ("normal-0", "normal-1", "fault-start", "fault-seal", "fault-reset", "fault-receiver-start",
                 "fault-stop-receiver", "fault-operation-audit", "storage")
    roots = []
    for kind, folder in (("normal", build), ("sanitized", sanitized)):
        portable = []
        for scenario in scenarios:
            evidence = json.loads((folder / ("native-ecc-" + scenario + "-audits.jsonl.root.json")).read_text())
            if evidence["status"] != "fixture_finished" or not evidence["data_lifecycle_test_double"] or evidence["real_receiver_executed"]:
                raise RuntimeError("Invalid fixture completion/scope evidence")
            original = Path(evidence["root"])
            destination = archive / kind / scenario
            shutil.copytree(original, destination)
            if scenario == "storage":
                shutil.copytree(Path(str(original) + "-late"), archive / kind / "storage-late")
            roots.append({"kind": kind, "scenario": scenario, "original": str(original), "directory": str(destination.relative_to(archive)),
                          "data_lifecycle_test_double": True, "real_receiver_executed": False})
            if scenario.startswith("normal-"):
                for line in (folder / ("native-ecc-" + scenario + "-audits.jsonl")).read_text().splitlines():
                    record = json.loads(line)
                    record["directory"] = str((destination / Path(record["directory"]).relative_to(original)).relative_to(archive))
                    portable.append(record)
        if len(portable) != 12:
            raise RuntimeError("Need twelve completed normal acquisition phases")
        index = archive / (kind + "-acquisition-index.jsonl")
        index.write_text("".join(json.dumps(r) + "\n" for r in portable))
        with (archive / ("results-reference-" + kind + ".txt")).open("w") as log:
            subprocess.run(["python3", str(root / "tests/system_acquisition_original_reference.py"), str(index)], stdout=log, stderr=subprocess.STDOUT, check=True)
    # 既有完整采集回归另有12阶段；复制根/子审计，保留原始记录，只改便携索引引用。
    portable = []
    for i, line in enumerate((sanitized / "system-acquisition-audits.jsonl").read_text().splitlines()):
        record = json.loads(line)
        destination = archive / "existing-acquisition" / ("phase-" + str(i))
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(Path(record["directory"]), destination)
        record["directory"] = str(destination.relative_to(archive))
        portable.append(record)
    if len(portable) != 12:
        raise RuntimeError("Need twelve completed existing acquisition phases")
    index = archive / "existing-acquisition-index.jsonl"
    index.write_text("".join(json.dumps(r) + "\n" for r in portable))
    with (archive / "results-existing-acquisition-reference.txt").open("w") as log:
        subprocess.run(["python3", str(root / "tests/system_acquisition_original_reference.py"), str(index)], stdout=log, stderr=subprocess.STDOUT, check=True)
    (archive / "fixture-roots.json").write_text(json.dumps(roots, indent=2) + "\n")
    print("PASS archived verified sources/headers, tested binary hashes, eighteen fixture roots plus storage replacements, and 36 portable acquisition phases; test data lifecycle is not a real receiver")


if __name__ == "__main__":
    main()
