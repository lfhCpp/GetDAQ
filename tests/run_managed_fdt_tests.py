#!/usr/bin/env python3
"""Actual ECC -> two data services -> FDT reconnect/unused cycles; remove owned raw."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    source=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_build',type=Path);parser.add_argument('receiver',type=Path)
    parser.add_argument('data_service',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();native=args.native_build.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='managed-fdt-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    programs={'fixture':native/'ecc-network-service-tests','ecc_service':native/'getdaq-ecc-server',
              'receiver':args.receiver.resolve(),'data_service':args.data_service.resolve()}
    cases=['fdt-0','fdt-1','fdt-empty-0','normal-0']
    maximum_wire_bytes=len(cases)*2*2*64*4104
    maximum_aggregate_test_bytes=2*maximum_wire_bytes
    assert maximum_aggregate_test_bytes < 20*1024**3
    command=['python3',str(source/'tests/run_ecc_network_service_tests.py'),str(programs['fixture']),
             str(programs['receiver']),str(programs['data_service']),str(programs['ecc_service']),
             '--output-parent',str(folder),'--jobs','1','--scenarios']+cases
    process=None;deleted=[]
    try:
        with (folder/'runner.log').open('w') as log:
            process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            code=process.wait(timeout=900)
        assert code==0,(folder/'runner.log').read_text()
        root=next(folder.glob('ecc-network-system-*'))
        results=json.loads((root/'results.json').read_text())
        assert len(results)==len(cases) and all(row['exit_code']==0 for row in results)
        for name in cases[:3]:
            snapshot=json.loads((root/name/'controller/snapshot.json').read_text())
            assert all(sender['protocol']=='FDT' for sender in snapshot['senders'])
            plans=list((root/name).glob('node-*/deployment/plan.json'));assert len(plans)==2
            assert all(item['input_protocol']=='FDT' for plan in plans for item in json.loads(plan.read_text())['sources'])
        empty=json.loads((root/'fdt-empty-0/final-status.json').read_text())
        for node in empty['data']['nodes']:
            assert node['lifecycle']['state']=='stopped'
            for receiver in node['lifecycle']['receivers']:
                assert receiver['source_unused_confirmed'] and receiver['received_bytes']=='0' and receiver['frames']=='0' and receiver['files']=='0'
                seal=json.loads((Path(receiver['output_directory'])/'.getdaq-receiver-seal.json').read_text())
                assert seal['source_unused_confirmed'] and not seal['sender_eof_confirmed'] and seal['connections']==0 and not seal['files']
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'raw_files_remaining':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    receipt={'schema':1,'results':results,'maximum_wire_bytes':maximum_wire_bytes,'maximum_aggregate_test_bytes':maximum_aggregate_test_bytes,'aggregate_limit_bytes':20*1024**3,
             'generated_raw_deleted':not list(folder.rglob('*.graw')),'real_hardware':False,'qt_gui_executed':False,
             'program_sha256':{name:sha(path) for name,path in programs.items()},'runner_sha256':sha(Path(__file__)),
             'underlying_runner_sha256':sha(source/'tests/run_ecc_network_service_tests.py')}
    (folder/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS actual managed FDT reconnect/unused ECC and default TCP regression; raw payload deleted;',folder)


if __name__=='__main__':main()
