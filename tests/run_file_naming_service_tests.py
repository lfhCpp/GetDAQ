#!/usr/bin/env python3
"""GET-named receiver through real authenticated control service; prune owned payload."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile


def sha(data):return hashlib.sha256(data).hexdigest()


def interrupted(signum,unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('filename service test interrupted')


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('service',type=Path);parser.add_argument('fixture',type=Path);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();args.evidence.mkdir(parents=True,exist_ok=True);parent=Path(tempfile.mkdtemp(prefix='file-naming-http-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    command=['python3',str(root/'tests/run_supervisor_control_tests.py'),str(args.service.resolve()),str(args.fixture.resolve()),str(args.receiver.resolve()),'--output-parent',str(parent),'--jobs','1','--scenarios','normal','catalog','--file-naming','get']
    process=None;cleanup=[]
    try:
        with (parent/'runner.log').open('w') as log:
            process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            code=process.wait(timeout=150)
        assert code==0,(parent/'runner.log').read_text()
        roots=list(parent.glob('remote-receiver-*'));assert len(roots)==1
        results=json.loads((roots[0]/'results.json').read_text());assert all(row['exit_code']==0 for row in results)
        raw=list(parent.rglob('*.graw'));assert raw and all(':' in path.name and 'T' in path.name for path in raw)
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
        for path in parent.rglob('*.graw'):
            cleanup.append({'path':str(path.relative_to(parent)),'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())});path.unlink()
        (parent/'payload-cleanup.json').write_text(json.dumps({'deleted':cleanup,'raw_files_remaining':len(list(parent.rglob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'passed':True,'results':results,'generated_raw_deleted':not list(parent.rglob('*.graw')),'real_hardware':False,
            'runner_sha256':sha(Path(__file__).read_bytes()),'underlying_runner_sha256':sha((root/'tests/run_supervisor_control_tests.py').read_bytes()),
            'program_sha256':{name:sha(path.resolve().read_bytes()) for name,path in [('service',args.service),('fixture',args.fixture),('receiver',args.receiver)]}}
    (parent/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS GET-named receivers via authenticated service: normal/catalog, 8 HTTP faults, codec/credentials; payload deleted;',parent)


if __name__=='__main__':main()
