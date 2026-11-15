#!/usr/bin/env python3
"""Actual source library -> ICE recorder, with fixed-connection failure checks."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import urllib.request
from receiver_transport_integration import frame, port
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def wait(action):
    deadline=time.monotonic()+6
    while time.monotonic()<deadline:
        value=action()
        if value:return value
        time.sleep(.01)
    raise AssertionError('ICE source observation timeout')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receiver',type=Path);parser.add_argument('source',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();args.receiver=args.receiver.resolve();args.source=args.source.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='ice-source-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    modes=[('normal',0),('normal',1),('discover',1),('empty',0),('destroy',0),('wrong-session',1),('send-loss',0),('finish-loss',1)]
    cases=[];deleted=[]
    for mode,minor in modes:
        root=folder/(mode+'-'+str(minor));output=root/'output';output.mkdir(parents=True)
        data_port,status_port=port(),port()
        while data_port==status_port:status_port=port()
        processes=[];receiver=None;client=None;replacement=None
        receiver_args=[str(args.receiver),'--output',str(output),'--port',str(data_port),'--status-port',str(status_port),
                       '--session-id','d'*64,'--queue-mib','1','--max-frame-mib','1']
        logs=[];lost=mode in ['send-loss','finish-loss'];success=mode in ['normal','discover','empty']
        try:
            stdout=(root/'receiver.stdout').open('w');stderr=(root/'receiver.stderr').open('w');logs+=[stdout,stderr]
            receiver=subprocess.Popen(receiver_args,stdout=stdout,stderr=stderr,start_new_session=True);processes.append(receiver)
            def status():
                try:
                    with urllib.request.urlopen('http://127.0.0.1:'+str(status_port)+'/status',timeout=.2) as response:return json.load(response)
                except OSError:return None
            wait(status)
            log=(root/'source.stdout').open('w');err=(root/'source.stderr').open('w');logs+=[log,err]
            ready=root/'ready'
            client=subprocess.Popen([str(args.source),str(data_port),str(minor),mode,str(ready)],stdout=log,stderr=err,start_new_session=True);processes.append(client)
            if lost:
                wait(lambda:ready.exists())
                wait(lambda:(lambda s:s if s and s['received_bytes']==8192 and s['frames']==2 else None)(status()))
                receiver.kill();receiver.wait(timeout=3)
                fresh=root/'replacement-output';fresh.mkdir()
                new_args=receiver_args.copy();new_args[new_args.index('--output')+1]=str(fresh);new_args[new_args.index('--session-id')+1]='e'*64
                new_log=(root/'replacement.stdout').open('w');new_err=(root/'replacement.stderr').open('w');logs+=[new_log,new_err]
                replacement=subprocess.Popen(new_args,stdout=new_log,stderr=new_err,start_new_session=True);processes.append(replacement)
                wait(lambda:(lambda s:s if s and s['session_id']=='e'*64 else None)(status()))
                Path(str(ready)+'.continue').write_text('replacement ready\n')
            assert client.wait(timeout=10)==0,(mode,(root/'source.stderr').read_text())
            source_status=json.loads((root/'source.stdout').read_text().splitlines()[-1])
            if lost:
                observed=status();assert observed['received_bytes']==0 and observed['bound_connections']==0 and not observed['sender_end_confirmed'],'Fixed source silently reconnected to replacement'
                assert source_status['state']==6 and not source_status['source_end_acknowledged']
                replacement.terminate();assert replacement.wait(timeout=6)==1
            elif success:
                assert receiver.wait(timeout=6)==0
                expected=b'' if mode=='empty' else frame(1)+frame(2,True)
                assert b''.join(p.read_bytes() for p in sorted(output.glob('*.graw')))==expected
                receipt=json.loads((output/'.getdaq-ice-receiver-seal.json').read_text())
                assert receipt['sender_end_confirmed'] and not receipt['sender_eof_confirmed'] and int(receipt['received_bytes'])==len(expected)
                assert source_status['source_end_acknowledged'] and source_status['locally_sent_bytes']==len(expected)
            else:
                observed=wait(status);assert not observed['sender_end_confirmed'] and not (output/'.getdaq-ice-receiver-seal.json').exists()
                if mode=='wrong-session':assert observed['received_bytes']==0 and source_status['state']==6
                else:
                    wait(lambda:(lambda s:s if s and s['frames']==2 else None)(status()))
                    assert not source_status['finish_issued'],'Source destructor sent implicit end'
                receiver.terminate();assert receiver.wait(timeout=6)==1
            cases.append({'case':mode+'-'+str(minor),'source_status':source_status,'passed':True,'replacement_received_bytes':0 if lost else None})
        finally:
            for process in reversed(processes):stop_owned_group(process)
            for log in logs:log.close()
            for path in root.rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    legacy_fixture=args.source.parent/'ice-ingress-fixture'
    root=folder/'legacy-peer';root.mkdir();processes=[];logs=[]
    try:
        selected=port();log=(root/'legacy.stdout').open('w');err=(root/'legacy.stderr').open('w');logs+=[log,err]
        legacy=subprocess.Popen([str(legacy_fixture),'127.0.0.1',str(selected),'d'*64,'legacy-server','0'],stdout=log,stderr=err,start_new_session=True);processes.append(legacy)
        wait(lambda:'legacy_ready' in (root/'legacy.stdout').read_text())
        with (root/'source.stdout').open('w') as source_log,(root/'source.stderr').open('w') as source_error:
            client=subprocess.Popen([str(args.source),str(selected),'0','legacy-peer',str(root/'ready')],stdout=source_log,stderr=source_error,start_new_session=True);processes.append(client)
            assert client.wait(timeout=10)==0,(root/'source.stderr').read_text()
        observed=json.loads((root/'source.stdout').read_text().splitlines()[-1]);assert observed['state']==6 and observed['issued_bytes']==0 and not observed['finish_issued']
        legacy.terminate();assert legacy.wait(timeout=5)==0
        assert json.loads((root/'legacy.stdout').read_text().splitlines()[-1])['data_calls']==0,'Legacy unsupported extension accepted raw bytes'
        cases.append({'case':'legacy-peer-0','source_status':observed,'passed':True,'legacy_data_calls':0})
    finally:
        for process in reversed(processes):stop_owned_group(process)
        for log in logs:log.close()
    sender=args.source.parent/'getdaq-ice-send'
    for mode in ['file','stdin','limit','interrupt']:
        root=folder/('cli-'+mode);output=root/'output';output.mkdir(parents=True);payload=frame(1)+frame(2,True)
        data_port,status_port=port(),port()
        while data_port==status_port:status_port=port()
        processes=[];logs=[]
        try:
            out=(root/'receiver.stdout').open('w');err=(root/'receiver.stderr').open('w');logs+=[out,err]
            receiver=subprocess.Popen([str(args.receiver),'--output',str(output),'--session-id','d'*64,'--port',str(data_port),'--status-port',str(status_port),'--queue-mib','1'],stdout=out,stderr=err,start_new_session=True);processes.append(receiver)
            def read_status():
                try:
                    with urllib.request.urlopen('http://127.0.0.1:'+str(status_port)+'/status',timeout=.2) as response:return json.load(response)
                except OSError:return None
            wait(read_status)
            selected='-'
            if mode in ['file','limit']:
                selected=str(root/'input.graw');Path(selected).write_bytes(payload)
            command=[str(sender),'--port',str(data_port),'--input',selected,'--chunk-kib','1','--max-bytes',str(len(payload)-1 if mode=='limit' else 1024**2)]
            out=(root/'source.stdout').open('w');err=(root/'source.stderr').open('w');logs+=[out,err]
            client=subprocess.Popen(command,stdin=subprocess.PIPE if selected=='-' else subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True);processes.append(client)
            if selected=='-':
                client.stdin.write(payload);client.stdin.flush()
                if mode=='stdin':client.stdin.close()
                else:
                    wait(lambda:(lambda value:value if value and value['frames']==2 else None)(read_status()))
                    assert not read_status()['sender_end_confirmed'];client.terminate()
            code=client.wait(timeout=10)
            success=mode in ['file','stdin'];assert code==(0 if success else 1)
            if success:
                assert receiver.wait(timeout=6)==0
                assert b''.join(p.read_bytes() for p in sorted(output.glob('*.graw')))==payload
                observed=json.loads((root/'source.stdout').read_text().splitlines()[-1]);assert observed['source_end_acknowledged'] and not observed['receiver_seal_verified']
            else:
                observed=read_status();assert not observed['sender_end_confirmed'] and not (output/'.getdaq-ice-receiver-seal.json').exists()
                if mode=='limit':assert observed['received_bytes']==0 and observed['bound_connections']==0
                receiver.terminate();assert receiver.wait(timeout=6)==1
            cases.append({'case':'cli-'+mode,'passed':True,'cli_exit':code})
        finally:
            for process in reversed(processes):stop_owned_group(process)
            for log in logs:log.close()
            for path in root.rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'cases':cases,'maximum_wire_bytes':8*1024**2,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':not list(folder.rglob('*.graw')),
            'real_hardware':False,'receiver_sha256':sha(args.receiver),'source_fixture_sha256':sha(args.source),'legacy_fixture_sha256':sha(legacy_fixture),'sender_sha256':sha(sender),'runner_sha256':sha(Path(__file__))}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS',len(cases),'actual ICE source/receiver cases; same fixed connection and UNKNOWN protected; raw deleted;',folder)


if __name__=='__main__':main()
