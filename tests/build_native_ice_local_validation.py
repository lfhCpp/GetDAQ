#!/usr/bin/env python3
"""Compile one local ICE ECC fixture and reuse the verified native sanitizer objects."""
import argparse,json,subprocess
from pathlib import Path
from run_receiver_observation_tests import sha


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('native',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--fault-probe',action='store_true');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];native=args.native.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    commands=[r['command'] for r in json.loads((native/'commands.json').read_text())];source=root/('tests/source_fault_commit_test.cpp' if args.fault_probe else 'tests/native_ecc_ice_receiver_test.cpp');obj=out/'local.o';program=out/('source-fault-commit-tests' if args.fault_probe else 'native-ecc-ice-receiver-tests')
    recipe=next(row for row in commands if '-c' in row and str(root/'tests/ice_source_control_test.cpp') in row);compile=recipe.copy();compile[compile.index('-c')+1]=str(source);compile[compile.index('-o')+1]=str(obj)
    assert '-std=c++11' in compile and '-pedantic-errors' in compile and '-fsanitize=address,undefined' in compile
    sources=json.loads((native/'source-sha256.json').read_text());assert all(sha(root/name)==digest for name,digest in sources.items())
    with (out/'compile.log').open('w') as log:subprocess.run(compile,stdout=log,stderr=subprocess.STDOUT,check=True)
    link=next(row for row in commands if '-c' not in row and '-o' in row and row[row.index('-o')+1]==str(native/'ice-source-control-tests')).copy()
    original=recipe[recipe.index('-o')+1];link=[str(obj) if word==original else word for word in link];link[link.index('-o')+1]=str(program)
    with (out/'link.log').open('w') as log:subprocess.run(link,stdout=log,stderr=subprocess.STDOUT,check=True)
    (out/'commands.json').write_text(json.dumps([compile,link],indent=2)+'\n');(out/'source-sha256.json').write_text(json.dumps({str(source.relative_to(root)):sha(source),str(Path(__file__).resolve().relative_to(root)):sha(Path(__file__))},indent=2)+'\n')
    (out/'build-receipt.json').write_text(json.dumps({'schema':1,'compile_count':1,'sanitized':True,'program_sha256':sha(program),'base_commands_sha256':sha(native/'commands.json'),
        'reused_objects':{word:sha(Path(word)) for word in link if word.endswith('.o') and word!=str(obj)},'old_libraries_instrumented':False},indent=2)+'\n')
    print('PASS local ICE ECC strict instrumented fixture; 1 compile, reused verified native objects')


if __name__=='__main__':main()
