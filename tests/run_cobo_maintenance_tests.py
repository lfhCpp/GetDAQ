#!/usr/bin/env python3
"""Run selected-board control fixtures and retain bounded metadata evidence only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(source,destination):
    assert not list(source.rglob('*.graw')),'Control-only fixture unexpectedly generated raw payload'
    files=[p for p in source.rglob('*') if p.is_file()]
    assert len(files)<=10000 and sum(p.stat().st_size for p in files)<=128*1024**2,'Metadata archive budget exceeded'
    copied={}
    for path in files:
        assert not path.is_symlink() and path.suffix in ('.json','.jsonl'),str(path)
        relative=path.relative_to(source);target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target);assert sha(path)==sha(target);copied[relative.as_posix()]=sha(target)
    return dict(original_root=str(source),files=copied)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program',type=Path);parser.add_argument('evidence',type=Path)
    parser.add_argument('--scope',choices=('preparation','coordinator'),required=True)
    parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    program=args.program.resolve(strict=True);args.evidence.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix=args.scope+'-',dir=args.evidence)).resolve()
    env=dict(os.environ)
    if args.sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    record=dict(schema=1,scope=args.scope,passed=False,real_hardware=False,raw_generated=False,
                data_lifecycle_test_double=args.scope=='coordinator',real_receiver_executed=False,
                sanitized=args.sanitized,leak_detection=args.sanitized,program=str(program),program_sha256=sha(program),
                runner_sha256=sha(Path(__file__)),runs=[])
    scenarios=['maintenance'] if args.scope=='preparation' else ['maintenance-0','maintenance-1']
    try:
        for scenario in scenarios:
            log=output/(scenario+'.log');before=set(Path('/tmp').glob('getdaq-system-preparation-*'))
            command=[str(program),str(ROOT/'tests/data/cobo-hardware.json'),str(ROOT/'tests/data/mutant-hardware.json')]
            index=output/(scenario+'.index')
            if args.scope=='preparation':command+=['--maintenance']
            else:command+=[str(index),'--scenario',scenario]
            with log.open('wb') as stream:
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=900)
            assert result.returncode==0,(scenario,result.returncode,str(log))
            text=log.read_text(errors='replace')
            assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text and 'ERROR: LeakSanitizer' not in text,str(log)
            if args.scope=='preparation':
                assert 'PASS 6 native selected CoBo maintenance cases' in text
                created=set(Path('/tmp').glob('getdaq-system-preparation-*'))-before
                assert len(created)==1,'Ambiguous owned preparation fixture directory'
                source=created.pop()
            else:
                assert 'PASS native ECC selected maintenance, two explicit cycles' in text
                info=json.loads(Path(str(index)+'.root.json').read_text());source=Path(info['root'])
                assert info['scenario']==scenario and info['data_lifecycle_test_double'] and not info['real_receiver_executed']
            saved=archive(source,output/scenario)
            record['runs'].append(dict(scenario=scenario,exit_code=result.returncode,log_sha256=sha(log),archive=saved))
        record['passed']=True
    finally:
        assert sha(program)==record['program_sha256'],'Program changed during test'
        record['raw_remaining']=len(list(output.rglob('*.graw')))
        (output/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS selected CoBo',args.scope,'fixtures;',len(record['runs']),'runs; metadata only;',output)


if __name__=='__main__':main()
