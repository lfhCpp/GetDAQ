#!/usr/bin/env python3
"""Bounded regression of ordinary run and alarm stop alongside receiver failure changes."""
import argparse,json,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["native_build","receiver","data_service","evidence"]:parser.add_argument(name,type=Path)
    parser.add_argument("--scenarios",nargs="+",choices=["normal-0","normal-1","alarm-running-1"],default=["normal-1","alarm-running-1"])
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix="receiver-failure-regression-",dir=args.evidence)).resolve()
    build=args.native_build.resolve();programs={"fixture":build/"ecc-network-service-tests","ecc_service":build/"getdaq-ecc-server",
        "receiver":args.receiver.resolve(),"data_service":args.data_service.resolve()}
    maximum=len(args.scenarios)*2*1024**2;assert maximum<=20*1024**3
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    rows=[];deleted=[];process=None
    try:
        for scenario in args.scenarios:
            case=folder/scenario
            command=[str(programs["fixture"]),str(root/"tests/data/cobo-hardware.json"),str(root/"tests/data/mutant-hardware.json"),
                str(programs["receiver"]),str(programs["data_service"]),str(programs["ecc_service"]),str(case),"--scenario",scenario]
            try:
                with (folder/(scenario+".log")).open("w") as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);code=process.wait(timeout=600)
                rows.append({"scenario":scenario,"command":command,"exit_code":code});assert code==0,(folder/(scenario+".log")).read_text()
            finally:
                stop_owned_group(process);process=None
                for path in case.rglob("*.graw"):deleted.append({"path":str(path.relative_to(folder)),"bytes":path.stat().st_size,"sha256":sha(path)});path.unlink()
    finally:
        stop_owned_group(process)
        for path in folder.rglob("*.graw"):deleted.append({"path":str(path.relative_to(folder)),"bytes":path.stat().st_size,"sha256":sha(path)});path.unlink()
        (folder/"payload-cleanup.json").write_text(json.dumps({"deleted":deleted,"remaining_raw":len(list(folder.rglob("*.graw")))},indent=2)+"\n")
        (folder/"results.json").write_text(json.dumps(rows,indent=2)+"\n")
    assert sum(r["bytes"] for r in deleted)<=maximum and not list(folder.rglob("*.graw"))
    (folder/"result.json").write_text(json.dumps({"schema":1,"passed":True,"cases":len(rows),"maximum_aggregate_test_bytes":maximum,
        "aggregate_limit_bytes":20*1024**3,"raw_deleted":True,"program_sha256":{str(p):sha(p) for p in programs.values()},
        "runner_sha256":sha(Path(__file__)),"real_hardware":False},indent=2)+"\n")
    print("PASS selected ordinary/alarm regression; raw deleted;",folder)

if __name__=="__main__":main()
