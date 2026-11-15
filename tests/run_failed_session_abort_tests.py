#!/usr/bin/env python3
"""Bounded actual owned-receiver cleanup tests; preserve metadata and delete only test raw."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
from types import SimpleNamespace
from run_supervisor_control_tests import Rig, frame, require, wait_for, process_identity
from run_receiver_observation_tests import stop_owned_group


def ports(count):
    held=[]
    try:
        for _ in range(count):
            s=socket.socket();s.bind(('127.0.0.1',0));held.append(s)
        return [s.getsockname()[1] for s in held]
    finally:
        for s in held:s.close()


def network(args, root):
    rig=Rig.__new__(Rig)
    try:
        rig.__init__(args,root,ports(5))
        pre=rig.make_request('abort-failed-session','no-owner');pre['expected_session_id']='a'*64
        require(rig.request('POST','/v1/operations',pre)[0]==409,'Unowned abort was accepted')
        rig.start();before=rig.status()['lifecycle'];rig.connect_senders()
        payload=frame(1);rig.sockets[1].sendall(payload)
        bad=rig.make_request('abort-failed-session','wrong-owner');bad['expected_session_id']=before['session_id'];bad['controller_epoch']='e'*64
        require(rig.request('POST','/v1/operations',bad)[0]==409,'Other owner epoch was accepted')
        wrong=rig.make_request('abort-failed-session','wrong-session');wrong['expected_session_id']='f'*64
        require(rig.request('POST','/v1/operations',wrong)[0]==412,'Wrong session was accepted')
        require(rig.query('receivers')['observation']['receivers'][1]['state']=='running','Wrong session stopped healthy receiver')
        pid=before['receivers'][0]['pid'];require(process_identity(pid)==rig.children[pid],'Victim ownership changed')
        os.kill(pid,signal.SIGKILL)
        wait_for(lambda:rig.query('receivers')['observation']['receivers'][0]['state']=='exited')
        # A normal Finish fails on the dead child and the still-connected peer;
        # negative cleanup must remain possible under the resulting control hold.
        rig.submit(rig.make_request('finish','failed-finish'));rig.completed('failed-finish','failed')
        require(rig.status()['control_needs_reconciliation'],'Failed Finish did not preserve control hold')
        request=rig.make_request('abort-failed-session','abort-failed');request['expected_session_id']=before['session_id']
        file=root/'abort-request.json';file.write_text(json.dumps(request)+'\n')
        accepted=rig.query('submit','--request-file',file);require(accepted['operation_id']=='abort-failed','CLI accepted another request')
        result=rig.completed('abort-failed');after=result['result']
        require(after['state']=='aborted' and after['needs_reconciliation'] and after['session_id']==before['session_id'],'Abort falsely recovered/completed data')
        require(all(r['reaped'] and not r['seal_verified'] and r['pid']==before['receivers'][i]['pid'] for i,r in enumerate(after['receivers'])),'Aborted receiver identity/seal differs')
        require(rig.status()['control_needs_reconciliation'],'Cleanup cleared control reconciliation')
        directory=Path(after['receivers'][1]['output_directory'])
        require(b''.join(p.read_bytes() for p in sorted(directory.glob('*.graw')))==payload,'Cleanup deleted or changed raw data')
        require(not (directory/'.getdaq-receiver-seal.json').exists(),'Cleanup fabricated EOF receipt')
        require(rig.query('submit','--request-file',file)['phase']=='complete','Same ID did not return original terminal record')
        raw_journal=(Path(rig.plan['audit_root'])/'supervisor.jsonl').read_text()
        require(raw_journal.count('"event":"failed-session-signal-intent"')==1,'Same ID replayed a signal')
        restart=rig.make_request('start','restart-forbidden')
        require(rig.request('POST','/v1/operations',restart)[0]==409,'Failed cleanup allowed positive replay')
        old_instance=rig.identity['instance_id'];rig.stop_service();rig.start_service()
        require(rig.identity['instance_id']!=old_instance,'Restart reused service instance')
        cold=rig.make_request('abort-failed-session','cold-abort');cold['expected_session_id']=before['session_id']
        code,_=rig.request('POST','/v1/operations',cold)
        if code==202:rig.completed('cold-abort','failed')
        else:require(code in (409,412),'Unexpected cold abort rejection')
        (root/'network-proof.json').write_text(json.dumps({'passed':True,'real_receivers':True,'production_query_cli':True,'original_session_only':True,'raw_preserved_before_test_cleanup':True,'automatic_restart':False,'acquisition_complete':False,'hardware_stopped_verified':False},indent=2)+'\n')
    finally:
        rig.close()
        for pid,identity in rig.children.items():
            if process_identity(pid)==identity:
                try:wait_for(lambda:process_identity(pid)!=identity,timeout=5)
                except AssertionError:
                    if process_identity(pid)==identity:os.kill(pid,signal.SIGKILL)
                    wait_for(lambda:process_identity(pid)!=identity,timeout=3)
        if hasattr(rig,'token_file') and rig.token_file.exists():rig.token_file.unlink()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixture','service','query','codec','receiver']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--output-parent',type=Path,required=True)
    args=parser.parse_args()
    for name in ['fixture','service','query','codec','receiver']:setattr(args,name,getattr(args,name).resolve())
    args.output_parent.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='failed-session-abort-',dir=args.output_parent.resolve()))
    # Six fixed small-frame scenarios plus one HTTP case: conservative total input/output bound.
    maximum=16*1024**2;require(maximum<20*1024**3,'Aggregate test budget exceeds user limit')
    results=[];deleted=[]
    def interrupted(_number,_frame):raise KeyboardInterrupt()
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    try:
        for scenario in ['abort-failed','abort-failed-fdt','abort-failed-zbuf','normal','fdt-cycle','zbuf-managed']:
            process=None
            with (root/(scenario+'.log')).open('w') as log:
                try:
                    process=subprocess.Popen([str(args.fixture),str(args.receiver),str(root/scenario),scenario],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=process.wait(timeout=45)
                finally:stop_owned_group(process)
            require(code==0,'Fixture failed: '+scenario);results.append(scenario)
        with (root/'codec.log').open('w') as log:
            subprocess.run([str(args.codec),'codec'],stdout=log,stderr=subprocess.STDOUT,timeout=10,check=True)
        network(args,root/'http')
        sibling=Rig.__new__(Rig)
        try:
            sibling.__init__(args,root/'http-parent-failure',ports(5));sibling.start()
            before=sibling.status()['lifecycle'];request=sibling.make_request('abort-failed-session','parent-failed-cleanup')
            request['expected_session_id']=before['session_id'];request['parent_failure_operation_id']='original-parent-failed-start'
            sibling.submit(request);result=sibling.completed('parent-failed-cleanup')['result']
            require(result['state']=='aborted' and result['needs_reconciliation'] and all(r['reaped'] and not r['seal_verified'] for r in result['receivers']),'Parent-declared failure did not terminate original healthy siblings incompletely')
            journal=(Path(sibling.plan['audit_root'])/'supervisor.jsonl').read_text()
            require('"parent_failure_independently_verified":false' in journal and '"hardware_stopped_verified":false' in journal,'Parent declaration promoted to independent hardware proof')
            (sibling.root/'parent-failure-proof.json').write_text(json.dumps({'passed':True,'original_parent_declaration':True,'local_receivers_were_healthy':True,'hardware_stopped_verified':False,'acquisition_complete':False},indent=2)+'\n')
        finally:
            sibling.close()
            if hasattr(sibling,'token_file') and sibling.token_file.exists():sibling.token_file.unlink()
    finally:
        for p in root.rglob('*.graw'):
            if p.is_file() and not p.is_symlink():
                deleted.append({'path':str(p.relative_to(root)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()});p.unlink()
        require(sum(d['bytes'] for d in deleted)<maximum,'Test raw exceeded conservative fixed-case budget')
        (root/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':[str(p.relative_to(root)) for p in root.rglob('*.graw') if p.is_file()]},indent=2)+'\n')
    (root/'result.json').write_text(json.dumps({'passed':True,'core_scenarios':results,'network_checked':True,'maximum_aggregate_test_bytes':maximum,'raw_deleted':True,'hardware_verified':False},indent=2)+'\n')
    print('PASS failed-session abort, original owner/session, actual receivers and query CLI; raw removed;',root)


if __name__=='__main__':main()
