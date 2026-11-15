#!/usr/bin/env python3
"""Original failed-session retirement; private tiny payloads, always cleaned."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile

from run_receiver_observation_tests import interrupted, stop_owned_group

SCENARIOS = ["retire-normal", "retire-lock", "retire-abort-audit", "retire-proof",
             "abort-failed", "normal", "partial-start", "reopen"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("receiver", type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    args = parser.parse_args()
    args.output_parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="failed-session-retirement-", dir=args.output_parent)).resolve()
    maximum = 32 * 1024**2  # Eight cases, two 12-frame cycles each: <2 MiB.
    assert maximum < 20 * 1024**3
    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    results, process, removed = [], None, []
    try:
        for name in SCENARIOS:
            with (root / (name + ".log")).open("w") as log:
                process = subprocess.Popen([str(args.fixture.resolve()), str(args.receiver.resolve()),
                                            str(root / name), name], stdout=log, stderr=subprocess.STDOUT,
                                           start_new_session=True)
                code = process.wait(timeout=40)
            stop_owned_group(process)
            results.append({"scenario": name, "exit_code": code})
            assert code == 0, (root / (name + ".log")).read_text()[-3000:]
    finally:
        stop_owned_group(process)
        for path in root.rglob("*.graw"):
            if path.is_file() and not path.is_symlink():
                removed.append({"path": str(path.relative_to(root)), "bytes": path.stat().st_size,
                                "sha256": sha(path)})
                path.unlink()
        remaining = [str(p.relative_to(root)) for p in root.rglob("*.graw")]
        (root / "cleanup.json").write_text(json.dumps({"removed": removed, "remaining": remaining}, indent=2) + "\n")
        (root / "result.json").write_text(json.dumps({"passed": len(results) == len(SCENARIOS) and all(r["exit_code"] == 0 for r in results),
            "cases": results, "fixture_sha256": sha(args.fixture.resolve()),
            "receiver_sha256": sha(args.receiver.resolve()), "runner_sha256": sha(Path(__file__)),
            "maximum_aggregate_test_bytes": maximum, "raw_remaining": len(remaining),
            "real_hardware": False, "throughput_benchmark": False}, indent=2) + "\n")
    assert not remaining
    print("PASS", len(results), "original-process retirement/lifecycle cases; owned raw removed;", root)


if __name__ == "__main__":
    main()
