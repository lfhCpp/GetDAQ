#!/usr/bin/env python3
"""Retained-object ECC production stop after external fixed-input faults."""
import argparse,json,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('build','receiver','data_service','evidence'):p.add_argument(name,type=Path)
    p.add_argument('--ecc-service',type=Path)
    p.add_argument('--client-probe',type=Path)
    p.add_argument('--scenarios',nargs='+',default=['input-fault-running-0','input-fault-running-1','input-fault-prepared-0','input-fault-prepared-1','input-fault-audit-0','input-fault-audit-1','normal-0','normal-1'])
    a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='input-fault-stop-',dir=a.evidence)).resolve()
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    root=Path(__file__).resolve().parents[1];build=a.build.resolve()
    service=a.ecc_service.resolve() if a.ecc_service else build/'getdaq-ecc-server'
    programs={'fixture':build/'ecc-network-service-tests','ecc':service,'receiver':a.receiver.resolve(),'data_service':a.data_service.resolve()}
    process=None;rows=[]
    try:
        for scenario in a.scenarios:
            command=[str(programs['fixture']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(programs['receiver']),str(programs['data_service']),str(service),str(folder/scenario),'--scenario',scenario]
            if scenario.startswith('input-fault-qt-'):
                assert a.client_probe,'Pass --client-probe for production Qt integration'
                command.append(str(a.client_probe.resolve()))
            with (folder/(scenario+'.log')).open('w') as log:
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                code=process.wait(timeout=150)
            assert code==0,(folder/(scenario+'.log')).read_text()[-3500:]
            stop_owned_group(process);process=None;rows.append({'scenario':scenario,'exit_code':code})
            if scenario.startswith('input-fault-'):
                proof=json.loads((folder/scenario/'input-fault-result.json').read_text());assert proof['passed']
                if 'audit' in scenario:assert proof['audit_fault_refused'] and proof['hardware_unchanged']
                else:assert proof['negative_stop_complete'] and proof['unknown_preserved'] and proof['reopen_no_replay_or_takeover']
    finally:
        stop_owned_group(process);deleted=[]
        for file in folder.rglob('*.graw'):
            if file.is_file() and not file.is_symlink():deleted.append({'path':str(file.relative_to(folder)),'bytes':file.stat().st_size});file.unlink()
        for name in ('credential','ecc-credential'):
            for file in folder.rglob(name):
                if file.is_file() and not file.is_symlink():file.unlink()
        remaining=[str(file) for file in folder.rglob('*.graw')]
        (folder/'cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':remaining},indent=2)+'\n')
    assert not remaining
    (folder/'result.json').write_text(json.dumps({'passed':True,'runs':rows,'maximum_aggregate_test_bytes':512*1024**2,'raw_remaining':0,'real_hardware':False,'mock_hardware_actual_ice':True,'program_sha256':{k:sha(v) for k,v in programs.items()}},indent=2)+'\n')
    print('PASS retained original targets, input-fault negative stop and audit refusal; own raw removed;',folder)
if __name__=='__main__':main()
