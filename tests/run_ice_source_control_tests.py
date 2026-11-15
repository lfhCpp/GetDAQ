#!/usr/bin/env python3
"""Actual source control -> native ICE data stream, bounded and cleaned."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import urllib.request
from receiver_transport_integration import port,frame
from run_receiver_observation_tests import interrupted,sha,stop_owned_group


def wait(action):
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        value=action()
        if value:return value
        time.sleep(.02)
    raise AssertionError('Source control observation timeout')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['receiver','fixture','evidence']:parser.add_argument(name,type=Path)
    parser.add_argument('--owner-loss-only',action='store_true',help='Only original control connection loss, both encodings')
    parser.add_argument('--retirement-only',action='store_true',help='Explicit failed-stream retirement and rejection cases')
    args=parser.parse_args();args.receiver=args.receiver.resolve();args.fixture=args.fixture.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='source-control-',dir=args.evidence));signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    cases=[];deleted=[]
    modes=[('cycles',0),('cycles',1),('empty',0),('cross-controller',1),('unbound',0),('legacy',1),('wrong-source',0),('wrong-session',1),('policy-fault',0),('instance-change',1),('end-ack-loss',0),('observe-timeout',0),('observe-malformed',1),('parent-failure-stop',0),('remote-fault',1)]
    owner_loss=[('observe-control-loss',0),('observe-control-loss',1),('observe-control-loss-running',0),('observe-control-loss-running',1)]
    retirement=[('retirement-success',0),('retirement-success',1),('retirement-cross-controller',0),('retirement-ack-loss',0),('retirement-ack-loss',1),('retirement-idle-fault',1),('retirement-owner-loss',0)]
    modes=retirement if args.retirement_only else owner_loss if args.owner_loss_only else modes+owner_loss
    for mode,minor in modes:
        name=mode+'-'+str(minor);root=folder/name;output=root/'output';output.mkdir(parents=True);receiver=client=None
        p,s=port(),port()
        while p==s:s=port()
        success=mode in ['cycles','empty','cross-controller','end-ack-loss','observe-timeout'];expected=b''
        if mode in ['cycles','cross-controller','end-ack-loss','parent-failure-stop','remote-fault','observe-control-loss-running'] or mode.startswith('retirement-'):
            for event in range(1,3 if mode in ['parent-failure-stop','remote-fault','observe-control-loss-running'] or mode.startswith('retirement-') else 7):
                data=bytearray(frame(event));data[128:136]=bytes([event])*8;expected+=data
        try:
            with (root/'stdout.log').open('w') as stdout,(root/'stderr.log').open('w') as stderr:
                receiver=subprocess.Popen([str(args.receiver),'--output',str(output),'--session-id','d'*64,'--port',str(p),'--status-port',str(s),'--queue-mib','1','--max-frame-mib','1'],stdout=stdout,stderr=stderr,start_new_session=True)
                def status():
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:'+str(s)+'/status',timeout=.25) as response:return json.load(response)
                    except OSError:return None
                wait(status)
                with (root/'client.log').open('w') as log:
                    client=subprocess.Popen([str(args.fixture),str(p),'d'*64,str(minor),mode,str(root)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    assert client.wait(timeout=15)==0,(mode,(root/'client.log').read_text())
                if success:assert receiver.wait(timeout=8)==0
                else:
                    def incomplete():
                        value=status()
                        return value if value and value['received_bytes']==len(expected) else None
                    observed=wait(incomplete);assert receiver.poll() is None and not observed['sender_end_confirmed'],(mode,receiver.poll(),observed)
                    receiver.terminate();assert receiver.wait(timeout=8)==1
            final=json.loads((root/'stdout.log').read_text().splitlines()[-1]);seal=output/'.getdaq-ice-receiver-seal.json'
            assert not final['sender_eof_confirmed'] and seal.exists()==success
            assert b''.join(path.read_bytes() for path in sorted(output.glob('*.graw')))==expected,(mode,'raw mismatch')
            control=json.loads((root/'control-result.json').read_text());assert control['passed']
            if mode.startswith('retirement-'):
                retired=json.loads((root/'retirement-result.json').read_text());assert retired['passed'] and retired['unknown_preserved']
                assert not retired['source_end_acknowledged'] and not retired['producer_stop_repeated']
                assert retired['acknowledged']==(mode in ['retirement-success','retirement-cross-controller'])

            if mode=='end-ack-loss':assert control['unknown'] and not control['source_end_acknowledged'] and final['sender_end_confirmed'],'Control UNKNOWN must remain distinct from actual data receipt'
            cases.append({'case':name,'passed':True,'raw_bytes':len(expected),'control':control,'receiver_end_confirmed':final['sender_end_confirmed']})
        finally:
            stop_owned_group(client);stop_owned_group(receiver)
            for path in root.rglob('*.graw'):deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'cases':cases,'maximum_aggregate_test_bytes':8*1024**2,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'receiver_sha256':sha(args.receiver),'fixture_sha256':sha(args.fixture),'runner_sha256':sha(Path(__file__)),'real_hardware':False}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(cases),'actual pinned source control/session/policy/UNKNOWN/data cases; raw deleted;',folder)


if __name__=='__main__':main()
