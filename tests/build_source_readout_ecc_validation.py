#!/usr/bin/env python3
"""Strict sanitizer overlay: verify unchanged native objects, rebuild readout/servant/ECC fixtures."""
import argparse,json,shlex,subprocess
from pathlib import Path
from run_receiver_observation_tests import sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['base','sdk','output']:parser.add_argument(name,type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];base=args.base.resolve();sdk=args.sdk.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    old=json.loads((base/'source-sha256.json').read_text());rows=json.loads((base/'commands.json').read_text());recipes=[r['command'] if isinstance(r,dict) else r for r in rows]
    compile_by_object={Path(cmd[cmd.index('-o')+1]):cmd for cmd in recipes if '-c' in cmd}
    libraries=json.loads((base/'dependency-sha256.json').read_text());assert all(sha(Path(name))==digest for name,digest in libraries.items())
    generated=json.loads((base/'generated-sha256.json').read_text());assert all(sha(Path(name))==digest for name,digest in generated.items())
    template=next(cmd for cmd in recipes if '-c' in cmd and str(root/'tests/ecc_network_service_test.cpp') in cmd)
    sources=[root/name for name in ['apps/sourcereader/sourceringreader.cpp','apps/sourcereader/sourceringreadout.cpp','apps/sourcereader/sourcemfmring.cpp','apps/icecontrol/sourcecontrolservant.cpp','tests/ecc_network_service_test.cpp','tests/native_ecc_ice_receiver_test.cpp']]
    commands=[];objects={};tracked={Path(__file__).resolve()};reused={}
    for source in sources:
        cmd=template.copy();obj=out/(source.stem+'.o');cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj);cmd+=['-I'+str(root/'apps/sourcereader')]
        assert '-std=c++11' in cmd and '-pedantic-errors' in cmd and '-fsanitize=address,undefined' in cmd
        with (out/(source.stem+'.compile.txt')).open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(cmd);objects[source]=obj
    replacements={base/'apps-icecontrol-sourcecontrolservant.o':objects[root/'apps/icecontrol/sourcecontrolservant.cpp'],base/'tests-ecc_network_service_test.o':objects[root/'tests/ecc_network_service_test.cpp']}
    links={}
    for name in ['ecc-network-service-tests','getdaq-ecc-server']:
        links[name]=next(cmd for cmd in recipes if '-c' not in cmd and '-o' in cmd and Path(cmd[cmd.index('-o')+1]).name==name).copy()
    links['native-ecc-ice-receiver-tests']=links['ecc-network-service-tests'].copy()
    links['native-ecc-ice-receiver-tests']=[str(objects[root/'tests/native_ecc_ice_receiver_test.cpp']) if word==str(base/'tests-ecc_network_service_test.o') else word for word in links['native-ecc-ice-receiver-tests']]
    for name,cmd in links.items():
        cmd=[str(replacements.get(Path(word),Path(word))) if word.endswith('.o') else word for word in cmd]
        cmd += [str(objects[root/'apps/sourcereader/sourceringreader.cpp']),str(objects[root/'apps/sourcereader/sourceringreadout.cpp']),str(objects[root/'apps/sourcereader/sourcemfmring.cpp'])]
        cmd[cmd.index('-o')+1]=str(out/name);links[name]=cmd
    dependencies={}
    for obj in sorted({Path(word) for cmd in links.values() for word in cmd if word.endswith('.o')}):
        if obj.parent==out:
            recipe=next(cmd for cmd in commands if cmd[cmd.index('-o')+1]==str(obj))
        else:
            assert obj in compile_by_object,obj
            recipe=compile_by_object[obj];source=Path(recipe[recipe.index('-c')+1])
            assert source.is_file() and (sha(source)==old[str(source.relative_to(root))] if root in source.parents else sha(source)==generated[str(source)]),source
            reused[str(obj)]=sha(obj)
        dep=recipe.copy();dep.remove('-c');i=dep.index('-o');del dep[i:i+2];dep+=['-MM','-MT','readout']
        output=subprocess.check_output(dep,text=True);words=shlex.split(output.replace('\\\n',' ').split(':',1)[1])
        for word in words:
            path=Path(word).resolve();dependencies[str(path)]=sha(path)
            if root in path.parents and sdk not in path.parents:
                tracked.add(path)
                if obj.parent!=out:assert old.get(str(path.relative_to(root)))==sha(path),(obj,path,'Changed dependency cannot reuse old object')
            elif obj.parent!=out and str(path) in generated:assert generated[str(path)]==sha(path)
    for cmd in links.values():
        with (out/(Path(cmd[cmd.index('-o')+1]).name+'.link.txt')).open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(cmd)
    assert all(sha(Path(name))==digest for name,digest in dependencies.items()) and all(sha(Path(name))==digest for name,digest in reused.items())
    (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');(out/'source-sha256.json').write_text(json.dumps({str(p.relative_to(root)):sha(p) for p in sorted(tracked)},indent=2)+'\n')
    (out/'dependency-sha256.json').write_text(json.dumps(dependencies,indent=2)+'\n');(out/'build-receipt.json').write_text(json.dumps({'schema':1,'sanitized':True,'compile_count':6,'reused_objects':reused,
        'base_commands_sha256':sha(base/'commands.json'),'old_vendor_libraries_instrumented':False,'program_sha256':{name:sha(out/name) for name in links}},indent=2)+'\n')
    print('PASS source readout ECC sanitizer overlay; 6 strict compiles,',len(reused),'verified unchanged objects;',len(tracked),'repo dependencies; old libraries uninstrumented')


if __name__=='__main__':main()
