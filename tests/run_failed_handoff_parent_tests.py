#!/usr/bin/env python3
"""Parent observes an operator-completed node handoff without posting a repair."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("fixture","receiver","service","injector","evidence"):parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args();names=("fixture","receiver","service","injector")
    programs={name:getattr(args,name).resolve(strict=True) for name in names};before={name:sha(path) for name,path in programs.items()}
    args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix="failed-handoff-parent-",dir=args.evidence)).resolve()
    repo=Path(__file__).resolve().parents[1];env=dict(os.environ,GETDAQ_TEST_HANDOFF_INJECTOR=str(programs["injector"]))
    for sig in (signal.SIGINT,signal.SIGTERM):signal.signal(sig,interrupted)
    cases=[];deleted=[];process=None
    try:
        for minor in (0,1):
            scenario="handoff-failed-completion-"+str(minor);case=folder/scenario
            command=[str(programs["fixture"]),str(repo/"tests/data/cobo-hardware.json"),str(repo/"tests/data/mutant-hardware.json"),
                str(programs["receiver"]),str(programs["service"]),str(folder/(scenario+".index")),"--scenario",scenario,str(case)]
            with (folder/(scenario+".log")).open("w") as log:
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True);code=process.wait(timeout=180)
            stop_owned_group(process);process=None
            assert code==0,(folder/(scenario+".log")).read_text(errors="replace")[-4000:]
            result=json.loads((case/"failed-handoff-parent-result.json").read_text())
            assert result["passed"] and result["original_failure_retained"] and result["successor_adopted"]
            assert result["operator_node_repair_count"]==1 and result["parent_repair_posts"]==0
            assert result["hardware_writes"]==0 and not result["raw_forwarded"]
            cases.append({"scenario":scenario,"command":command,"exit_code":code,"result":result})
    finally:
        if process is not None:stop_owned_group(process)
        raw=list(folder.rglob("*.graw"));total=sum(path.stat().st_size for path in raw)
        assert total<=64*1024**2,"Synthetic fixture exceeded its small aggregate test payload guard"
        for path in raw:
            assert folder in path.resolve().parents and not path.is_symlink()
            deleted.append({"path":str(path.relative_to(folder)),"bytes":path.stat().st_size,"sha256":sha(path)});path.unlink()
        for path in folder.rglob("credential"):
            if path.is_file() and not path.is_symlink():path.unlink()
        (folder/"cleanup.json").write_text(json.dumps({"deleted":deleted,"raw_remaining":len(list(folder.rglob("*.graw")))},indent=2)+"\n")
    for path in folder.rglob("*.log"):
        contents=path.read_text(errors="replace")
        assert not any(marker in contents for marker in ("ERROR: AddressSanitizer","ERROR: LeakSanitizer","AddressSanitizer:DEADLYSIGNAL","runtime error:")),str(path)
    assert before=={name:sha(path) for name,path in programs.items()}
    (folder/"result.json").write_text(json.dumps({"passed":True,"cases":cases,"program_sha256":before,
        "runner_sha256":sha(Path(__file__)),"deleted_raw":deleted,"raw_remaining":0,
        "maximum_aggregate_test_bytes":64*1024**2,"real_hardware":False},indent=2)+"\n")
    print("PASS both parent encodings: explicit node repair observed, parent did not POST repair, successor adopted; raw removed:",folder)

if __name__=="__main__":main()
