#!/usr/bin/env python3
"""Bounded export reconnection checks; no production or retained raw data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request


def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0))
        return sock.getsockname()[1]


def sha(data): return hashlib.sha256(data).hexdigest()


def frame():
    data=bytearray([123])*4096
    data[0]=6; data[1:4]=(64).to_bytes(3,'big');data[4]=7
    data[5:7]=(1).to_bytes(2,'big');data[7]=4
    data[8:10]=(2).to_bytes(2,'big');data[10:12]=(4).to_bytes(2,'big')
    data[12:16]=(992).to_bytes(4,'big');data[22:26]=(1).to_bytes(4,'big');data[26:28]=bytes(2)
    return bytes(data)


def interrupt(signum, unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('export reconnection test interrupted')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    parser.add_argument('--legacy-demonstration',action='store_true')
    args=parser.parse_args();binary=args.receiver.resolve()
    args.evidence.mkdir(parents=True,exist_ok=True)
    assert shutil.disk_usage(args.evidence).free>=64*1024**2
    folder=Path(tempfile.mkdtemp(prefix='export-reconnect-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupt);signal.signal(signal.SIGTERM,interrupt)
    data_port,status_port,export_port=port(),port(),port()
    process=None;clients=[];records=[];result=None
    def status():
        with urllib.request.urlopen('http://127.0.0.1:%d/status'%status_port,timeout=1) as response:return json.load(response)
    def wait(predicate):
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            assert process.poll() is None,'receiver exited before expected status'
            try:
                value=status()
                if predicate(value):return value
            except (OSError,ValueError):pass
            time.sleep(.02)
        raise AssertionError('export status timeout')
    def connect():
        client=socket.create_connection(('127.0.0.1',export_port),timeout=3);clients.append(client);return client
    def close(client):
        client.close();clients.remove(client)
    def exact(client,count):
        result=bytearray()
        while len(result)<count:
            chunk=client.recv(count-len(result));assert chunk,'early export EOF';result.extend(chunk)
        return bytes(result)
    def ended(client):
        try: assert client.recv(1)==b'', 'terminated subscription produced unexpected bytes'
        except ConnectionResetError: pass # 未读客户端输入会使内核用RST结束连接。
    try:
        with (folder/'stdout.txt').open('w') as out,(folder/'stderr.txt').open('w') as err:
            process=subprocess.Popen([str(binary),'--output',str(folder),'--port',str(data_port),
                '--status-port',str(status_port),'--export-port',str(export_port),
                '--queue-mib','1','--max-frame-mib','1','--once'],stdout=out,stderr=err)
            wait(lambda value:value['service']=='getdaq-receiver')
            if args.legacy_demonstration:
                for i in range(8):
                    client=connect();wait(lambda value:value['export_clients']==i+1);close(client)
                time.sleep(.2)
                stale=status();assert stale['export_clients']==8 and stale['received_bytes']==0
                rejected=connect();ended(rejected);close(rejected)
                final=wait(lambda value:value['export_disconnections']>=1)
                assert final['export_clients']==8
                process.terminate();process.wait(timeout=10)
                result={'legacy_idle_slot_exhaustion_confirmed':True,'status':final,'raw_bytes':0}
                print('PASS original idle disconnects exhaust all eight slots before the next frame')
            else:
                # Repeat idle connections without creating any data connection or frame.
                for i in range(20):
                    before=status()['export_disconnections'];client=connect()
                    wait(lambda value:value['export_clients']==1);close(client)
                    idle=wait(lambda value:value['export_clients']==0 and value['export_disconnections']==before+1)
                    assert idle['received_bytes']==idle['frames']==0
                records.append({'check':'20 idle disconnect/reconnect cycles','status':idle})
                active=[connect() for unused in range(8)]
                full=wait(lambda value:value['export_clients']==8)
                extra=connect();ended(extra);close(extra)
                rejected=wait(lambda value:value['export_disconnections']==full['export_disconnections']+1)
                assert rejected['export_clients']==8
                for client in active:close(client)
                empty=wait(lambda value:value['export_clients']==0)
                records.append({'check':'eight client limit and recovery','status':empty})
                # Export is receive-only. Write-side EOF and unsolicited bytes end subscription.
                for action in ('write-eof','unsolicited-data'):
                    before=status()['export_disconnections'];client=connect()
                    wait(lambda value:value['export_clients']==1)
                    if action=='write-eof':client.shutdown(socket.SHUT_WR)
                    else:client.sendall(b'x')
                    ended(client);close(client)
                    value=wait(lambda value:value['export_clients']==0 and value['export_disconnections']==before+1)
                    records.append({'check':action,'status':value})
                watcher=connect();wait(lambda value:value['export_clients']==1)
                payload=frame();assert len(payload)<=20*1024**3
                with socket.create_connection(('127.0.0.1',data_port),timeout=3) as sender:sender.sendall(payload)
                count=int.from_bytes(exact(watcher,8),'little')
                assert count==len(payload) and exact(watcher,count)==payload
                process.wait(timeout=10)
                assert process.returncode==0,(folder/'stderr.txt').read_text()
                final=json.loads((folder/'stdout.txt').read_text().strip().splitlines()[-1])
                assert final['stream_sealed'] and final['sender_eof_confirmed'] and not final['error']
                assert final['frames']==1 and final['received_bytes']==final['sealed_bytes']==len(payload)
                actual=b''.join(path.read_bytes() for path in folder.glob('*.graw'));assert actual==payload
                seal=json.loads((folder/'.getdaq-receiver-seal.json').read_text())
                assert seal['complete'] and int(seal['received_bytes'])==len(payload)
                result={'checks':records,'final_status':final,'raw_bytes':len(payload),'raw_sha256':sha(payload),
                        'export_received_bytes':count+8}
                print('PASS idle reconnects, eight-client recovery, client protocol termination and exact recording/export')
    finally:
        for client in clients:client.close()
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        deleted=[]
        for path in folder.glob('*.graw'):
            deleted.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())});path.unlink()
        cleanup={'deleted':deleted,'raw_files_remaining':len(list(folder.glob('*.graw')))}
        (folder/'payload-cleanup.json').write_text(json.dumps(cleanup,indent=2)+'\n')
    result.update({'receiver_sha256':sha(binary.read_bytes()),'runner_sha256':sha(Path(__file__).read_bytes()),
                   'legacy_demonstration':args.legacy_demonstration,'real_hardware':False,
                   'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':cleanup['raw_files_remaining']==0})
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Evidence:',folder)


if __name__=='__main__':main()
