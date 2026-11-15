#!/usr/bin/env python3
"""本机真实receiver进程联调；保留独立证据目录，不替代实机/吞吐验收。"""
import argparse
import concurrent.futures
import json
from pathlib import Path
import subprocess
import tempfile
import time

SCENARIOS = ["normal", "empty", "truncated", "signal-no-eof", "timeout", "partial-start",
             "abort-connected", "identity", "file-set", "storage", "journal", "lease", "reopen", "validation", "disk-reserve", "catalog",
             "catalog-replace", "observe-clean", "observe-error", "observe-signal", "observe-lost", "observe-byte-storage"]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("receiver", type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be 1..8")
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="receiver-process-", dir=str(parent)))
    def run(scenario):
        command = [str(args.fixture.resolve()), str(args.receiver.resolve()), str(output / scenario), scenario]
        start = time.monotonic()
        with (output / (scenario + ".log")).open("w") as log:
            try:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=45)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        return {"scenario": scenario, "command": command, "exit_code": code, "seconds": round(time.monotonic()-start, 3)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(run, SCENARIOS))
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result["scenario"], "exit", result["exit_code"], "seconds", result["seconds"])
    print("Evidence:", output)
    if any(result["exit_code"] != 0 for result in results):
        raise SystemExit(1)
    print("PASS", len(results), "actual process lifecycle scenarios; catalog metadata and same-inode links, synthetic localhost frames, no hardware/throughput claim")

if __name__ == "__main__":
    main()
