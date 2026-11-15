"""Bind an actual CLI to zero-network cold inspection, including ASan order."""
import json
from pathlib import Path


def create(path, program, guard, asan_runtime=None):
    config = dict(program=str(Path(program).resolve(strict=True)),
                  guard=str(Path(guard).resolve(strict=True)),
                  asan_runtime=str(Path(asan_runtime).resolve(strict=True)) if asan_runtime else None)
    body = '''#!/usr/bin/python3
import hashlib,json,os,sys
from pathlib import Path
config=CONFIG
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
directory=Path(sys.argv[-1]);parent=directory.parent
record=dict(program_sha256=sha(config['program']),guard_sha256=sha(config['guard']),wrapper_sha256=sha(__file__),asan_runtime_sha256=sha(config['asan_runtime']) if config['asan_runtime'] else None)
file=parent/'cold-cli-programs.json'
if file.exists():
    assert json.loads(file.read_text())==record,'Actual CLI/guard/wrapper changed during fixture'
else:
    fd=os.open(str(file),os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w') as stream:json.dump(record,stream,sort_keys=True);stream.write('\\n')
env=dict(os.environ)
if sys.argv[1]=='cold-complete-inspect':
    env['LD_PRELOAD']=':'.join(x for x in (config['asan_runtime'],config['guard']) if x)
    env['GETDAQ_OFFLINE_NETWORK_LOG']=str(parent/'cold-inspect-network.log')
os.execve(config['program'],[config['program']]+sys.argv[1:],env)
'''.replace('CONFIG', repr(config), 1)
    Path(path).write_text(body)
    Path(path).chmod(0o700)
