#!/usr/bin/env python3
"""Real source-side producer join/drain/retained-stream tests; all owned raw removed."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import urllib.request
from receiver_transport_integration import frame,port
from run_receiver_observation_tests import interrupted,sha,stop_owned_group


def wait(action):
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        value=action()
        if value:return value
        time.sleep(.02)
    raise AssertionError('Controlled source observation timeout')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['receiver','fixture','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args();args.receiver=args.receiver.resolve();args.fixture=args.fixture.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='controlled-ice-',dir=args.evidence));signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    modes=[('cycles',0),('cycles',1),('empty',0),('wrong-session',1),('idle-connect',0),('drain-fail',0),('nonstd-drain',1),('idle-stop',1),('late-feed',0),('destroy',1),('end-loss',0),('missing-callback',0)]
    cases=[];deleted=[]
    for mode,minor in modes:
        name=mode+'-'+str(minor);root=folder/name;output=root/'output';output.mkdir(parents=True);process=child=replacement=None;streams=[]
        data_port,status_port=port(),port()
        while data_port==status_port:status_port=port()
        command=[str(args.receiver),'--output',str(output),'--session-id','d'*64,'--port',str(data_port),'--status-port',str(status_port),'--queue-mib','1','--max-frame-mib','1']
        expected=b'' if mode in ['empty','wrong-session','idle-connect','missing-callback'] else frame(1) if mode in ['drain-fail','nonstd-drain','idle-stop'] else frame(1)+frame(2)
        if mode=='cycles':expected+=frame(3)+frame(4,True)
        try:
            stdout=(root/'stdout.log').open('w');stderr=(root/'stderr.log').open('w');streams.extend([stdout,stderr]);process=subprocess.Popen(command,stdout=stdout,stderr=stderr,start_new_session=True)
            def status():
                try:
                    with urllib.request.urlopen('http://127.0.0.1:'+str(status_port)+'/status',timeout=.25) as response:return json.load(response)
                except OSError:return None
            wait(status);ready=root/'source-ready';log=(root/'source.log').open('w');streams.append(log)
            child=subprocess.Popen([str(args.fixture),str(data_port),str(minor),'d'*64,mode,str(ready)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            if mode=='end-loss':
                wait(lambda:ready.exists());wait(lambda:(lambda value:value and value['received_bytes']==8192 and value['frames']==2)(status()))
                process.kill();process.wait(timeout=3)
                second=root/'replacement';second.mkdir();replacement_command=command.copy();replacement_command[replacement_command.index('--output')+1]=str(second);replacement_command[replacement_command.index('--session-id')+1]='e'*64
                rstdout=(root/'replacement.log').open('w');rstderr=(root/'replacement-error.log').open('w');streams.extend([rstdout,rstderr]);replacement=subprocess.Popen(replacement_command,stdout=rstdout,stderr=rstderr,start_new_session=True)
                wait(status);Path(str(ready)+'.continue').write_text('replacement ready\n');assert child.wait(timeout=10)==0
                observed=status();assert observed['received_bytes']==0 and observed['bound_connections']==0,'Final End reconnected to another receiver'
            else:assert child.wait(timeout=10)==0,(name,(root/'source.log').read_text())
            log.flush()
            if mode!='missing-callback':
                source=json.loads((root/'source.log').read_text().splitlines()[-1]);assert source['source_end_acknowledged']==(mode in ['cycles','empty'])
                if mode in ['wrong-session','idle-connect','drain-fail','nonstd-drain','idle-stop','late-feed','end-loss']:assert source['state']==9,'Source failed to retain UNKNOWN'
            if mode in ['cycles','empty']:
                assert process.wait(timeout=8)==0;receipt=json.loads((output/'.getdaq-ice-receiver-seal.json').read_text());assert receipt['sender_end_confirmed'] and not receipt['sender_eof_confirmed'] and int(receipt['received_bytes'])==len(expected)
            elif mode!='end-loss':
                seen=wait(lambda:(lambda value:value if value and value['received_bytes']==len(expected) and value['frames']==len(expected)//4096 else None)(status()))
                assert not seen['sender_end_confirmed'] and process.poll() is None and not (output/'.getdaq-ice-receiver-seal.json').exists()
                # accepted_file_bytes includes the writer's bounded small-frame
                # buffer. Only fixture cleanup closes it here, never a source End.
                process.terminate();assert process.wait(timeout=8)==1
                final=json.loads((root/'stdout.log').read_text().splitlines()[-1])
                assert not final['sender_end_confirmed'] and not final['sender_eof_confirmed'] and not (output/'.getdaq-ice-receiver-seal.json').exists()
            # Normal/error bytes are compared before fixture cleanup; killed source
            # files are preserved if present but not treated as persistence proof.
            if mode!='end-loss':assert b''.join(p.read_bytes() for p in sorted(output.glob('*.graw')))==expected,(name,'raw mismatch')
            cases.append({'case':name,'passed':True,'expected_bytes':len(expected),'success':mode in ['cycles','empty']})
        finally:
            stop_owned_group(child);stop_owned_group(process);stop_owned_group(replacement)
            for stream in streams:stream.close()
            for path in root.rglob('*.graw'):deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    maximum=8*1024**2;assert maximum<20*1024**3
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':cases,'maximum_aggregate_test_bytes':maximum,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'receiver_sha256':sha(args.receiver),'fixture_sha256':sha(args.fixture),'runner_sha256':sha(Path(__file__)),'real_hardware':False},indent=2)+'\n')
    print('PASS',len(cases),'actual controlled ICE source join/drain/cycles/UNKNOWN cases; raw deleted;',folder)


if __name__=='__main__':main()
