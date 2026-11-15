#!/usr/bin/env python3
"""Build optional ICE ingress with strict new-code and local-vendor receipts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('sdk',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    sdk=args.sdk.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    recipes=json.loads((sdk/'compile_commands.json').read_text())
    selected=[r for r in recipes if any(part in str(r['file']) for part in ['/apps/iceingress/','/ice-ingress/generated/IceIngress.cpp']) or
              str(r['file']).endswith(('/tests/ice_ingress_fixture.cpp','/tests/ice_source_client_fixture.cpp','/tests/ice_ingress_options_test.cpp')) or
              ('getdaq-ice-ingress-core.dir' in r['command'] and '/apps/datareceiver/' in r['file'])]
    assert len(selected)==21,(len(selected),[r['file'] for r in selected])
    generated=[sdk/'ice-ingress/generated/IceIngress.cpp',sdk/'ice-ingress/generated/IceIngress.h',
               sdk/'generated/mdaq/daq/DataProcessor.cpp',sdk/'generated/mdaq/daq/DataProcessor.h']
    sources={Path(r['file']).resolve() for r in selected if root in Path(r['file']).resolve().parents and sdk not in Path(r['file']).resolve().parents}
    sources.update((root/'apps/iceingress').glob('*.h'))
    for name in ['apps/iceingress/CMakeLists.txt','apps/iceingress/IceIngress.ice','third_party/ice-compat/CMakeLists.txt',
                 'apps/datareceiver/mfmheader.h','apps/datareceiver/blockqueue.h','apps/datareceiver/mfmstream.h','apps/datareceiver/diskreserve.h','apps/datareceiver/framestore.h','apps/datareceiver/netutil.h',
                 'apps/datareceiver/exporter.h','apps/datareceiver/bytestore.h',
                 'common/storage/directorylease.h','common/text/utf8.h','tests/build_ice_ingress_validation.py',
                 'source/mordicus/MDaq-release-20210208/src/mdaq/daq/DataProcessor.ice']:
        sources.add(root/name)
    inputs={str(p.relative_to(root)):sha(p) for p in sorted(sources)}
    objects={};commands=[]
    for index,recipe in enumerate(selected):
        command=shlex.split(recipe['command']);obj=output/(str(index)+'.o');at=command.index('-o');command[at+1]=str(obj)
        command=[item for item in command if not item.startswith('-std=') and item not in ['-O3','-O2','-O1','-g']]
        command+=['-std=c++11','-pedantic-errors']
        if args.sanitized:command+=['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined']
        else:command+=['-O2']
        with (output/(str(index)+'.compile.txt')).open('w') as log:
            subprocess.run(command,cwd=recipe['directory'],stdout=log,stderr=subprocess.STDOUT,check=True)
        objects[Path(recipe['file']).resolve()]=obj;commands.append(command)
    common=[obj for path,obj in objects.items() if path.name not in ['main.cpp','sendmain.cpp','ice_ingress_fixture.cpp','ice_source_client_fixture.cpp','ice_ingress_options_test.cpp']]
    libraries=[sdk/'libcompat-GET.a',sdk/'libcompat-Ice.a',sdk/'libcompat-IceUtil.a',sdk/'libcompat-bzip.a']
    assert all(p.is_file() for p in libraries)
    for name,main_file in [('getdaq-ice-receiver',root/'apps/iceingress/main.cpp'),('ice-ingress-fixture',root/'tests/ice_ingress_fixture.cpp'),('ice-source-client-fixture',root/'tests/ice_source_client_fixture.cpp'),('getdaq-ice-send',root/'apps/iceingress/sendmain.cpp')]:
        linked=common+[objects[main_file]]
        command=['c++','-o',str(output/name)]+[str(p) for p in linked+libraries]+['-pthread','-ldl','-lrt']
        if args.sanitized:command+=['-fsanitize=address,undefined']
        with (output/(name+'.link.txt')).open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(command)
    options_test=output/'ice-ingress-options-tests'
    options_command=['c++','-o',str(options_test),str(objects[root/'apps/iceingress/iceingress_options.cpp']),
                    str(objects[root/'tests/ice_ingress_options_test.cpp']),'-pthread']
    if args.sanitized:options_command+=['-fsanitize=address,undefined']
    with (output/'ice-ingress-options-tests.link.txt').open('w') as log:
        subprocess.run(options_command,stdout=log,stderr=subprocess.STDOUT,check=True)
    commands.append(options_command)
    test_environment=dict(os.environ)
    if args.sanitized:test_environment['ASAN_OPTIONS']='detect_leaks=0'
    test_result=subprocess.run([str(options_test)],capture_output=True,text=True,check=True,env=test_environment)
    assert 'PASS ICE receiver option' in test_result.stdout,test_result.stdout
    assert all(sha(root/name)==digest for name,digest in inputs.items()),'source changed while compiling'
    dependencies={name:subprocess.check_output(['ldd',str(output/name)],text=True) for name in ['getdaq-ice-receiver','ice-ingress-fixture','ice-source-client-fixture','getdaq-ice-send','ice-ingress-options-tests']}
    assert all(not any(token in linked for token in ['libQt','libyaml','libboost','libcurl','libCore.so']) for linked in dependencies.values())
    receipt={'schema':1,'sanitized':args.sanitized,'strict_cpp11_compile_count':len(selected),'local_vendor_libraries_instrumented':False,
             'qt_used':False,'option_test_stdout':test_result.stdout.strip(),
             'sanitizer_runtime_note':'LeakSanitizer disabled for the isolated options test because this runner reports a fatal LSan runtime error; ASan/UBSan instrumentation remains active.' if args.sanitized else '',
             'program_sha256':{name:sha(output/name) for name in dependencies},'runtime_dependencies':dependencies,
             'compiler_version':subprocess.check_output(['c++','--version'],text=True).splitlines()[0]}
    (output/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    (output/'source-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (output/'generated-sha256.json').write_text(json.dumps({str(p):sha(p) for p in generated},indent=2)+'\n')
    (output/'dependency-sha256.json').write_text(json.dumps({str(p):sha(p) for p in libraries},indent=2)+'\n')
    (output/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS optional ICE ingress:',len(selected),'strict C++11 compiles;',len(inputs),'source inputs; sanitized',args.sanitized,'; local old Ice/GET/bzip not instrumented; no Qt')


if __name__=='__main__':main()
