#!/usr/bin/env python3
"""Offscreen QWidget regression in a private temp root; clean owned synthetic payload."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program",type=Path);parser.add_argument("evidence",type=Path)
    args=parser.parse_args();args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix="receiver-policy-widgets-",dir=args.evidence)).resolve()
    temp=folder/"tmp";temp.mkdir(mode=0o700);runtime=folder/"runtime";runtime.mkdir(mode=0o700)
    # The sole simulator writes <=64 KiB/200ms/node; one CoBo in this suite.
    # A 240s timeout plus all tiny mock files fits this whole-round bound.
    maximum=256*1024**2;assert maximum<20*1024**3
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    env=dict(os.environ,QT_QPA_PLATFORM="offscreen",TMPDIR=str(temp),XDG_RUNTIME_DIR=str(runtime))
    process=None;deleted=[]
    try:
        with (folder/"widgets.log").open("w") as log:
            process=subprocess.Popen([str(args.program.resolve())],stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True);code=process.wait(timeout=240)
        assert code==0,(folder/"widgets.log").read_text()[-5000:]
    finally:
        stop_owned_group(process)
        for p in temp.rglob("*.graw"):
            if p.is_file() and not p.is_symlink():deleted.append({"path":str(p.relative_to(folder)),"bytes":p.stat().st_size,"sha256":sha(p)});p.unlink()
        remaining=[str(p.relative_to(folder)) for p in temp.rglob("*.graw") if p.is_file()]
        (folder/"payload-cleanup.json").write_text(json.dumps({"deleted_by_wrapper":deleted,"remaining_raw":remaining,
            "successful_QTemporaryDir_cases_remove_payload_at_destruction":True},indent=2)+"\n")
    assert not remaining
    (folder/"result.json").write_text(json.dumps({"schema":1,"passed":True,"exit_code":code,"program":str(args.program.resolve()),
        "program_sha256":sha(args.program.resolve()),"runner_sha256":sha(Path(__file__)),"maximum_aggregate_test_bytes":maximum,
        "aggregate_limit_bytes":20*1024**3,"raw_deleted":True,"target_platforms_verified":False,"real_hardware":False},indent=2)+"\n")
    print("PASS complete offscreen QWidget regression; owned raw removed;",folder)

if __name__=="__main__":main()
