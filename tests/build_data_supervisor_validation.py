#!/usr/bin/env python3
"""Build native data management and its optional low-frequency Qt service with receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('build',type=Path);parser.add_argument('--sanitized',action='store_true')
    args=parser.parse_args();build=args.build.resolve();build.mkdir(parents=True,exist_ok=True)
    command=['cmake','-S',str(root/'apps/datasupervisor'),'-B',str(build),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DGETDAQ_BUILD_SUPERVISOR_SERVICE=ON','-DBUILD_TESTING=ON']
    if args.sanitized:command+=['-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_CXX_FLAGS=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    else:command+=['-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-pedantic-errors','-DCMAKE_EXE_LINKER_FLAGS=']
    with (build/'results-configure-verified.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    recipes=json.loads((build/'compile_commands.json').read_text())
    tracked={Path(__file__).resolve(),root/'apps/datasupervisor/CMakeLists.txt',root/'apps/datasupervisor/service/CMakeLists.txt',root/'common/experiment/CMakeLists.txt'}
    for recipe in recipes:
        assert '-std=c++11' in recipe['command'] and '-pedantic-errors' in recipe['command']
        if args.sanitized:assert '-fsanitize=address,undefined' in recipe['command']
        p=Path(recipe['file']).resolve()
        if root in p.parents and build not in p.parents:tracked.add(p)
    for folder in ('apps/datasupervisor','common/experiment','common/storage','common/text','common/control','common/config','common/http'):
        tracked.update((root/folder).rglob('*.h'))
        tracked.update((root/folder).glob('CMakeLists.txt'))
    tracked.add(root/'tests/owned_test_directory.h')
    before={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    compile_command=['cmake','--build',str(build),'--parallel','2']
    with (build/'results-build-verified.txt').open('w') as log:subprocess.run(compile_command,stdout=log,stderr=subprocess.STDOUT,check=True)
    after={str(p.relative_to(root)):sha(p) for p in sorted(tracked)};assert before==after,'source changed while compiling'
    programs=['getdaq-supervisor','getdaq-supervisor-query','getdaq-catalog','supervisor-control-tests','receiver-catalog-tests','data-supervisor-tests','managed-ice-supervisor-tests']
    dependencies={}
    for name in programs:
        linked=subprocess.check_output(['ldd',str(build/name)],text=True);dependencies[name]=linked
        assert not any(s in linked for s in ('libIce','libyaml','libboost','libcurl','libCore.so'))
        if name!='getdaq-supervisor':assert 'libQt' not in linked
    generated={str(Path(r['file']).resolve().relative_to(build)):sha(Path(r['file'])) for r in recipes if build in Path(r['file']).resolve().parents}
    receipt={'schema':1,'sanitized':args.sanitized,'system_qt_instrumented':False,'raw_data_path_uses_qt':False,
             'commands':[command,compile_command],'compile_count':len(recipes),'tracked_input_count':len(after),
             'program_sha256':{p:sha(build/p) for p in programs},'dependencies':dependencies,
             'compiler_version':subprocess.check_output(['c++','--version'],text=True).splitlines()[0]}
    (build/'source-sha256.json').write_text(json.dumps(after,indent=2)+'\n');(build/'generated-sha256.json').write_text(json.dumps(generated,indent=2)+'\n')
    (build/'verified-compile-commands.json').write_text(json.dumps(recipes,indent=2)+'\n');(build/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS data supervisor build:',len(recipes),'strict C++11 compiles;',len(after),'inputs; sanitized',args.sanitized)


if __name__=='__main__':main()
