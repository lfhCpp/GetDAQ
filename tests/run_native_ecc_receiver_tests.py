#!/usr/bin/env python3
"""运行原生ECC + 真实独立receiver的本机联调，普通/插桩二进制均可。"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

def main():
    root = Path(__file__).resolve().parent.parent
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
    output = Path(tempfile.mkdtemp(prefix="native-receivers-", dir=str(parent)))
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    scenarios = ["normal-0", "normal-1", "truncated", "disconnect-ack-loss"]
    def run(scenario):
        command = [str(args.fixture.resolve()), str(root / "tests/data/cobo-hardware.json"), str(root / "tests/data/mutant-hardware.json"),
                   str(args.receiver.resolve()), str(output / (scenario + "-audits.jsonl")), "--scenario", scenario]
        start = time.monotonic()
        with (output / (scenario + ".log")).open("w") as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=600).returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        return {"scenario": scenario, "command": command, "exit_code": code, "seconds": round(time.monotonic()-start, 3)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(run, scenarios))
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result["scenario"], "exit", result["exit_code"], "seconds", result["seconds"])
    print("Evidence:", output)
    if any(result["exit_code"] != 0 for result in results):
        raise SystemExit(1)
    print("PASS 4 native ECC + actual receiver scenarios; synthetic hardware/frames, no target-platform/throughput claim")

if __name__ == "__main__":
    main()
