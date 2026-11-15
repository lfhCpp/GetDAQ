#!/usr/bin/env python3
"""Independently recheck the current header graph of cached ASan compilations."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check(receipt_file):
    receipt=json.loads(receipt_file.read_text());assert receipt['sanitized']
    for path,digest in receipt['dependencies'].items():assert sha(path)==digest,path
    for path,digest in receipt['compiled_objects'].items():assert sha(path)==digest,path
    for path,digest in receipt['reused_objects'].items():assert sha(path)==digest,path
    for command in receipt['commands']:
        if '-c' in command:assert '-std=c++11' in command and '-pedantic-errors' in command and '-fsanitize=address,undefined' in command
    checked={};overlay=receipt.get('reuse_overlay')
    if overlay:
        current=overlay;seen=set();origins={}
        while current:
            prior_file=Path(current['file']).resolve();assert prior_file not in seen and len(seen)<32
            seen.add(prior_file);assert sha(prior_file)==current['sha256'];prior=json.loads(prior_file.read_text());assert prior['sanitized']
            recipes={c[c.index('-o')+1]:c for c in prior['commands'] if '-c' in c}
            for obj,digest in prior['compiled_objects'].items():
                origins.setdefault(obj,(digest,recipes[obj],prior['dependencies']))
            current=prior.get('reuse_overlay')
        for obj,digest in receipt['reused_objects'].items():
            if obj not in origins:continue
            expected,recipe,inputs=origins[obj];assert expected==digest
            command=recipe.copy();assert '-std=c++11' in command and '-pedantic-errors' in command and '-fsanitize=address,undefined' in command
            command.remove('-c');position=command.index('-o');del command[position:position+2];command+=['-MM','-MT','cached-handoff-object']
            text=subprocess.check_output(command,text=True,cwd=Path(__file__).resolve().parents[1])
            paths=[str(Path(p).resolve()) for p in shlex.split(text.replace('\\\n',' ').split(':',1)[1])]
            for path in paths:
                current=sha(path);assert current==inputs.get(path)==receipt['dependencies'].get(path),(obj,path)
            checked[obj]=len(paths)
    return {'passed':True,'receipt_sha256':sha(receipt_file),'compiled_objects':len(receipt['compiled_objects']),
            'reused_objects_hash_verified':len(receipt['reused_objects']),'cached_compilations_with_current_inputs_verified':checked,
            'old_libraries_wholly_instrumented':False}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receipt',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
    result=check(args.receipt.resolve())
    if args.output:args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS cached ASan inputs:',len(result['cached_compilations_with_current_inputs_verified']),'cached compilations; all object hashes verified')
