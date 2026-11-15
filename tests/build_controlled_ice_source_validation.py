#!/usr/bin/env python3
"""Strict controlled source build using verified native ICE source dependency objects."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from run_receiver_observation_tests import sha


def main():
    root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
    for name in ['sdk','ice_build','output']:parser.add_argument(name,type=Path)
    parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    sdk=args.sdk.resolve();dependency=args.ice_build.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    receipt=json.loads((dependency/'build-receipt.json').read_text());assert receipt['sanitized']==args.sanitized
    recipes=json.loads((sdk/'compile_commands.json').read_text());selected=[r for r in recipes if r['file'].endswith(('/apps/icecontrol/controlledicesource.cpp','/tests/controlled_ice_source_fixture.cpp'))]
    assert len(selected)==2
    inputs={str(Path(r['file']).resolve().relative_to(root)):sha(Path(r['file'])) for r in selected}
    for name in ['apps/icecontrol/controlledicesource.h','apps/icecontrol/CMakeLists.txt','apps/iceingress/icesourceclient.h','apps/iceingress/CMakeLists.txt','common/text/utf8.h','tests/build_controlled_ice_source_validation.py']:
        inputs[name]=sha(root/name)
    objects=[];commands=[]
    for index,recipe in enumerate(selected):
        command=shlex.split(recipe['command']);obj=output/(str(index)+'.o');command[command.index('-o')+1]=str(obj)
        command=[word for word in command if not word.startswith('-std=') and word not in ['-O1','-O2','-O3','-g']]+['-std=c++11','-pedantic-errors']
        command+=['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined'] if args.sanitized else ['-O2']
        with (output/(str(index)+'.compile.txt')).open('w') as log:subprocess.run(command,cwd=recipe['directory'],stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(command);objects.append(obj)
    dependency_commands=json.loads((dependency/'commands.json').read_text());source_objects=[]
    for command in dependency_commands:
        if '-c' in command and any(word.endswith(('/icesourceclient.cpp','/IceIngress.cpp')) for word in command):source_objects.append(Path(command[command.index('-o')+1]))
    assert len(source_objects)==2
    generated=json.loads((dependency/'generated-sha256.json').read_text());assert all(sha(Path(name))==digest for name,digest in generated.items())
    libraries=json.loads((dependency/'dependency-sha256.json').read_text());assert all(sha(Path(name))==digest for name,digest in libraries.items())
    base_sources=json.loads((dependency/'source-sha256.json').read_text());assert all(sha(root/name)==digest for name,digest in base_sources.items())
    command=['c++','-o',str(output/'controlled-ice-source-fixture')]+[str(p) for p in objects+source_objects]+list(libraries)+['-pthread','-ldl','-lrt']
    if args.sanitized:command+=['-fsanitize=address,undefined']
    with (output/'link.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    commands.append(command);assert all(sha(root/name)==digest for name,digest in inputs.items())
    inputs.update(base_sources);runtime=subprocess.check_output(['ldd',str(output/'controlled-ice-source-fixture')],text=True)
    assert not any(word in runtime for word in ['libQt','libyaml','libboost','libcurl'])
    (output/'source-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n');(output/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    (output/'build-receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitized,'compile_count':2,'reused_dependency_objects':{str(p):sha(p) for p in source_objects},
        'dependency_build_receipt_sha256':sha(dependency/'build-receipt.json'),'program_sha256':{'controlled-ice-source-fixture':sha(output/'controlled-ice-source-fixture')},'runtime_dependencies':runtime,'old_vendor_libraries_instrumented':False},indent=2)+'\n')
    print('PASS controlled ICE source: 2 strict C++11 compiles;',len(inputs),'inputs; sanitized',args.sanitized,'; reused verified source/protocol objects; no Qt')


if __name__=='__main__':main()
