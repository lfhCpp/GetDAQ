#!/usr/bin/env python3
"""Build all new source ring/lifecycle/client logic with strict C++11 and optional sanitizers."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from run_receiver_observation_tests import sha


def main():
    root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sdk',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    sdk=args.sdk.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    recipes=json.loads((sdk/'compile_commands.json').read_text())
    sources=[root/name for name in ['apps/sourcereader/sourceringreader.cpp','apps/sourcereader/sourceringreadout.cpp','apps/sourcereader/sourcemfmring.cpp','apps/icecontrol/controlledicesource.cpp','apps/iceingress/icesourceclient.cpp','tests/ring_ice_source_fixture.cpp','tests/source_ring_reader_test.cpp','tests/source_ring_readout_test.cpp','tests/source_mfm_ring_test.cpp']]
    sources.append(sdk/'ice-ingress/generated/IceIngress.cpp')
    selected=[]
    for source in sources:
        rows=[r for r in recipes if Path(r['file']).resolve()==source]
        assert len(rows)==1,source
        selected.append(rows[0])
    tracked=set(sources)|{root/name for name in ['apps/datareceiver/mfmheader.h','apps/sourcereader/sourcemfmring.h','apps/sourcereader/sourceringreader.h','apps/sourcereader/sourceringreadout.h','apps/sourcereader/CMakeLists.txt','apps/icecontrol/controlledicesource.h','apps/icecontrol/CMakeLists.txt',
        'apps/iceingress/icesourceclient.h','common/text/utf8.h','apps/iceingress/IceIngress.ice','tests/build_source_ring_validation.py','tests/run_ring_ice_source_tests.py','tests/run_ice_source_control_tests.py','tests/receiver_transport_integration.py','tests/run_receiver_observation_tests.py']}
    tracked.add(sdk/'ice-ingress/generated/IceIngress.h')
    labels=lambda path:str(path.relative_to(root)) if root in path.parents else str(path)
    before={labels(p):sha(p) for p in sorted(tracked)};commands=[];objects=[]
    for index,recipe in enumerate(selected):
        command=shlex.split(recipe['command']);obj=out/(str(index)+'.o');command[command.index('-o')+1]=str(obj)
        command=[word for word in command if not word.startswith('-std=') and word not in ['-O1','-O2','-O3','-g']]+['-std=c++11','-pedantic-errors']
        command+=['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined'] if args.sanitized else ['-O2']
        with (out/(str(index)+'.compile.txt')).open('w') as log:subprocess.run(command,cwd=recipe['directory'],stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(command);objects.append(obj)
    libraries=[sdk/('libcompat-'+name+'.a') for name in ['GET','Ice','IceUtil','bzip']]
    dependencies={str(path):sha(path) for path in libraries};programs={}
    for name,indices,libs in [('ring-ice-source-fixture',[0,1,2,3,4,5,9],libraries),('source-ring-reader-tests',[0,6],[]),('source-ring-readout-tests',[0,1,7],[]),('source-mfm-ring-tests',[0,1,2,8],[])]:
        command=['c++','-o',str(out/name)]+[str(objects[i]) for i in indices]+[str(path) for path in libs]+['-pthread','-ldl','-lrt']
        if args.sanitized:command+=['-fsanitize=address,undefined']
        with (out/(name+'.link.txt')).open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(command);runtime=subprocess.check_output(['ldd',str(out/name)],text=True)
        assert not any(word in runtime for word in ['libQt','libyaml','libboost','libcurl'])
        programs[name]={'sha256':sha(out/name),'runtime_dependencies':runtime}
    assert before=={labels(p):sha(p) for p in sorted(tracked)} and dependencies=={str(p):sha(p) for p in libraries}
    (out/'source-sha256.json').write_text(json.dumps(before,indent=2)+'\n');(out/'dependency-sha256.json').write_text(json.dumps(dependencies,indent=2)+'\n');(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    (out/'build-receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitized,'compile_count':len(selected),'programs':programs,'old_vendor_libraries_instrumented':False},indent=2)+'\n')
    print('PASS source ring build;',len(selected),'strict C++11 compiles;',len(before),'inputs; sanitized',args.sanitized,'; no Qt')


if __name__=='__main__':main()
