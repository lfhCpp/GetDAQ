#!/usr/bin/env python3
"""Inject bounded private-output I/O failures; this is not a real media/power-loss test."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def port():
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));return sock.getsockname()[1]


def frame(event,size=4096):
    data=bytearray([event&255])*size;data[0]=6;data[1:4]=(size//64).to_bytes(3,'big');data[4]=7
    data[5:7]=(1).to_bytes(2,'big');data[7]=4;data[8:10]=(2).to_bytes(2,'big');data[10:12]=(4).to_bytes(2,'big')
    data[12:16]=((size-128)//4).to_bytes(4,'big');data[22:26]=event.to_bytes(4,'big');data[26:28]=bytes(2);return bytes(data)


def interrupted(signum,unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('I/O injection test interrupted')


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path);parser.add_argument('--sanitized',action='store_true')
    args=parser.parse_args();binary=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    parent=Path(tempfile.mkdtemp(prefix='receiver-io-fault-',dir=args.evidence)).resolve()
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    library=parent/'receiver-io-faults.so';source=root/'tests/receiver_io_faults.c'
    command=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',str(source),'-o',str(library)]
    with (parent/'build-interposer.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    small=frame(1)+frame(2)+frame(3)
    cases=[('buffered-zero','write-zero',small,'Buffered file write returned zero bytes',False),
           ('direct-zero','write-zero',frame(1,768*1024),'File write returned zero bytes',False),
           ('buffered-eio','write-eio',small,'Buffered file write failed: Input/output error',False),
           ('raw-sync-eio','fsync-raw-eio',small,'fsync failed: Input/output error',False),
           ('directory-sync-eio','fsync-directory-eio',small,'Directory fsync failed: Input/output error',False),
           ('seal-sync-eio','fsync-seal-eio',small,'Receiver seal fsync failed: Input/output error',False),
           ('seal-close-eio','close-seal-eio',small,'Receiver seal close failed: Input/output error',False),
           ('seal-rename-eperm','rename-seal-eperm',small,'Receiver seal rename failed: Operation not permitted',False),
           ('published-directory-sync-eio','fsync-published-directory-eio',small,'Receiver seal directory fsync failed: Input/output error',False),
           ('interrupted-write-retry','write-eintr-once',small,'',True)]
    total=sum(len(data) for unused,unused2,data,unused3,unused4 in cases);assert total<=20*1024**3
    records=[]
    for name,mode,data,error,success in cases:
        case=parent/name;case.mkdir();output=case/'output';output.mkdir();process=None;sender=None;deleted=[]
        data_port,status_port=port(),port();env=dict(os.environ,GETDAQ_TEST_IO_DIRECTORY=str(output),GETDAQ_TEST_IO_MODE=mode)
        preload=[str(library)]
        if args.sanitized:
            asan=subprocess.check_output(['c++','-print-file-name=libasan.so'],text=True).strip();assert Path(asan).is_file()
            preload.insert(0,asan);env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        env['LD_PRELOAD']=':'.join(preload)
        try:
            with (case/'stdout.txt').open('w') as out,(case/'stderr.txt').open('w') as err:
                process=subprocess.Popen([str(binary),'--output',str(output),'--port',str(data_port),'--status-port',str(status_port),'--queue-mib','1','--max-frame-mib','1','--once'],stdout=out,stderr=err,env=env)
                deadline=time.monotonic()+10
                while True:
                    assert process.poll() is None,(case/'stderr.txt').read_text()
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:%d/status'%status_port,timeout=1) as response:
                            if json.load(response)['service']=='getdaq-receiver':break
                    except (OSError,ValueError):pass
                    assert time.monotonic()<deadline;time.sleep(.02)
                sender=socket.create_connection(('127.0.0.1',data_port),timeout=5)
                try:sender.sendall(data)
                except (BrokenPipeError,ConnectionResetError):pass
                sender.close();sender=None;process.wait(timeout=15)
            assert process.returncode==(0 if success else 1),(name,(case/'stderr.txt').read_text())
            final=json.loads((case/'stdout.txt').read_text().strip().splitlines()[-1])
            assert final['stream_sealed']==success and bool(final['error'])==(not success)
            if not success:assert error in final['error'] and 'failed: Success' not in final['error']
            seal=output/'.getdaq-receiver-seal.json';assert seal.exists()==success
            files=list(output.glob('*.graw'));assert len(files)==1;actual=files[0].read_bytes()
            expected=b'' if mode in ('write-zero','write-eio') else data
            assert actual==expected,(name,'actual file bytes differ')
            if success:assert final['received_bytes']==final['accepted_file_bytes']==final['sealed_bytes']==len(data) and final['sender_eof_confirmed']
            result={'case':name,'mode':mode,'expected_success':success,'return_code':process.returncode,'final_status':final,
                    'raw_bytes':len(actual),'raw_sha256':hashlib.sha256(actual).hexdigest(),'sent_bytes':len(data),
                    'simulated_io_failure':not success,'actual_device_failure':False,'seal_published':seal.exists()}
            records.append(result);print('PASS',name)
        finally:
            if sender:sender.close()
            if process and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=3)
            for path in output.glob('*.graw'):
                deleted.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (case/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'raw_files_remaining':len(list(output.glob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'sanitized':args.sanitized,'receiver_sha256':sha(binary),'runner_sha256':sha(Path(__file__)),'interposer_source_sha256':sha(source),'interposer_sha256':sha(library),
            'commands':[command],'cases':records,'aggregate_wire_bytes':total,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':not list(parent.rglob('*.graw')),'actual_device_failure':False,'power_loss_tested':False}
    (parent/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS private I/O injection:',len(cases),'cases;',total,'wire bytes; raw files deleted;',parent)


if __name__=='__main__':main()
