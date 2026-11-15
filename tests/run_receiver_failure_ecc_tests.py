#!/usr/bin/env python3
"""Bounded sequential receiver-failure ECC tests; always remove owned raw payload."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
from run_receiver_observation_tests import interrupted, sha, stop_owned_group

SCENARIOS = ["receiver-running-0", "receiver-running-1", "receiver-paused", "receiver-ready",
             "receiver-stop-loss", "receiver-disabled", "receiver-normal-exit",
             "receiver-offline", "receiver-restart"]
SCENARIOS += ["broken-cleanup-0","broken-cleanup-1","broken-cleanup-readback","broken-cleanup-readback-again"]
STALE_SCENARIOS = ["receiver-stale-stop-0","receiver-stale-stop-1"]
SCENARIOS += ["receiver-cleanup-0","receiver-cleanup-1","receiver-cleanup-stop-loss","receiver-cleanup-audit"]
ICE_SCENARIOS = ["ice-receiver-running-0","ice-receiver-running-1","ice-receiver-stop-loss","ice-receiver-restart"]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("fixture","receiver","data_service","ecc_service","evidence"):
        parser.add_argument(name,type=Path)
    parser.add_argument("--scenarios",nargs="+",choices=SCENARIOS+ICE_SCENARIOS+STALE_SCENARIOS,default=SCENARIOS)
    args=parser.parse_args()
    if len(args.scenarios)!=len(set(args.scenarios)):
        parser.error("Use distinct scenarios")
    root=Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix="receiver-failure-",dir=args.evidence)).resolve()
    programs={name:getattr(args,name).resolve() for name in ("fixture","receiver","data_service","ecc_service")}
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    # Two RawSender fixtures each emit two 4096-byte frames. Include wire and disk
    # copies conservatively at 2 MiB per case; never synthesize a size-scaled payload.
    maximum=len(args.scenarios)*2*1024**2
    assert maximum<=20*1024**3
    deleted=[];results=[];process=None
    (folder/"program-sha256.json").write_text(json.dumps({k:{"path":str(p),"sha256":sha(p)} for k,p in programs.items()},indent=2)+"\n")
    try:
        for scenario in args.scenarios:
            case=folder/scenario
            command=[str(programs["fixture"]),str(root/"tests/data/cobo-hardware.json"),str(root/"tests/data/mutant-hardware.json"),
                     str(programs["receiver"]),str(programs["data_service"]),str(programs["ecc_service"]),str(case),"--scenario",scenario]
            try:
                with (folder/(scenario+".log")).open("w") as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=process.wait(timeout=600)
                results.append({"scenario":scenario,"command":command,"exit_code":code})
                assert code==0,(folder/(scenario+".log")).read_text()
                assert json.loads((case/"scope.json").read_text())["passed"]
                if scenario in STALE_SCENARIOS:
                    proof=json.loads((case/"stale-receiver-proof.json").read_text())
                    assert proof["passed"] and proof["original_http_client_still_pending"] and proof["controller_operation_changed"]
                    assert proof["stale_hold_events"]==0 and proof["only_manual_negative_request"] and proof["status_responsive"]
                if scenario in ICE_SCENARIOS:
                    proof=json.loads((case/"ice-receiver-failure-proof.json").read_text())
                    assert proof["passed"] and proof["source_workers_stopped"] and proof["device_stop_acknowledged"]
                    assert not proof["drain_requested"] and not proof["end_requested"] and not proof["receiver_cleanup_requested"]
                    observed=json.loads((case/"after-receiver-failure-sources.json").read_text())
                    assert len(observed["sources"])==2
                    assert all(s["state"]=="unknown" and s["failure_stop_acknowledged"] and not s["source_end_acknowledged"] for s in observed["sources"])
                    assert not list(case.rglob(".getdaq-ice-receiver-seal.json"))
            finally:
                stop_owned_group(process);process=None
                for path in case.rglob("*.graw"):
                    deleted.append({"path":str(path.relative_to(folder)),"bytes":path.stat().st_size,"sha256":sha(path)})
                    path.unlink()
    finally:
        stop_owned_group(process)
        for path in folder.rglob("*.graw"):
            deleted.append({"path":str(path.relative_to(folder)),"bytes":path.stat().st_size,"sha256":sha(path)})
            path.unlink()
        (folder/"payload-cleanup.json").write_text(json.dumps({"deleted":deleted,"remaining_raw":len(list(folder.rglob("*.graw")))},indent=2)+"\n")
        (folder/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    assert sum(row["bytes"] for row in deleted)<=maximum
    assert not list(folder.rglob("*.graw"))
    (folder/"scope.json").write_text(json.dumps({"schema":1,"passed":True,"cases":len(results),"maximum_aggregate_test_bytes":maximum,
        "aggregate_limit_bytes":20*1024**3,"raw_deleted":True,"runner_sha256":sha(Path(__file__)),"real_hardware":False,
        "target_platforms_verified":False,"throughput_benchmark":False},indent=2)+"\n")
    print("PASS",len(results),"receiver failure ECC cases; raw payload removed;",folder)

if __name__=="__main__":
    main()
