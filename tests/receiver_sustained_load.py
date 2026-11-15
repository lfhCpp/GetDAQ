#!/usr/bin/env python3
"""多进程原生接收器持续写盘测量；只生成合成CoBo帧，不连接硬件。"""
import argparse
import concurrent.futures
import fcntl
import hashlib
import json
import os
from pathlib import Path
import secrets
import selectors
import shutil
import signal
import socket
import stat
import struct
import subprocess
import tempfile
import time


MAX_TOTAL_BYTES = 20 * 1024**3
DISK_RESERVE_BYTES = 1024**3


def concurrency_lock_path():
    """Use the caller's writable temporary directory, never the source tree."""
    checkout = Path(__file__).resolve().parents[1]
    key = hashlib.sha256(os.fsencode(str(checkout))).hexdigest()[:16]
    return Path(tempfile.gettempdir()) / ('getdaq-receiver-load-' + str(os.getuid()) + '-' + key + '.lock')


def acquire_concurrency_lock():
    """Open a private, stable lock inode without following or sharing links."""
    lock_path = concurrency_lock_path()
    required_flags = ('O_NOFOLLOW', 'O_CLOEXEC')
    if any(not hasattr(os, flag) for flag in required_flags):
        raise RuntimeError('platform cannot safely open the receiver load lock')
    flags = os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | os.O_CLOEXEC
    try:
        descriptor = os.open(str(lock_path), flags, 0o600)
    except OSError as error:
        raise RuntimeError('cannot safely open receiver load lock: ' + str(error))
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise RuntimeError('receiver load lock is not a regular file')
        if metadata.st_uid != os.getuid() or metadata.st_nlink != 1:
            raise RuntimeError('receiver load lock has an unexpected owner or link count')
        os.fchmod(descriptor, 0o600)
        lock_file = os.fdopen(descriptor, 'r+')
        descriptor = -1
        try:
            fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            lock_file.close()
            raise
        return lock_file
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def remove_generated_payload(root):
    """Only remove payload in this invocation's freshly created private directory."""
    removed = []
    for folder in root.glob('stream-*'):
        if folder.is_symlink() or not folder.is_dir():
            raise RuntimeError('Refusing unexpected stream directory: ' + str(folder))
        for raw in folder.glob('*.graw'):
            if raw.is_symlink() or not raw.is_file():
                raise RuntimeError('Refusing unexpected payload file: ' + str(raw))
            size = raw.stat().st_size
            raw.unlink()
            removed.append({'path': str(raw.relative_to(root)), 'bytes': size})
    receipt = {'schema': 1, 'automatic_cleanup': True,
               'deleted_bytes': sum(entry['bytes'] for entry in removed),
               'files': removed, 'remaining_generated_payload_files': 0}
    (root / 'payload-cleanup.json').write_text(json.dumps(receipt, indent=2) + '\n')


