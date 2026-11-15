#!/usr/bin/env python3
"""Actual inherited private-control socket and FDT TCP; never turn controller loss into EOF."""
import argparse
import json
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request
from receiver_transport_integration import frame, port
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def wait(action):
    deadline=time.monotonic()+5
    while time.monotonic()<deadline:
        value=action()
        if value:return value
        time.sleep(.02)
    raise AssertionError('cycle fixture observation timeout')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();binary=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='receiver-cycle-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    cases=[];deleted=[];wire_total=0
    for name in ['finish-active','controller-loss','abort-refused','abort-empty','finish-empty','invalid-request','finish-unused','unused-refused']:
        root=folder/name;root.mkdir();output=root/'output';output.mkdir()
        data_port,status_port=port(),port()
        while status_port==data_port:status_port=port()
        parent,child=socket.socketpair();process=None;data=None
        command=[str(binary),'--output',str(output),'--port',str(data_port),'--status-port',str(status_port),'--input-protocol','FDT','--cycle-control-stdin',
                 '--session-id','d'*64,'--sender-id','C0','--configuration-sha256','a'*64,'--artifact-sha256','b'*64,'--queue-mib','1','--max-frame-mib','1']
        expected=b''
        try:
            with (root/'stdout.log').open('w') as stdout,(root/'stderr.log').open('w') as stderr:
                process=subprocess.Popen(command,stdin=child.fileno(),stdout=stdout,stderr=stderr,start_new_session=True);child.close()
                def status():
                    if process.poll() is not None:return None
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:'+str(status_port)+'/status',timeout=.2) as response:return json.load(response)
                    except OSError:return None
                initial=wait(status);assert initial['cycle_control']=='stdin-socket-v1' and not initial['once'] and initial['cycle_control_connected']
                if name in ['finish-active','controller-loss','abort-refused','unused-refused']:
                    data=socket.create_connection(('127.0.0.1',data_port),timeout=2)
                    expected=frame(1);payload=bytes(4)+expected;data.sendall(payload);wire_total+=len(payload)
                    wait(lambda:(lambda s:s if s and s['connected'] and s['frames']==1 else None)(status()))
                    if name=='unused-refused':
                        parent.sendall(b'E');wait(lambda:(lambda s:s if s and s['cycle_unused_refused'] else None)(status()));assert process.poll() is None
                    if name=='abort-refused':
                        parent.sendall(b'A');wait(lambda:(lambda s:s if s and s['cycle_abort_refused'] else None)(status()));assert process.poll() is None
                    if name=='controller-loss':
                        parent.close();wait(lambda:(lambda s:s if s and not s['cycle_control_connected'] else None)(status()))
                    else:
                        parent.sendall(b'F');wait(lambda:(lambda s:s if s and s['cycle_finish_requested'] else None)(status()))
                        assert process.poll() is None and not (output/'.getdaq-receiver-seal.json').exists()
                    data.sendall(bytes([255])*4);wire_total+=4;data.shutdown(socket.SHUT_WR);data.close();data=None
                    if name=='controller-loss':
                        wait(lambda:(lambda s:s if s and s['sender_eof_confirmed'] and not s['connected'] else None)(status()))
                        assert process.poll() is None and not (output/'.getdaq-receiver-seal.json').exists()
                        data=socket.create_connection(('127.0.0.1',data_port),timeout=2)
                        second=frame(2,True);payload=bytes(4)+second+bytes([255])*4;data.sendall(payload);wire_total+=len(payload);expected+=second
                        data.shutdown(socket.SHUT_WR);data.close();data=None
                        last=wait(lambda:(lambda s:s if s and s['connections']==2 and s['frames']==2 and s['sender_eof_confirmed'] else None)(status()))
                        assert not last['stream_sealed'];process.terminate()
                else:parent.sendall({'abort-empty':b'A','finish-empty':b'F','invalid-request':b'X','finish-unused':b'E'}[name])
                code=process.wait(timeout=5)
            final=json.loads((root/'stdout.log').read_text().splitlines()[-1])
            success=name in ['finish-active','abort-refused','unused-refused','finish-unused']
            assert code==(0 if success else 1) and (output/'.getdaq-receiver-seal.json').exists()==success
            raw=b''.join(path.read_bytes() for path in sorted(output.glob('*.graw')));assert raw==expected
            if name=='finish-unused':assert final['source_unused_confirmed'] and not final['sender_eof_confirmed'] and not final['stream_sealed'] and final['connections']==0
            if name=='controller-loss':assert not final['cycle_control_connected'] and 'private finish request' in final['error']
            if name=='abort-empty':assert final['cycle_startup_aborted'] and final['connections']==0 and not final['sender_eof_confirmed']
            if name=='finish-empty':assert 'actual sender EOF' in final['error']
            if name=='invalid-request':assert 'Invalid private cycle control' in final['error']
            cases.append({'case':name,'command':command,'return_code':code,'final_status':final,'raw_bytes':len(raw),'success_seal':success})
        finally:
            if data:data.close()
            parent.close();child.close();stop_owned_group(process)
            for path in root.rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    assert wire_total<1024**2
    receipt={'schema':1,'cases':cases,'wire_bytes':wire_total,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':not list(folder.rglob('*.graw')),
             'receiver_sha256':sha(binary),'runner_sha256':sha(Path(__file__)),'real_hardware':False}
    (folder/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS 8 private cycle control cases; controller-loss recording preserved; raw payload deleted;',folder)


if __name__=='__main__':main()
