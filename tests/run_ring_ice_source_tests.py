#!/usr/bin/env python3
"""Two real producer/consumer threads, borrowed ring spans, ICE receiver, bounded cleanup."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
import urllib.request
from receiver_transport_integration import port,frame
from run_receiver_observation_tests import interrupted,sha,stop_owned_group
from run_ice_source_control_tests import wait


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['receiver','fixture','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args();args.receiver=args.receiver.resolve();args.fixture=args.fixture.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='ring-ice-',dir=args.evidence));signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    cases=[];deleted=[]
    for mode,minor in [('cycles',0),('cycles',1),('empty',0),('publish-fault',1),('partial-header',0),('partial-body',1)]:
        root=folder/(mode+'-'+str(minor));output=root/'output';output.mkdir(parents=True);receiver=client=None
        p,s=port(),port()
        while p==s:s=port()
        try:
            with (root/'stdout.log').open('w') as stdout,(root/'stderr.log').open('w') as stderr:
                receiver=subprocess.Popen([str(args.receiver),'--output',str(output),'--session-id','e'*64,'--port',str(p),'--status-port',str(s),'--queue-mib','1','--max-frame-mib','1'],stdout=stdout,stderr=stderr,start_new_session=True)
                def status():
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:'+str(s)+'/status',timeout=.25) as response:return json.load(response)
                    except OSError:return None
                wait(status)
                with (root/'client.log').open('w') as log:
                    client=subprocess.Popen([str(args.fixture),str(p),'e'*64,str(minor),mode,str(root/'source-result.json')],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    assert client.wait(timeout=15)==0,(mode,(root/'client.log').read_text())
                result=json.loads((root/'source-result.json').read_text())
                if mode in ['publish-fault','partial-header','partial-body']:
                    expected_received=317 if mode=='publish-fault' else 8192
                    observed=wait(lambda: (value if value and value['received_bytes']==expected_received else None) if (value:=status()) is not None else None)
                    assert receiver.poll() is None and not observed['sender_end_confirmed']
                    assert result['ring_unknown'] and result['source_unknown'] and result['raw_unknown'] and not result['end_acknowledged']
                    if mode=='publish-fault':assert result['handed_total']==317 and result['published_total']==0
                    else:assert result['handed_total']==result['published_total']==result['complete_write_total']==8192 and result['raw_write_total']==8192+(7 if mode=='partial-header' else 2048)
                    receiver.terminate();assert receiver.wait(timeout=8)==1
                else:
                    assert receiver.wait(timeout=8)==0
                    assert not result['ring_unknown'] and not result['source_unknown'] and result['end_acknowledged']
            final=json.loads((root/'stdout.log').read_text().splitlines()[-1]);expected=b''
            if mode in ['cycles','partial-header','partial-body']:
                for event in range(1,10 if mode=='cycles' else 3):
                    data=bytearray(frame(event));data[128:136]=bytes([event])*8;expected+=data
                assert result['handed_total']==result['published_total']==len(expected)
            assert b''.join(path.read_bytes() for path in sorted(output.glob('*.graw')))==expected,(mode,'raw mismatch')
            assert not final['sender_eof_confirmed']
            assert (output/'.getdaq-ice-receiver-seal.json').exists()==(mode in ['cycles','empty'])
            cases.append({'case':mode+'-'+str(minor),'passed':True,'raw_bytes':len(expected),'source':result,'received_bytes':final['received_bytes']})
        finally:
            stop_owned_group(client);stop_owned_group(receiver)
            for path in root.rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':cases,'maximum_aggregate_test_bytes':2*1024**2,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'receiver_sha256':sha(args.receiver),'fixture_sha256':sha(args.fixture),'runner_sha256':sha(Path(__file__)),'real_hardware':False},indent=2)+'\n')
    print('PASS',len(cases),'actual two-thread ring/ICE/drain/publish-UNKNOWN cases; raw deleted;',folder)


if __name__=='__main__':main()
