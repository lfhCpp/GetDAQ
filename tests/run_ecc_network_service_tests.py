#!/usr/bin/env python3
"""实际ECC网络进程、两数据服务及原生接收器的跨进程联调；仅合成本机数据。"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import time

SCENARIOS = ["hardware-observation-0", "hardware-observation-1", "normal-0", "normal-1", "start-ack-loss", "truncated", "ecc-restart", "ecc-interrupted", "request-fault"]
HANDOFF_SCENARIOS = ["handoff-service-0", "handoff-service-1"]
TRANSPORT_SCENARIOS=["zbuf-0","zbuf-1","fdt-0","fdt-1","fdt-empty-0","ice-0","ice-1","ice-empty-0","ice-observe-0","ice-observe-1"]
QT_SCENARIOS = ["qt-gui-0", "qt-gui-1", "qt-catalog-0", "qt-catalog-1", "qt-catalog-failure", "qt-catalog-recovery", "qt-alarm-0", "qt-alarm-1", "cobo-maintenance-window-0", "cobo-maintenance-window-1"]
PREPARATION_SCENARIOS = ["prepare-failure-0", "prepare-failure-1", "prepare-stop-loss"]
PRE_RUN_SCENARIOS = ["pre-run-configured-0", "pre-run-configured-1", "pre-run-ready-0", "pre-run-ready-1"]
ALARM_SCENARIOS = ["alarm-running-0", "alarm-running-1", "alarm-paused", "alarm-stop-loss", "alarm-ready", "alarm-disabled", "alarm-restart", "alarm-during-stop"]
ALARM_UNKNOWN_SCENARIOS = ["alarm-unknown-prepare-0", "alarm-unknown-prepare-1", "alarm-unknown-start"]
RETIREMENT_RECOVERY_SCENARIOS = ["plan-transition-complete-0", "plan-transition-complete-1"]
COLD_COMPLETION_SCENARIOS = ["plan-transition-cold-complete-0", "plan-transition-cold-complete-1"]
MAX_SYNTHETIC_RAW_BYTES = 20 * 1024**3


def remove_raw_payloads(directory, root):
    removed_files = 0
    removed_bytes = 0
    errors = []
    if directory.exists():
        for path in directory.rglob("*.graw"):
            try:
                info = path.lstat()
                if not stat.S_ISREG(info.st_mode):
                    errors.append("Refusing non-regular generated raw path: " + str(path))
                    continue
                path.unlink()
                removed_files += 1
                removed_bytes += info.st_size
            except OSError as error:
                errors.append(str(path) + ": " + str(error))
    remaining = []
    if directory.exists():
        remaining = [str(path.relative_to(root)) for path in directory.rglob("*.graw")]
    return {"raw_files_removed": removed_files, "raw_bytes_removed": removed_bytes,
            "raw_remaining_files": remaining, "raw_cleanup_errors": errors}


def main():
    source = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture", "receiver", "data_service", "ecc_service"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--qt-probe", type=Path)
    parser.add_argument("--handoff-tool", type=Path)
    parser.add_argument("--scenarios", nargs="+", choices=SCENARIOS + HANDOFF_SCENARIOS + TRANSPORT_SCENARIOS + QT_SCENARIOS + PREPARATION_SCENARIOS + PRE_RUN_SCENARIOS + ALARM_SCENARIOS + ALARM_UNKNOWN_SCENARIOS + RETIREMENT_RECOVERY_SCENARIOS + COLD_COMPLETION_SCENARIOS, default=SCENARIOS)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8 or len(args.scenarios) != len(set(args.scenarios)):
        parser.error("Use 1..8 workers and distinct scenarios")
    if any(s in QT_SCENARIOS for s in args.scenarios) and args.qt_probe is None:
        parser.error("Qt scenarios require --qt-probe")
    if any(s in COLD_COMPLETION_SCENARIOS for s in args.scenarios) and args.handoff_tool is None:
        parser.error("Cold completion scenarios require --handoff-tool")
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="ecc-network-system-", dir=str(parent)))
    programs = {name: getattr(args, name).resolve() for name in ("fixture", "receiver", "data_service", "ecc_service")}
    if args.handoff_tool is not None:
        programs["handoff_tool"] = args.handoff_tool.resolve()
    if args.qt_probe is not None:
        programs["qt_probe"] = args.qt_probe.resolve()
    (root / "program-sha256.json").write_text(json.dumps({name: {"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for name, p in programs.items()}, indent=2) + "\n")
    if any(s.startswith("cobo-maintenance-window-") for s in args.scenarios):
        guard = root / "deny-raw-payload-reads.so"
        subprocess.run(["cc", "-shared", "-fPIC", str(source / "tests/deny_raw_payload_reads.c"), "-ldl", "-o", str(guard)], check=True)
        programs["raw_read_guard"] = guard
        (root / "program-sha256.json").write_text(json.dumps({name: {"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for name, p in programs.items()}, indent=2) + "\n")
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    def run(scenario):
        command = [str(programs["fixture"]), str(source / "tests/data/cobo-hardware.json"), str(source / "tests/data/mutant-hardware.json"),
                   str(programs["receiver"]), str(programs["data_service"]), str(programs["ecc_service"]), str(root / scenario), "--scenario", scenario]
        if scenario in QT_SCENARIOS:
            command.append(str(programs["qt_probe"]))
        started = time.monotonic()
        run_environment=dict(environment)
        if args.handoff_tool is not None:
            run_environment["GETDAQ_TEST_ECC_HANDOFF_TOOL"] = str(programs["handoff_tool"])
        if scenario.startswith("cobo-maintenance-window-"):
            run_environment["GETDAQ_RAW_READ_GUARD"]=str(programs["raw_read_guard"])
            run_environment["GETDAQ_GUI_RAW_READ_LOG"]=str(root / scenario / "gui-raw-reads.log")
        with (root / (scenario + ".log")).open("w") as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=run_environment, timeout=600).returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
            except OSError as error:
                code = "start_error"
                log.write("\nRunner could not start scenario: " + str(error) + "\n")
        cleanup = remove_raw_payloads(root / scenario, root)
        return {"scenario": scenario, "command": command, "exit_code": code,
                "seconds": round(time.monotonic()-started, 3), **cleanup}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(run, args.scenarios))
    generated_bytes = sum(result["raw_bytes_removed"] for result in results)
    generated_files = sum(result["raw_files_removed"] for result in results)
    remaining = [item for result in results for item in result["raw_remaining_files"]]
    cleanup_errors = [item for result in results for item in result["raw_cleanup_errors"]]
    cleanup = {"limit_bytes": MAX_SYNTHETIC_RAW_BYTES, "generated_raw_files": generated_files,
               "generated_raw_bytes": generated_bytes, "remaining_raw_files": remaining,
               "remaining_raw_bytes": 0 if not remaining else None,
               "limit_exceeded": generated_bytes > MAX_SYNTHETIC_RAW_BYTES,
               "cleanup_errors": cleanup_errors, "scenarios": results}
    (root / "cleanup.json").write_text(json.dumps(cleanup, indent=2) + "\n")
    if cleanup["limit_exceeded"] or remaining or cleanup_errors:
        for result in results:
            result["exit_code"] = 1
    (root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result["scenario"], "exit", result["exit_code"], "seconds", result["seconds"])
    print("Evidence:", root)
    if any(result["exit_code"] != 0 for result in results):
        raise SystemExit(1)
    print("PASS", len(results), "actual ECC network cases; synthetic_raw_removed", generated_files,
          "files/", generated_bytes, "bytes; raw_remaining", len(remaining),
          "; localhost only, no hardware/throughput claim")


if __name__ == "__main__":
    main()
