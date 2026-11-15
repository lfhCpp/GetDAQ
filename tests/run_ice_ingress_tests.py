#!/usr/bin/env python3
"""Actual optional ICE data ingress; every test payload is bounded and deleted."""
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
    deadline=time.monotonic()+6
    while time.monotonic()<deadline:
        value=action()
        if value:return value
        time.sleep(.02)
    raise AssertionError('ICE ingress fixture observation timeout')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver',type=Path);parser.add_argument('fixture',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();args.receiver=args.receiver.resolve();args.fixture=args.fixture.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='ice-ingress-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    modes=[('complete',0),('complete',1),('byte-storage',1),('export',1),('empty',1),('legacy',0),('peer-close',1),('wrong-session',0),
           ('wrong-connection',1),('wrong-count',1),('truncated',0),('oversized-frame',1),('second-source',0),
           ('message-limit',1),('reconnect-count',0)]
    maximum_wire_bytes=32*1024**2
    assert maximum_wire_bytes < 20*1024**3
    cases=[];deleted=[]
    for mode,minor in modes:
        name=mode+'-'+str(minor);root=folder/name;output=root/'output';output.mkdir(parents=True)
        data_port,status_port=port(),port()
        while status_port==data_port:status_port=port()
        command=[str(args.receiver),'--output',str(output),'--session-id','d'*64,'--port',str(data_port),
                 '--status-port',str(status_port),'--queue-mib','1','--max-frame-mib','1','--message-mib','1','--min-free-mib','1']
        process=None;client=None;export=None;exported=b''
        expected=(bytes((i*37+11)&255 for i in range(65537)) if mode=='byte-storage' else
                  b'' if mode in ['empty','oversized-frame','message-limit','reconnect-count'] else frame(1)+frame(2,True))
        success=mode in ['complete','byte-storage','export','empty','wrong-session','wrong-connection','wrong-count']
        if mode=='byte-storage': command+=['--storage-mode','byte-storage']
        if mode=='export':
            export_port=port()
            while export_port in [data_port,status_port]:export_port=port()
            command+=['--export-port',str(export_port),'--export-queue-mib','1']
        try:
            with (root/'stdout.log').open('w') as stdout,(root/'stderr.log').open('w') as stderr:
                process=subprocess.Popen(command,stdout=stdout,stderr=stderr,start_new_session=True)
                def status():
                    if process.poll() is not None:return None
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:'+str(status_port)+'/status',timeout=.25) as response:return json.load(response)
                    except OSError:return None
                initial=wait(status);assert not initial['sender_eof_confirmed'] and not initial['sender_end_confirmed']
                assert initial['minimum_free_bytes']==1024*1024,'ICE receiver did not apply the configured disk reserve'
                if mode=='export':
                    export=socket.create_connection(('127.0.0.1',export_port),timeout=2)
                    wait(lambda:(lambda value:value if value and value['export_clients']==1 else None)(status()))
                with (root/'client.log').open('w') as log:
                    client=subprocess.Popen([str(args.fixture),'127.0.0.1',str(data_port),'d'*64,mode,str(minor)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=client.wait(timeout=12)
                if export:
                    while True:
                        block=export.recv(65536)
                        if not block:break
                        exported+=block
                    export.close();export=None
                    packed=(4096).to_bytes(8,'little')+frame(1)+(4096).to_bytes(8,'little')+frame(2,True)
                    assert exported==packed,'ICE input export did not preserve ordinary length/raw format'
                assert code==0 or mode in ['oversized-frame','truncated'],(name,code,(root/'client.log').read_text())
                if mode in ['legacy','peer-close','message-limit','reconnect-count']:
                    seen=wait(lambda:(lambda value:value if value and value['received_bytes']==len(expected) and value['frames']==(2 if expected else 0) else None)(status()))
                    assert process.poll() is None and not seen['sender_end_confirmed'] and not seen['sender_eof_confirmed']
                    assert not (output/'.getdaq-ice-receiver-seal.json').exists()
                    process.terminate() # Fixture cleanup only; never a source end marker.
                exit_code=process.wait(timeout=8)
            lines=(root/'stdout.log').read_text().splitlines();final=json.loads(lines[-1]);receipt_path=output/'.getdaq-ice-receiver-seal.json'
            assert exit_code==(0 if success else 1),(name,exit_code,(root/'stderr.log').read_text())
            assert not final['sender_eof_confirmed'] and receipt_path.exists()==success
            assert not (output/'.getdaq-receiver-seal.json').exists(),'ICE marker became a TCP/FDT seal'
            raw=b''.join(p.read_bytes() for p in sorted(output.glob('*.graw')));assert raw==expected,(name,len(raw),len(expected))
            if success:
                receipt=json.loads(receipt_path.read_text());assert receipt['kind']=='getdaq.ice-receiver-seal' and receipt['sender_end_confirmed'] and not receipt['sender_eof_confirmed']
                assert receipt['storage_mode']==('byte-storage' if mode=='byte-storage' else 'frame-storage')
                assert int(receipt['received_bytes'])==len(expected) and final['received_bytes']==final['accepted_file_bytes']==final['synced_bytes']==len(expected)
                assert final['completion_receipt_published'] and final['writer_done'] and final['bound_connections']==1
            cases.append({'case':name,'client_exit':code,'receiver_exit':exit_code,'success':success,'raw_bytes':len(raw),'raw_sha256':sha_bytes(raw),'final_status':final})
        finally:
            if export:export.close()
            stop_owned_group(client);stop_owned_group(process)
            for path in root.rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'cases':cases,'maximum_wire_bytes':maximum_wire_bytes,'aggregate_limit_bytes':20*1024**3,
            'generated_raw_deleted':not list(folder.rglob('*.graw')),'real_hardware':False,
            'receiver_sha256':sha(args.receiver),'fixture_sha256':sha(args.fixture),'runner_sha256':sha(Path(__file__))}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS',len(cases),'actual ICE ingress cases; legacy data accepted without fake completion; raw payload deleted;',folder)


def sha_bytes(value):
    import hashlib
    return hashlib.sha256(value).hexdigest()


if __name__=='__main__':main()
