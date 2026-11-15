#!/usr/bin/env python3
"""Verify GET-style raw filenames via real TCP; always delete synthetic payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request


def sha(data):return hashlib.sha256(data).hexdigest()


def port():
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));return sock.getsockname()[1]


def frame(event,size=4096,asad=0,little=False):
    order='little' if little else 'big';data=bytearray([event&255])*size
    data[0]=6|(128 if little else 0);data[1:4]=(size//64).to_bytes(3,order);data[4]=7
    data[5:7]=(1).to_bytes(2,order);data[7]=4;data[8:10]=(2).to_bytes(2,order);data[10:12]=(4).to_bytes(2,order)
    data[12:16]=((size-128)//4).to_bytes(4,order);data[22:26]=event.to_bytes(4,order);data[26:28]=bytes([0,asad]);return bytes(data)


def blob(kind,event=0):
    data=bytearray(64);data[0]=0xc0;data[1:4]=(64).to_bytes(3,'little');data[4]=7
    data[5:7]=kind.to_bytes(2,'little');data[14:18]=event.to_bytes(4,'little');return bytes(data)


def interrupted(signum,unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('filename test interrupted')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();binary=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='receiver-file-naming-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    hb,bye=bytes(4),bytes([255])*4
    a,b,c,d,e=[frame(event) for event in (10,11,1,1,2)];p,q=frame(20,asad=1),frame(21,asad=1,little=True)
    mutant,unknown=blob(8,3),blob(0x1234)
    rotating=[frame(i,1024*1024) for i in range(1,14)]
    cases=[('get-mixed-sources','TCP',[a+p+b+q+c+mutant+d+unknown+e],{'CoBo0_AsAd0':[a+b,c,d+e],'CoBo0_AsAd1':[p+q],'MuTanT':[mutant],'MFM4660_Source7':[unknown]}),
           ('get-size-rotation','TCP',[b''.join(rotating)],{'CoBo0_AsAd0':rotating}),
           ('get-reconnect','TCP',[a,a],{'CoBo0_AsAd0':[a,a]}),
           ('get-fdt','FDT',[hb+a+hb+b+bye],{'CoBo0_AsAd0':[a+b]})]
    total=sum(len(wire) for unused,unused2,connections,unused3 in cases for wire in connections)
    assert total<=20*1024**3 and shutil.disk_usage(folder).free>=total+64*1024**2
    results=[]
    for name,protocol,connections,expected in cases:
        case=folder/name;case.mkdir();data_port,status_port=port(),port();process=None;sender=None;deleted=[]
        def status():
            with urllib.request.urlopen('http://127.0.0.1:%d/status'%status_port,timeout=1) as response:return json.load(response)
        def wait(predicate):
            deadline=time.monotonic()+15
            while time.monotonic()<deadline:
                assert process.poll() is None,'receiver exited before expected status'
                try:
                    value=status()
                    if predicate(value):return value
                except (OSError,ValueError):pass
                time.sleep(.02)
            raise AssertionError('filename status timeout')
        try:
            with (case/'stdout.txt').open('w') as out,(case/'stderr.txt').open('w') as err:
                command=[str(binary),'--output',str(case),'--port',str(data_port),'--status-port',str(status_port),
                         '--file-naming','get','--input-protocol',protocol,'--rotate-mib','1','--max-frame-mib','1','--queue-mib','1']
                if len(connections)==1:command+=['--once']
                process=subprocess.Popen(command,stdout=out,stderr=err)
                wait(lambda value:value.get('file_naming')=='get')
                for index,wire in enumerate(connections):
                    sender=socket.create_connection(('127.0.0.1',data_port),timeout=10)
                    for pos in range(0,len(wire),65537):sender.sendall(wire[pos:pos+65537])
                    sender.close();sender=None
                    if len(connections)>1:wait(lambda value:value['connections']==index+1 and value['stream_sealed'])
                if len(connections)>1:process.terminate()
                process.wait(timeout=15)
            assert process.returncode==0,(name,(case/'stderr.txt').read_text())
            final=json.loads((case/'stdout.txt').read_text().strip().splitlines()[-1])
            expected_bytes=sum(len(raw) for files in expected.values() for raw in files)
            assert final['file_naming']=='get' and final['received_bytes']==final['accepted_file_bytes']==final['sealed_bytes']==expected_bytes
            assert final['stream_sealed'] and final['sender_eof_confirmed'] and not final['error']
            files_by_source={};stamps=set();file_records=[]
            pattern=re.compile(r'(.+)_([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3})_([0-9]{4,})\.graw')
            for path in case.glob('*.graw'):
                match=pattern.fullmatch(path.name);assert match,(name,path.name)
                source,stamp,index=match.groups();stamps.add(stamp);counter=int(index)
                assert index==str(counter).zfill(4)
                files_by_source.setdefault(source,{})[counter]=path.read_bytes()
                file_records.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())})
            assert set(files_by_source)==set(expected) and len(stamps)==1
            for source,raw_files in expected.items():
                assert sorted(files_by_source[source])==list(range(len(raw_files)))
                assert [files_by_source[source][i] for i in range(len(raw_files))]==raw_files,(name,source)
            assert final['files']==len(file_records)
            seal=json.loads((case/'.getdaq-receiver-seal.json').read_text());assert seal['complete'] and seal['durable'] and int(seal['received_bytes'])==expected_bytes
            assert {item['name']:int(item['bytes']) for item in seal['files']}=={item['name']:item['bytes'] for item in file_records}
            result={'case':name,'wire_bytes':sum(map(len,connections)),'raw_bytes':expected_bytes,'files':file_records,'final_status':final}
            results.append(result);print('PASS',name,len(file_records),'exact files')
        finally:
            if sender:sender.close()
            if process and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            for path in case.glob('*.graw'):
                deleted.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())});path.unlink()
            (case/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'raw_files_remaining':len(list(case.glob('*.graw')))},indent=2)+'\n')
    invalid=subprocess.run([str(binary),'--output',str(folder),'--file-naming','GET'],capture_output=True,text=True)
    assert invalid.returncode==2 and 'File naming must' in invalid.stderr
    result={'schema':1,'receiver_sha256':sha(binary.read_bytes()),'runner_sha256':sha(Path(__file__).read_bytes()),'cases':results,
            'aggregate_wire_bytes':total,'aggregate_limit_bytes':20*1024**3,'real_hardware':False,'generated_raw_deleted':not list(folder.rglob('*.graw'))}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS filename TCP tests:',total,'wire bytes; raw files deleted;',folder)


if __name__=='__main__':main()
