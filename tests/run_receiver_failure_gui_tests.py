#!/usr/bin/env python3
"""Actual Qt RunControl -> ECC -> supervisor -> receiver fault; bounded cleanup."""
import argparse,json,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["native_build","receiver","data_service","qt_probe","evidence"]:parser.add_argument(name,type=Path)
    args=parser.parse_args();source=Path(__file__).resolve().parents[1];build=args.native_build.resolve()
    args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix="receiver-failure-qt-",dir=args.evidence)).resolve()
    programs={"fixture":build/"ecc-network-service-tests","ecc_service":build/"getdaq-ecc-server",
        "receiver":args.receiver.resolve(),"data_service":args.data_service.resolve(),"qt_probe":args.qt_probe.resolve()}
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    rows=[];deleted=[];process=None;maximum=4*1024**2;assert maximum<20*1024**3
    try:
        for scenario in ["qt-receiver-failure-0","qt-receiver-failure-1"]:
            case=folder/scenario;cmd=[str(programs["fixture"]),str(source/"tests/data/cobo-hardware.json"),
                str(source/"tests/data/mutant-hardware.json"),str(programs["receiver"]),str(programs["data_service"]),
                str(programs["ecc_service"]),str(case),"--scenario",scenario,str(programs["qt_probe"])]
            try:
                with (folder/(scenario+".log")).open("w") as log:
                    process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);code=process.wait(timeout=600)
                rows.append({"scenario":scenario,"command":cmd,"exit_code":code});assert code==0,(folder/(scenario+".log")).read_text()
                journal=case/"gui-runs/hardware/NativeIntegration/run_0000/journal.json";record=json.loads(journal.read_text())
                policy=json.loads(record["native_receiver_failure_cache"])
                assert record["phase"]!="Complete" and record["duration_incomplete"] and policy["latched"] and policy["phase"]=="complete"
                assert not (journal.parent/"run.csv").exists()
                assert json.loads((case/"scope.json").read_text())["real_qt_runcontrol"]
            finally:
                stop_owned_group(process);process=None
                for p in case.rglob("*.graw"):deleted.append({"path":str(p.relative_to(folder)),"bytes":p.stat().st_size,"sha256":sha(p)});p.unlink()
    finally:
        stop_owned_group(process)
        for p in folder.rglob("*.graw"):deleted.append({"path":str(p.relative_to(folder)),"bytes":p.stat().st_size,"sha256":sha(p)});p.unlink()
        (folder/"payload-cleanup.json").write_text(json.dumps({"deleted":deleted,"remaining_raw":len(list(folder.rglob("*.graw")))},indent=2)+"\n")
        (folder/"results.json").write_text(json.dumps(rows,indent=2)+"\n")
    assert sum(r["bytes"] for r in deleted)<=maximum and not list(folder.rglob("*.graw"))
    (folder/"result.json").write_text(json.dumps({"schema":1,"passed":True,"cases":2,"maximum_aggregate_test_bytes":maximum,"aggregate_limit_bytes":20*1024**3,
        "raw_deleted":True,"program_sha256":{str(p):sha(p) for p in programs.values()},"runner_sha256":sha(Path(__file__)),
        "real_hardware":False,"target_platforms_verified":False,"system_qt_instrumented":False},indent=2)+"\n")
    print("PASS actual Qt receiver failure stop evidence both encodings; raw deleted;",folder)

if __name__=="__main__":main()