def stop_for_signal(signum, frame):
    raise KeyboardInterrupt('Load test interrupted by signal ' + str(signum))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver',type=Path);parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--streams',type=int,default=2);parser.add_argument('--mib-per-stream',type=int,default=512)
    parser.add_argument('--rotate-mib',type=int,default=1024,help='Rotate generated raw files at this MiB threshold')
    parser.add_argument('--loader',type=Path,help='Optional target userspace dynamic loader for the receiver binary')
    parser.add_argument('--library-path',help='Colon-separated target userspace library directories; requires --loader')
    parser.add_argument('--prune-payload',action='store_true',help='Compatibility option; generated raw files are now always removed')
    args=parser.parse_args()
    if not 1 <= args.streams <= 8 or args.mib_per_stream < 1:
        parser.error('streams must be 1..8 and mib-per-stream must be positive')
    if not 1 <= args.rotate_mib <= MAX_TOTAL_BYTES // 1024**2:
        parser.error('rotate-mib must be 1..20480')
    total_bytes = args.streams * args.mib_per_stream * 1024**2
    if total_bytes > MAX_TOTAL_BYTES:
        parser.error('aggregate generated payload across ALL streams must not exceed 20 GiB')
    program=args.receiver.resolve()
    if not program.is_file() or not os.access(program, os.X_OK):
        parser.error('receiver must be an executable file')
    if bool(args.loader) != bool(args.library_path):
        parser.error('--loader and --library-path must be supplied together')
    launch_prefix=[]
    loader_digest=None
    if args.loader:
        loader=args.loader.resolve()
        if not loader.is_file() or not os.access(loader,os.X_OK):
            parser.error('loader must be an executable file')
        library_dirs=args.library_path.split(':')
        if not library_dirs or any(not item or not Path(item).is_dir() for item in library_dirs):
            parser.error('every --library-path entry must be an existing directory')
        launch_prefix=[str(loader),'--library-path',args.library_path]
        loader_digest=hashlib.sha256(loader.read_bytes()).hexdigest()
    # Keep concurrent invocations in this checkout from adding their disk budgets.
    try:
        lock_file = acquire_concurrency_lock()
    except BlockingIOError:
        parser.error('another receiver load test is running; run tests sequentially')
    except (OSError, RuntimeError) as error:
        parser.error(str(error))
    args.evidence.mkdir(parents=True,exist_ok=True)
    if shutil.disk_usage(args.evidence).free < total_bytes + DISK_RESERVE_BYTES:
        lock_file.close()
        parser.error('not enough free disk for the requested payload plus 1 GiB reserve')
    root=Path(tempfile.mkdtemp(prefix='receiver-load-',dir=args.evidence)).resolve();children=[]
    previous_handlers = {sig: signal.signal(sig, stop_for_signal)
                         for sig in (signal.SIGINT, signal.SIGTERM)}
    def start(index):
        folder=root/('stream-'+str(index));folder.mkdir(mode=0o700)
        for attempt in range(5):
            port=30000+secrets.randbelow(25000)
            p=subprocess.Popen(launch_prefix+[str(program),'--output',str(folder),'--bind','127.0.0.1','--port',str(port),
                '--status-port','0','--queue-mib','8','--max-frame-mib','1','--rotate-mib',str(args.rotate_mib),'--once'],
                stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
            children.append(p)
            observer=selectors.DefaultSelector();observer.register(p.stdout,selectors.EVENT_READ)
            ready=observer.select(10);line=p.stdout.readline() if ready else '';observer.close()
            if line.startswith('Listening data '):
                return folder,p,port,line
            p.terminate();out,err=p.communicate(timeout=10)
            if 'Cannot bind' not in err and 'Address already in use' not in err and 'bind' not in err:raise RuntimeError((line,out,err))
        raise RuntimeError('No listener after explicit collision retries')
    frame=4096;batchFrames=64;batchSize=frame*batchFrames;target=args.mib_per_stream*1024*1024
    def send(index,port):
        block=bytearray([0xa5])*batchSize
        for at in range(0,batchSize,frame):
            block[at]=6;block[at+1:at+4]=(frame//64).to_bytes(3,'big');block[at+4]=7
            struct.pack_into('>HBHHI',block,at+5,1,4,2,4,(frame-128)//4)
            block[at+26]=index;block[at+27]=0
        digest=hashlib.sha256();event=1;sent=0;started=time.monotonic()
        with socket.create_connection(('127.0.0.1',port),timeout=600) as connection:
            while sent<target:
                length=min(batchSize,target-sent)
                for at in range(0,length,frame):struct.pack_into('>I',block,at+22,event);event+=1
                view=memoryview(block)[:length];digest.update(view);connection.sendall(view);sent+=length
        return {'bytes':sent,'frames':event-1,'sha256':digest.hexdigest(),'sender_seconds':time.monotonic()-started,'sender_eof_time':time.monotonic()}
    started=time.monotonic();records=[];executor=None
    try:
        sessions=[start(i) for i in range(args.streams)]
        executor=concurrent.futures.ThreadPoolExecutor(max_workers=args.streams)
        jobs=[executor.submit(send,i,session[2]) for i,session in enumerate(sessions)]
        sent=[job.result(timeout=1200) for job in jobs]
        (root/'sender-results.json').write_text(json.dumps(sent,indent=2)+'\n')
        for index,(folder,p,port,startup) in enumerate(sessions):
            pid,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
            completed=time.monotonic();out,err=p.communicate(timeout=10)
            (folder/'receiver-output.txt').write_text(startup+out+err)
            assert p.returncode==0,(p.returncode,out[-2000:],err[-2000:])
            final=json.loads(out.strip().splitlines()[-1]);seal=json.loads((folder/'.getdaq-receiver-seal.json').read_text())
            expected=sent[index];assert final['received_bytes']==final['accepted_file_bytes']==final['sealed_bytes']==expected['bytes']
            assert final['frames']==expected['frames'] and final['sender_eof_confirmed'] and final['stream_sealed']
            assert seal['complete'] and seal['durable'] and seal['sender_eof_confirmed'] and int(seal['received_bytes'])==expected['bytes']
            digest=hashlib.sha256();files=[];sealFiles={x['name']:int(x['bytes']) for x in seal['files']}
            for raw in sorted(folder.glob('*.graw'),key=lambda p:int(p.stem.rsplit('_',1)[1])):
                assert raw.stat().st_size==sealFiles[raw.name]
                fileDigest=hashlib.sha256()
                with raw.open('rb') as source:
                    for block in iter(lambda:source.read(4*1024*1024),b''):digest.update(block);fileDigest.update(block)
                files.append({'name':raw.name,'bytes':raw.stat().st_size,'sha256':fileDigest.hexdigest()})
            assert len(files)==len(sealFiles) and sum(f['bytes'] for f in files)==expected['bytes'] and digest.hexdigest()==expected['sha256']
            records.append({'stream':index,'bytes':expected['bytes'],'frames':expected['frames'],'sha256':expected['sha256'],'files':files,
                'sender_seconds':expected['sender_seconds'],'receiver_tail_seconds':max(0,completed-expected['sender_eof_time']),
                'receiver_user_cpu_seconds':usage.ru_utime,'receiver_system_cpu_seconds':usage.ru_stime,'receiver_max_rss_kib':usage.ru_maxrss,'final_status':final})
        elapsed=time.monotonic()-started
        meminfo={}
        for line in Path('/proc/meminfo').read_text().splitlines():
            key,value=line.split(':',1);meminfo[key]=int(value.strip().split()[0])*1024
        result={'schema':1,'receiver':str(program),'receiver_sha256':hashlib.sha256(program.read_bytes()).hexdigest(),
            'test_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'physical_hosts':1,'real_hardware':False,
            'runtime_loader':str(args.loader.resolve()) if args.loader else None,'runtime_loader_sha256':loader_digest,
            'runtime_library_path':args.library_path,
            'qt_raw_path':False,'durable_fsync':True,'streams':records,'total_bytes':args.streams*target,'wall_seconds_including_hash_verification':elapsed,
            'memory_total_bytes':meminfo['MemTotal'],'payload_exceeds_physical_memory':args.streams*target>meminfo['MemTotal'],
            'generated_payload_pruned_after_hash_and_seal':True,'maximum_total_test_payload_bytes':MAX_TOTAL_BYTES,
            'rotation_bytes':args.rotate_mib*1024**2,
            'timing_note':'Sender timings include generation/hash/backpressure; tail times are observed in parent wait order, not exact fsync duration'}
    finally:
        # Prevent a second Ctrl+C / SIGTERM from interrupting child shutdown and cleanup.
        for sig in previous_handlers: signal.signal(sig, signal.SIG_IGN)
        try:
            for child in children:
                if child.poll() is None: child.terminate()
            for child in children:
                try: child.communicate(timeout=10)
                except subprocess.TimeoutExpired: child.kill();child.communicate()
            if executor is not None: executor.shutdown(wait=True)
        finally:
            try: remove_generated_payload(root)
            finally:
                lock_file.close()
                for sig, handler in previous_handlers.items(): signal.signal(sig, handler)
    (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS durable native receiver load:',args.streams,'streams',args.streams*target,
          'bytes; full sender/file SHA, EOF/exit/seal; generated payload deleted; evidence',root)


if __name__=='__main__':main()
