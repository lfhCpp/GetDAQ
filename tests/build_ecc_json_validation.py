#!/usr/bin/env python3
"""Record standalone ECC journal/state builds without YAML dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build',type=Path);parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    build=args.build.resolve();build.mkdir(parents=True,exist_ok=True)
    command=['cmake','-S',str(root/'apps/ecc'),'-B',str(build),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    if args.sanitized:
        command+=['-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_CXX_FLAGS=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    else:command+=['-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-pedantic-errors','-DCMAKE_EXE_LINKER_FLAGS=']
    with (build/'results-configure-verified.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    all_recipes=json.loads((build/'compile_commands.json').read_text())
    targets=['getdaq-experiment-core.dir/','getdaq-ecc-core.dir/','getdaq-ecc-journal.dir/','getdaq-ecc-journal-tool.dir/','ecc-state-tests.dir/','ecc-journal-tests.dir/']
    recipes=[x for x in all_recipes if any(t in x['command'] for t in targets)]
    tracked={Path(x['file']).resolve() for x in recipes}|{Path(__file__).resolve()}
    for folder in ['apps/ecc','common/experiment','common/config','common/text']:
        tracked.update((root/folder).glob('*.h'));tracked.update((root/folder).glob('CMakeLists.txt'))
    before={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    for x in recipes:
        assert '-std=c++11' in x['command'] and '-pedantic-errors' in x['command']
        assert 'yaml' not in x['command'].lower()
        if args.sanitized:assert '-fsanitize=address,undefined' in x['command']
    build_command=['cmake','--build',str(build),'--parallel','2','--target','ecc-state-tests','ecc-journal-tests','getdaq-ecc-journal-tool']
    with (build/'results-build-verified.txt').open('w') as log:subprocess.run(build_command,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert before=={str(p.relative_to(root)):sha(p) for p in sorted(tracked)},'Inputs changed during build'
    programs=['ecc-state-tests','ecc-journal-tests','getdaq-ecc-journal'];dependencies={}
    for name in programs:
        dependencies[name]=subprocess.check_output(['ldd',str(build/name)],text=True)
        assert not any(v in dependencies[name] for v in ('libQt','libIce','libyaml','libboost','libcurl'))
    (build/'source-sha256.json').write_text(json.dumps(before,indent=2)+'\n')
    (build/'verified-compile-commands.json').write_text(json.dumps(recipes,indent=2)+'\n')
    (build/'build-receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitized,'compile_count':len(recipes),'program_sha256':{n:sha(build/n) for n in programs},'runtime_dependencies':dependencies,'commands':[command,build_command]},indent=2)+'\n')
    print('PASS standalone ECC JSON build:',len(recipes),'strict C++11 compiles; no YAML; sanitized',args.sanitized)


if __name__=='__main__':main()
