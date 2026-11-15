"""Generate a private test launcher: lose one real CLI POST reply, then inspect only."""
from pathlib import Path


def create(path:Path,tool:Path,guard:Path,log:Path,asan_runtime=None):
    preload=([str(asan_runtime.resolve(strict=True))] if asan_runtime else [])+[str(guard)]
    program='''#!/usr/bin/env python3
import fcntl,json,os,subprocess,sys,time
from pathlib import Path
tool=__TOOL__
guard_log=Path(__LOG__)
arguments=sys.argv[1:]
before=guard_log.read_bytes() if guard_log.exists() else b''
environment=dict(os.environ)
release=Path(str(guard_log)+'.release')
if arguments and arguments[0]=='submit':
    environment['LD_PRELOAD']=__PRELOAD__
    environment['GETDAQ_HANDOFF_REPLY_LOG']=str(guard_log)
    environment['GETDAQ_HANDOFF_REPLY_RELEASE']=str(release)
    if not before:
        with open(arguments[3],'rb') as reservation:
            fcntl.flock(reservation,fcntl.LOCK_EX|fcntl.LOCK_NB)
            rejected=subprocess.run([tool,*arguments])
            assert rejected.returncode==2 and not Path(arguments[-1]).exists(), 'CLI submitted while successor activation held its plan'
        result=subprocess.Popen([tool,*arguments],env=environment)
        deadline=time.monotonic()+10
        while not guard_log.exists() or guard_log.read_bytes()!=b'post\\n':
            assert result.poll() is None and time.monotonic()<deadline, 'CLI did not reach the one POST'
            time.sleep(0.01)
        with open(arguments[3],'rb') as reservation:
            held=False
            try:fcntl.flock(reservation,fcntl.LOCK_EX|fcntl.LOCK_NB)
            except BlockingIOError:held=True
            assert held, 'Successor startup could consume the plan during original handoff'
        release.write_text('release test reply\\n');release.chmod(0o600)
        result.wait(timeout=15)
        Path(str(guard_log)+'.locks.json').write_text(json.dumps(dict(exclusive_activation_rejected=True,shared_handoff_blocks_activation=True))+'\\n')
    else:result=subprocess.run([tool,*arguments],env=environment)
else:result=subprocess.run([tool,*arguments],env=environment)
after=guard_log.read_bytes() if guard_log.exists() else b''
if after!=before:
    assert arguments[0]=='submit' and result.returncode==2 and after==b'post\\ndrop\\n', 'Expected exactly one ambiguous CLI POST'
    assert Path(arguments[-1],'submission-held.json').is_file(), 'Lost reply was not durably held'
    print('TEST: actual ECC reply consumed; querying original operation, no POST retry',flush=True)
    arguments[0]='inspect'
    result=subprocess.run([tool,*arguments])
sys.exit(result.returncode)
'''
    program=program.replace('__TOOL__',repr(str(tool))).replace('__LOG__',repr(str(log))).replace('__PRELOAD__',repr(':'.join(preload)))
    # Exact template tokens above must not alter LD_PRELOAD/guard variable names.
    path.write_text(program);path.chmod(0o700)
