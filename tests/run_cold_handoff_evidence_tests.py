#!/usr/bin/env python3
"""Run bounded cold-evidence metadata cases and delete all owned test inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);parser.add_argument('evidence',type=Path)
    parser.add_argument('--network-guard',type=Path);parser.add_argument('--sanitized',action='store_true')
    args=parser.parse_args();binary=args.binary.resolve(strict=True);args.evidence.mkdir(parents=True,exist_ok=True)
    before=sha(binary);environment=dict(os.environ)
    if args.network_guard:environment['LD_PRELOAD']=str(args.network_guard.resolve(strict=True))
    if args.sanitized:environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with tempfile.TemporaryDirectory(prefix='cold-handoff-',dir=args.evidence) as directory:
        temporary=Path(directory)
        network_log=temporary/'network.log'
        if args.network_guard:environment['GETDAQ_OFFLINE_NETWORK_LOG']=str(network_log)
        result=subprocess.run([str(binary),str(temporary)],env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
        (args.evidence/'metadata-tests.log').write_text(result.stdout)
        files=[p for p in temporary.rglob('*') if p.is_file()]
        assert not any(p.suffix=='.graw' for p in files)
        maximum=max([p.stat().st_size for p in files],default=0);total=sum(p.stat().st_size for p in files)
        assert maximum<=1024**2 and total<=2*1024**2
        assert result.returncode==0,result.stdout[-4000:]
        if args.network_guard:assert not network_log.exists() or network_log.stat().st_size==0,'Metadata review attempted network access'
        assert not any(x in result.stdout for x in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:'))
    assert not temporary.exists() and sha(binary)==before
    passed=[line for line in result.stdout.splitlines() if line.startswith('PASS ') and not line.startswith('PASS cold handoff evidence:')]
    summary={'passed':True,'cases':len(passed),'program_sha256':before,'sanitized':args.sanitized,
             'network_guard':bool(args.network_guard),'metadata_bytes_deleted':total,'largest_metadata_file_bytes':maximum,
             'temporary_inputs_removed':True,'raw_files_created':0,'real_hardware':False}
    (args.evidence/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('PASS cold handoff evidence metadata:',len(passed),'cases; owned inputs deleted, no raw data')


if __name__=='__main__':main()
