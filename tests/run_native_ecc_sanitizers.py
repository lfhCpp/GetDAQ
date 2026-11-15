#!/usr/bin/env python3
"""运行本机Ice夹具；数据生命周期仅为明确测试替身，不验证原始receiver。"""
import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess
import time


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/getdaq-native-ecc-asan"))
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--include-acquisition", action="store_true", help="Also run the existing full acquisition fixture")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be 1..8")
    output = args.output_dir.resolve()
    environment = dict(os.environ)
    environment.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    fixtures = [str(root / "tests/data" / n) for n in ("cobo-hardware.json", "mutant-hardware.json")]
    scenarios = ["normal-0", "normal-1", "fault-start", "fault-seal", "fault-reset", "fault-receiver-start",
                 "fault-stop-receiver", "fault-operation-audit", "storage", "fault-prepare-0", "fault-prepare-1", "fault-prepare-stop-loss", "fault-prepare-stop-name",
                 "fault-pre-run-configured-0", "fault-pre-run-configured-1", "fault-pre-run-ready-0", "fault-pre-run-ready-1", "fault-negative-audit", "fault-negative-directory"]
    if args.include_acquisition:
        scenarios.append("acquisition")

    def run(scenario):
        if scenario == "acquisition":
            command = [str(output / "hardware-system-acquisition-tests")] + fixtures + [str(output / "system-acquisition-audits.jsonl")]
        else:
            command = [str(output / "native-ecc-coordinator-tests")] + fixtures
            command += [str(output / ("native-ecc-" + scenario + "-audits.jsonl")), "--scenario", scenario]
        start = time.monotonic()
        with (output / ("results-" + scenario + ".txt")).open("w") as log:
            try:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=900 if scenario == "acquisition" else 600)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        return scenario, code, round(time.monotonic() - start, 2)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(run, scenarios))
    for scenario, code, duration in results:
        print(scenario, "exit", code, "seconds", duration)
    if any(code != 0 for _, code, _ in results):
        raise SystemExit(1)
    print("PASS all native ECC fixture scenarios with ASan/UBSan/LeakSanitizer; no real receiver or hardware proof")


if __name__ == "__main__":
    main()
