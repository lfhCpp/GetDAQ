#!/usr/bin/env python3
"""真实ECC库、两个服务及独立receiver的本机跨进程联调，保留证据。"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

SCENARIOS = ["normal-0", "normal-1", "truncated", "start-ack-loss", "finish-ack-loss", "partial-start",
             "service-restart", "service-restart-paused", "data-only", "plan-mismatch", "audit-fault", "ownership"]


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("receiver", type=Path)
    parser.add_argument("service", type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--scenarios", nargs="+", choices=SCENARIOS, default=SCENARIOS)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8 or len(args.scenarios) != len(set(args.scenarios)):
        parser.error("Use 1..8 workers and distinct scenarios")
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="native-remote-receivers-", dir=str(parent)))
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    def run(scenario):
        command = [str(args.fixture.resolve()), str(root / "tests/data/cobo-hardware.json"), str(root / "tests/data/mutant-hardware.json"),
                   str(args.receiver.resolve()), str(args.service.resolve()), str(output / (scenario + "-audits.jsonl")), "--scenario", scenario]
        started = time.monotonic()
        with (output / (scenario + ".log")).open("w") as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=600).returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        return {"scenario": scenario, "command": command, "exit_code": code, "seconds": round(time.monotonic()-started, 3)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(run, args.scenarios))
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result["scenario"], "exit", result["exit_code"], "seconds", result["seconds"])
    print("Evidence:", output)
    if any(result["exit_code"] != 0 for result in results):
        raise SystemExit(1)
    print("PASS", len(results), "native ECC + 2 actual service processes + independent receivers; synthetic localhost, no hardware/throughput claim")


if __name__ == "__main__":
    main()
