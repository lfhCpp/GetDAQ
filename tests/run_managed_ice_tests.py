#!/usr/bin/env python3
"""Actual mixed native/ICE child supervision and catalog; bounded owned payload cleanup."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
from run_receiver_observation_tests import interrupted, sha, stop_owned_group
from receiver_transport_integration import frame


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixture','native','ice','source','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args()
    for name in ['fixture','native','ice','source']:setattr(args,name,getattr(args,name).resolve())
    args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='managed-ice-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    cases=[('normal',0),('normal',1),('mixed',1),('empty',0),('byte-storage',1),('missing-end',1),('truncated',0),('receipt-tamper',1),('archive-tamper',0),('abort-startup',1),('wrong-program',0)]
    maximum_bytes=8*1024**2;assert maximum_bytes<20*1024**3
    results=[];deleted=[];process=None
    try:
        for case,minor in cases:
            root=folder/(case+'-'+str(minor));root.mkdir()
            command=[str(args.fixture),str(args.native),str(args.ice),str(args.source),str(root),case,str(minor)]
            try:
                with (root/'fixture.log').open('w') as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=process.wait(timeout=25)
                assert code==0,(case,code,(root/'fixture.log').read_text())
                # Compare original bytes, independently of manager counters and receipts.
                if case not in ['abort-startup','wrong-program']:
                    for index,directory in enumerate(sorted((root/'incoming').glob('getdaq-*'))):
                        expected=b'' if case=='empty' else bytes((i*37+11)&255 for i in range(65537)) if case=='byte-storage' else frame(1)+frame(2,True)
                        if case=='mixed' and index==0:
                            native=bytearray([1])*4096
                            native[:16]=bytes([6,0,0,64,7,0,1,4,0,2,0,4,0,0,3,224]);native[26:28]=b'\0\0';expected=bytes(native)
                        actual=b''.join(path.read_bytes() for path in sorted(directory.glob('*.graw')))
                        assert actual==expected,(case,index,len(actual),len(expected))
                results.append({'case':case,'encoding':minor,'exit_code':code})
            finally:
                stop_owned_group(process);process=None
                for path in root.rglob('*.graw'):
                    deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
                (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':results,'maximum_aggregate_test_bytes':maximum_bytes,'aggregate_limit_bytes':20*1024**3,
        'raw_deleted':not list(folder.rglob('*.graw')),'real_hardware':False,'program_sha256':{name:sha(getattr(args,name)) for name in ['fixture','native','ice','source']},'runner_sha256':sha(Path(__file__))},indent=2)+'\n')
    print('PASS',len(results),'actual managed ICE/mixed/counters/catalog/error cases; raw deleted;',folder)


if __name__=='__main__':main()
