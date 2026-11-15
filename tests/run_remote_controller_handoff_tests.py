#!/usr/bin/env python3
"""ECC data adapter transfer to a prebound successor, with real node services."""
import argparse
import json
import os
import signal
import subprocess
import tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted, sha, stop_owned_group

CASES = ["handoff-0", "handoff-1", "handoff-ack-loss", "handoff-receipt-fault"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["fixture", "receiver", "service", "evidence"]: parser.add_argument(name, type=Path)
    completion=['handoff-complete-0','handoff-complete-1','handoff-retire-complete-0','handoff-retire-complete-1','handoff-cold-retire-0','handoff-cold-retire-1','handoff-cold-control-0','handoff-cold-control-1']
    parser.add_argument('--cases',nargs='+',choices=CASES+['handoff-planned-0','handoff-planned-1']+completion,default=CASES)
    parser.add_argument('--sanitized',action='store_true',help='Run fixture with ASan/UBSan/LeakSanitizer failure gates')
    args = parser.parse_args()
    for name in ["fixture", "receiver", "service"]: setattr(args, name, getattr(args, name).resolve(strict=True))
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="remote-controller-handoff-", dir=args.evidence)).resolve()
    source = Path(__file__).resolve().parents[1]
    before = {name:sha(getattr(args,name)) for name in ["fixture", "receiver", "service"]}
    signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
    rows=[]; deleted=[]; process=None
    environment=dict(os.environ)
    if args.sanitized:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    try:
        for case in args.cases:
            command=[str(args.fixture),str(source/"tests/data/cobo-hardware.json"),str(source/"tests/data/mutant-hardware.json"),
                str(args.receiver),str(args.service),str(root/(case+".index")),"--scenario",case,str(root/case)]
            with (root/(case+".log")).open("w") as log:
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True,env=environment)
                assert process.wait(timeout=240)==0,(root/(case+".log")).read_text()[-4000:]
            stop_owned_group(process);process=None
            proof=json.loads((root/case/"handoff-proof.json").read_text())
            assert proof["passed"] and proof["original_manager_fenced"] and proof["old_raw_retained"] and not proof["raw_forwarded"]
            assert proof["encoding_minor"]==(0 if case.endswith('-0') else 1)
            assert proof["reply_loss"]==(case=="handoff-ack-loss" or case.startswith('handoff-complete-') or case.startswith('handoff-cold-control-'))
            if case.startswith('handoff-retire-complete-'):
                assert proof['explicit_plan_completion']
            if case.startswith('handoff-cold-'):
                assert all(proof[k] for k in ['cold_data_completion','original_objects_destroyed','native_journal_unchanged'])
                assert proof['production_cli_integrated'] is False
            if case.startswith('handoff-planned-'):
                plan_path=root/case/'successor-inputs/successor.json'
                plan=json.loads(plan_path.read_text());status=json.loads((root/case/'planned-startup-status.json').read_text())
                marker=json.loads(Path(str(plan_path)+'.activation.json').read_text())
                receipt=json.loads((root/case/'control-handoff-result.json').read_text())
                assert marker['plan_sha256']==sha(plan_path)
                assert receipt['successor_epoch']==plan['receiver_controller_epoch']==status['data']['controller_epoch']
                assert receipt['successor_scope_sha256']==plan['data_controller_scope_sha256']==status['data']['controller_scope_sha256']
                assert status['snapshot_sha256']==plan['snapshot_sha256'] and not status['data']['needs_reconciliation']
                assert len(status['data']['nodes'])==2
                for node in status['data']['nodes']:
                    life=node['lifecycle'];assert life['state']=='stopped' and life['receivers']
                    assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
            rows.append({"case":case,"passed":True})
    finally:
        stop_owned_group(process)
        for path in root.rglob("*.graw"):
            deleted.append({"path":str(path.relative_to(root)),"bytes":path.stat().st_size,"sha256":sha(path)});path.unlink()
        (root/"cleanup.json").write_text(json.dumps({"deleted":deleted,"raw_remaining":len(list(root.rglob("*.graw")))},indent=2)+"\n")
    assert sum(row["bytes"] for row in deleted)<=2*1024**2
    assert before=={name:sha(getattr(args,name)) for name in before}
    (root/"result.json").write_text(json.dumps({"passed":True,"cases":rows,"program_sha256":before,"raw_remaining":0,
        "maximum_aggregate_test_bytes":2*1024**2,"aggregate_limit_bytes":20*1024**3,"real_hardware":False,
        "runner_sha256":sha(Path(__file__).resolve()),"sanitized":args.sanitized,"leak_detection":args.sanitized},indent=2)+"\n")
    print("PASS original and successor ECC data adapters; two actual services, no automatic replay; owned raw removed;",root)


if __name__ == "__main__": main()
