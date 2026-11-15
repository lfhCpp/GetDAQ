#!/usr/bin/env python3
"""Actual kernel ENOSPC in a private 8MiB tmpfs, never fill the workspace disk."""
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


def sha(data):return hashlib.sha256(data).hexdigest()


def port():
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));return sock.getsockname()[1]


def frame(event,size):
    data=bytearray([event&255])*size;data[0]=6;data[1:4]=(size//64).to_bytes(3,'big');data[4]=7
    data[5:7]=(1).to_bytes(2,'big');data[7]=4;data[8:10]=(2).to_bytes(2,'big');data[10:12]=(4).to_bytes(2,'big')
    data[12:16]=((size-128)//4).to_bytes(4,'big');data[22:26]=event.to_bytes(4,'big');data[26:28]=bytes(2);return bytes(data)


def blob(source):
    data=bytearray([source])*64;data[0]=0xc0;data[1:4]=(64).to_bytes(3,'little');data[4]=source
    data[5:7]=(0x1234).to_bytes(2,'little');data[7]=0;return bytes(data)


def interrupted(signum,unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('private ENOSPC test interrupted')


def worker(binary,parent):
    marker=json.loads((parent/'fixture.json').read_text())
    assert marker['kind']=='getdaq.private-enospc-fixture' and marker['mount_namespace']!=os.readlink('/proc/self/ns/mnt')
    assert os.geteuid()==0 and parent.name.startswith('receiver-enospc-')
    results=[];total=12*1024**2+20*64;assert total<=20*1024**3
    for case_name,inodes,payloads in [('bytes',64,[frame(i,1024*1024) for i in range(1,13)]),('inodes',12,[blob(i) for i in range(1,21)])]:
        case=parent/case_name;case.mkdir();mountpoint=case/'ramdisk';mountpoint.mkdir()
        process,sender=None,None;mounted=False;cleanup=[]
        data_port,status_port=port(),port()
        try:
            command=['mount','-t','tmpfs','-o','size=8M,nr_inodes=%d,mode=0700'%inodes,'getdaq-enospc',str(mountpoint)]
            subprocess.run(command,capture_output=True,text=True,check=True);mounted=True
            capacity=os.statvfs(mountpoint);assert capacity.f_blocks*capacity.f_frsize==8*1024**2 and capacity.f_files==inodes
            def status():
                with urllib.request.urlopen('http://127.0.0.1:%d/status'%status_port,timeout=1) as response:return json.load(response)
            with (case/'stdout.txt').open('w') as out,(case/'stderr.txt').open('w') as err:
                process=subprocess.Popen([str(binary),'--output',str(mountpoint),'--port',str(data_port),'--status-port',str(status_port),
                                          '--queue-mib','1','--max-frame-mib','1','--rotate-mib','32','--once'],stdout=out,stderr=err)
                deadline=time.monotonic()+10
                while True:
                    assert process.poll() is None,'receiver exited before readiness'
                    try:
                        if status()['service']=='getdaq-receiver':break
                    except (OSError,ValueError):pass
                    assert time.monotonic()<deadline;time.sleep(.02)
                sender=socket.create_connection(('127.0.0.1',data_port),timeout=5)
                sent=0;send_error=''
                try:
                    for payload in payloads:sender.sendall(payload);sent+=len(payload)
                except (BrokenPipeError,ConnectionResetError) as error:send_error=type(error).__name__
                finally:sender.close();sender=None
                process.wait(timeout=15)
            assert process.returncode==1,(case_name,(case/'stderr.txt').read_text())
            final=json.loads((case/'stdout.txt').read_text().strip().splitlines()[-1])
            assert 'No space left on device' in final['error'] and not final['stream_sealed']
            assert not (mountpoint/'.getdaq-receiver-seal.json').exists()
            exhausted=os.statvfs(mountpoint);raw_files=list(mountpoint.glob('*.graw'));assert raw_files
            if case_name=='bytes':
                assert exhausted.f_bavail==0 and exhausted.f_favail>0 and len(raw_files)==1
                expected=b''.join(payloads);actual=raw_files[0].read_bytes()
                assert actual and len(actual)<=8*1024**2 and expected[:len(actual)]==actual
            else:
                assert exhausted.f_favail==0 and exhausted.f_bavail>0
                assert 'Cannot create' in final['error']
                expected={i:payloads[i-1] for i in range(1,21)};actual=[]
                for path in raw_files:
                    raw=path.read_bytes();assert len(raw)==64 and expected[raw[4]]==raw;actual.append(raw)
                actual=b''.join(sorted(actual,key=lambda data:data[4]))
                assert len(actual)<len(b''.join(payloads))
            files=[{'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())} for path in raw_files]
            result={'case':case_name,'return_code':process.returncode,'final_status':final,'files':files,
                    'partial_file_bytes':sum(item['bytes'] for item in files),'sent_complete_payload_bytes':sent,'sender_error':send_error,
                    'tmpfs_capacity_bytes':capacity.f_blocks*capacity.f_frsize,'tmpfs_inode_limit':inodes,
                    'free_bytes_at_failure':exhausted.f_bavail*exhausted.f_frsize,'free_inodes_at_failure':exhausted.f_favail,
                    'actual_kernel_enospc':True,'no_fault_interposer':True,'shared_disk_filled':False}
            results.append(result);print('PASS actual ENOSPC',case_name,flush=True)
        finally:
            if sender:sender.close()
            if process and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=3)
            if mounted:
                for path in mountpoint.glob('*.graw'):
                    cleanup.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())});path.unlink()
                (case/'payload-cleanup.json').write_text(json.dumps({'deleted':cleanup,'raw_files_remaining':len(list(mountpoint.glob('*.graw')))},indent=2)+'\n')
                subprocess.run(['umount',str(mountpoint)],capture_output=True,text=True,check=True)
            assert not list(mountpoint.iterdir()),'private mount not empty after unmount'
            mountpoint.rmdir()
    result={'schema':1,'receiver_sha256':sha(binary.read_bytes()),'runner_sha256':sha(Path(__file__).read_bytes()),'cases':results,
            'aggregate_maximum_wire_bytes':total,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':True,
            'private_mount_namespace':True,'real_hardware':False,'power_loss_tested':False}
    (parent/'result.json').write_text(json.dumps(result,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path);parser.add_argument('--inside-namespace',action='store_true')
    args=parser.parse_args();binary=args.receiver.resolve();signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    if args.inside_namespace:worker(binary,args.evidence.resolve());return
    args.evidence.mkdir(parents=True,exist_ok=True);parent=Path(tempfile.mkdtemp(prefix='receiver-enospc-',dir=args.evidence)).resolve()
    (parent/'fixture.json').write_text(json.dumps({'kind':'getdaq.private-enospc-fixture','mount_namespace':os.readlink('/proc/self/ns/mnt'),'receiver_sha256':sha(binary.read_bytes())},indent=2)+'\n')
    command=['unshare','--user','--map-root-user','--mount','python3',str(Path(__file__).resolve()),str(binary),str(parent),'--inside-namespace']
    process=None
    try:
        with (parent/'worker.log').open('w') as log:process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);code=process.wait(timeout=60)
        assert code==0,(parent/'worker.log').read_text()
        assert not list(parent.rglob('*.graw')) and (parent/'result.json').exists()
    finally:
        if process:
            try:os.killpg(process.pid,signal.SIGTERM)
            except ProcessLookupError:pass
            if process.poll() is None:
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    try:os.killpg(process.pid,signal.SIGKILL)
                    except ProcessLookupError:pass
                    process.wait(timeout=3)
            else:
                try:os.killpg(process.pid,0)
                except ProcessLookupError:pass
                else:os.killpg(process.pid,signal.SIGKILL)
    print('PASS private actual ENOSPC: byte/inode exhaustion; <=8MiB raw per case, deleted; no shared disk filled;',parent)


if __name__=='__main__':main()
