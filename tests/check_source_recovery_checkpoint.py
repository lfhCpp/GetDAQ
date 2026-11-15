#!/usr/bin/env python3
"""Recheck the recorded phase 70.49 source, program, evidence and package digests."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(root, values):
    for name, expected in values.items():
        path = root / name
        if not path.is_file() or digest(path) != expected:
            raise RuntimeError("Missing or changed recorded file: " + str(path))
    return len(values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    record = json.loads(args.checkpoint.read_text())
    if record["schema"] != 1 or record["goal_complete"] or record["current_scenarios"] != 22:
        raise RuntimeError("Unexpected phase checkpoint or unsupported completion claim")
    assertions = 0
    for name in ["programs", "evidence", "validation_inputs", "documentation"]:
        assertions += check(root, record[name])
    for path, receipt in record["production_inputs"].items():
        assertions += check(root, {path: receipt["sha256"]})
        assertions += check(root, receipt["inputs"])
    for package, entry in record["packages"].items():
        package_root = root / package
        assertions += check(package_root, {"manifest.json": entry["manifest_sha256"]})
        manifest = json.loads((package_root / "manifest.json").read_text())
        if manifest["files"] != entry["files"]:
            raise RuntimeError("Package file list differs: " + package)
        assertions += check(package_root, entry["files"])
    phase = args.checkpoint.resolve().parent
    if list(phase.rglob("*.graw")):
        raise RuntimeError("Owned test raw files remain under phase directory")
    print("PASS", assertions, "recorded digest checks; owned raw zero; full project remains incomplete")


if __name__ == "__main__":
    main()
