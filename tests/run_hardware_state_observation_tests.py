#!/usr/bin/env python3
"""Actual native/HTTP hardware-state observations; metadata-only, owned credentials cleaned."""
import argparse
import json
import signal
import subprocess
import tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('build','receiver','data_service','evidence'):
        p.add_argument(name,type=Path)
    args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='hardware-state-',dir=args.evidence)).resolve()
    signal.signal(signal.SIGINT,interrupted)
    signal.signal(signal.SIGTERM,interrupted)
    build=args.build.resolve()
    programs={name:build/name for name in ('native-ecc-coordinator-tests','ecc-network-service-tests','getdaq-ecc-server')}
    process=None
    rows=[]
    try:
        for minor in (0,1):
            scenario='hardware-observation-'+str(minor)
            for kind in ('native','http'):
                name=kind+'-'+str(minor)
                if kind=='native':
                    command=[str(programs['native-ecc-coordinator-tests']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(folder/(name+'.jsonl')),'--scenario',scenario]
                else:
                    command=[str(programs['ecc-network-service-tests']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(args.receiver.resolve()),str(args.data_service.resolve()),str(programs['getdaq-ecc-server']),str(folder/name),'--scenario',scenario]
                with (folder/(name+'.log')).open('w') as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=process.wait(timeout=90)
                assert code==0,(folder/(name+'.log')).read_text()[-2500:]
                stop_owned_group(process)
                process=None
                rows.append({'name':name,'exit_code':code})
                if kind=='native':
                    native_root=Path(json.loads((folder/(name+'.jsonl.root.json')).read_text())['root'])
                    race=json.loads((native_root/'hardware-state-race.json').read_text())
                    assert all(race[key] for key in ('held_actual_ice_rpc','negative_request_acquired_lock','parent_state_and_id_unchanged','samples_withdrawn'))
                    assert race['hardware_writes']==0
                    (folder/(name+'-race.json')).write_text(json.dumps(race,indent=2)+'\n')
                else:
                    scope=json.loads((folder/name/'scope.json').read_text())
                    assert not scope['real_receivers'] and not scope['full_ecc_workflow_tested']
                    response=json.loads((folder/name/'hardware-state.json').read_text())
                    assert response['consistent'] and not response['recovery_verified']
                    mismatch=json.loads((folder/name/'hardware-state-mismatch.json').read_text())
                    assert not mismatch['nodes'][0]['available'] and not mismatch['nodes'][0]['values']
                    input_fault=json.loads((folder/name/'hardware-state-input-fault.json').read_text())
                    assert input_fault=={'changed_source_status':503,'restored_source_status':503,'fixed_bit_reads':0,'hardware_writes':0}
    finally:
        stop_owned_group(process)
        for n in ('credential','ecc-credential','wrong-hardware-credential'):
            for file in folder.rglob(n):
                if file.is_file() and not file.is_symlink():file.unlink()
        assert not list(folder.rglob('*.graw'))
    (folder/'result.json').write_text(json.dumps({'passed':True,'runs':rows,'raw_remaining':0,
        'maximum_test_bytes':4*1024**2,'metadata_only':True,'real_hardware':False,
        'receiver_executed':False,'program_sha256':{name:sha(file) for name,file in programs.items()}},indent=2)+'\n')
    print('PASS two Ice encodings, native and authenticated actual HTTP, no raw;',folder)


if __name__=='__main__':
    main()
