#!/usr/bin/env python3
"""Actual ECC -> two data hosts -> source-side ICE streams; bounded raw cleanup."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
from run_receiver_observation_tests import interrupted,sha,stop_owned_group


def main():
    source=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
    for name in ['native_build','receiver','data_service','evidence']:parser.add_argument(name,type=Path)
    parser.add_argument('--observe-only',action='store_true',help='Read overlap/status responsiveness, both encodings')
    args=parser.parse_args();native=args.native_build.resolve();args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='managed-ice-ecc-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    programs={'fixture':native/'ecc-network-service-tests','ecc_service':native/'getdaq-ecc-server','receiver':args.receiver.resolve(),'data_service':args.data_service.resolve()}
    cases=['ice-observe-0','ice-observe-1'] if args.observe_only else ['ice-0','ice-1','ice-empty-0'];maximum=8*1024**2;assert maximum<20*1024**3
    command=['python3',str(source/'tests/run_ecc_network_service_tests.py'),str(programs['fixture']),str(programs['receiver']),str(programs['data_service']),str(programs['ecc_service']),
        '--output-parent',str(folder),'--jobs','1','--scenarios']+cases
    process=None;deleted=[]
    try:
        with (folder/'runner.log').open('w') as log:process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);code=process.wait(timeout=900)
        assert code==0,(folder/'runner.log').read_text()
        root=next(folder.glob('ecc-network-system-*'));results=json.loads((root/'results.json').read_text());assert len(results)==len(cases) and all(row['exit_code']==0 for row in results)
        for name in cases:
            if args.observe_only:
                proof=json.loads((root/name/'source-overlap-proof.json').read_text());assert proof['passed'] and proof['status_responsive'] and proof['overlap_status']==409 and proof['hardware_actions']==0
            snapshot=json.loads((root/name/'controller/snapshot.json').read_text());assert all(s['protocol']=='ICE' for s in snapshot['senders'])
            for node in json.loads((root/name/'final-status.json').read_text())['data']['nodes']:
                for receiver in node['lifecycle']['receivers']:
                    assert receiver['source_end_confirmed'] and receiver['seal_verified'] and receiver['reaped']
                    seal=json.loads((Path(receiver['output_directory'])/'.getdaq-ice-receiver-seal.json').read_text());assert seal['sender_end_confirmed'] and not seal['sender_eof_confirmed']
            if name=='ice-empty-0':assert all(r['received_bytes']=='0' and r['files']=='0' for n in json.loads((root/name/'final-status.json').read_text())['data']['nodes'] for r in n['lifecycle']['receivers'])
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    (folder/'result.json').write_text(json.dumps({'schema':1,'results':results,'maximum_aggregate_test_bytes':maximum,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'program_sha256':{name:sha(p) for name,p in programs.items()},'runner_sha256':sha(Path(__file__)),'real_hardware':False},indent=2)+'\n')
    print('PASS',len(results),'actual full ECC ICE/session/source-control/empty cases; raw deleted;',folder)


if __name__=='__main__':main()
