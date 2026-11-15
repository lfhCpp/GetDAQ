#!/usr/bin/env python3
"""Rebuild changed ECC objects using actual header dependencies, retaining the original ASan build."""
import argparse
import concurrent.futures
import hashlib
import json
import shlex
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["base", "sdk", "output"]:parser.add_argument(name,type=Path)
    parser.add_argument("--target",choices=("remote","planning","successor","network","handoff","preparation","coordinator","handoff-evidence"),default="remote")
    parser.add_argument('--reuse-overlay',type=Path,help='Reuse freshly hashed objects only when the compiler recipe and every input of that translation unit match')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    base,sdk,out=args.base.resolve(),args.sdk.resolve(),args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    old=json.loads((base/"source-sha256.json").read_text())
    recipes=[row["command"] for row in json.loads((base/"commands.json").read_text())]
    compile_by_object={Path(cmd[cmd.index("-o")+1]):cmd for cmd in recipes if "-c" in cmd}
    name="getdaq-ecc-server" if args.target=="planning" else "ecc-network-service-tests"
    link=next(cmd.copy() for cmd in recipes if "-c" not in cmd and "-o" in cmd and Path(cmd[cmd.index("-o")+1]).name==name)
    generated=json.loads((base/"generated-sha256.json").read_text())
    template=next(cmd.copy() for cmd in recipes if "-c" in cmd and Path(cmd[cmd.index("-c")+1]).name=="ecc_network_service_test.cpp")
    for name,digest in json.loads((base/"dependency-sha256.json").read_text()).items():
        path=Path(name);path=path if path.is_absolute() else root/path
        assert sha(path)==digest, str(path)
    fixture=root/({'successor':'tests/ecc_successor_identity_test.cpp','network':'tests/ecc_network_service_test.cpp',
                  'handoff':'apps/ecc/handoff/main.cpp','preparation':'tests/hardware_system_preparation_test.cpp',
                  'coordinator':'tests/native_ecc_coordinator_test.cpp','handoff-evidence':'tests/remote_handoff_evidence_test.cpp'}.get(args.target,'tests/native_ecc_remote_receiver_test.cpp'))
    old_fixture=Path(template[template.index("-o")+1]);new_fixture=out/"native-ecc-remote-receiver-tests.o"
    fixture_command=template.copy();fixture_command[fixture_command.index("-c")+1]=str(fixture);fixture_command[fixture_command.index("-o")+1]=str(new_fixture)
    units=[];reused={};dependencies={};commands=[];substitutions={old_fixture:new_fixture} if args.target!="planning" else {}
    # Inspect the exact dependency set of each retained compilation. A changed
    # repository header invalidates that object, rather than unrelated units.
    for obj in [Path(word) for word in link if word.endswith(".o") and (args.target=="planning" or Path(word)!=old_fixture)]:
        recipe=compile_by_object[obj];source=Path(recipe[recipe.index("-c")+1])
        dep=recipe.copy();dep.remove("-c");position=dep.index("-o");del dep[position:position+2];dep += ["-MM","-MT","handoff"]
        output=subprocess.check_output(dep,text=True,cwd=root)
        paths=[Path(word).resolve() for word in shlex.split(output.replace("\\\n"," ").split(":",1)[1])]
        changed=False
        for path in paths:
            digest=sha(path);dependencies[str(path)]=digest
            if root in path.parents and (sdk not in path.parents or str(path.relative_to(root)) in old):
                expected=old.get(str(path.relative_to(root)),generated.get(str(path)))
                if expected!=digest:changed=True
        if changed:
            target=out/obj.name;command=recipe.copy();command[command.index("-o")+1]=str(target)
            substitutions[obj]=target;units.append(command)
        else:
            assert old.get(str(source.relative_to(root)),generated.get(str(source)))==sha(source),str(source)
            reused[str(obj)]=sha(obj)
    if args.target!="planning":units.append(fixture_command)
    # New production translation units are absent from the preserved baseline
    # link. Compile them explicitly rather than resolving against stale code.
    old_sources={Path(command[command.index('-c')+1]).resolve() for command in compile_by_object.values()}
    added=[]
    new_sources=list((root/'apps/ecc/native').glob('*.cpp'))
    if args.target=='handoff':new_sources.extend((root/'apps/ecc/handoff').glob('*.cpp'))
    new_sources.extend(root/'apps/datasupervisor'/name for name in ['receivercycleevidence.cpp','receiverplantransition.cpp','receiverplantransition_support.cpp','receiverplantransition_predecessor.cpp'])
    for source in sorted(new_sources):
        if source.resolve() in old_sources or source.resolve()==fixture.resolve():continue
        command=template.copy();obj=out/(source.name+'.o')
        command[command.index('-c')+1]=str(source);command[command.index('-o')+1]=str(obj)
        units.append(command);added.append(str(obj))
    unit_dependencies={}
    for command in units:
        dep=command.copy();dep.remove('-c');position=dep.index('-o');del dep[position:position+2];dep+=['-MM','-MT','successor']
        output=subprocess.check_output(dep,text=True,cwd=root)
        current=unit_dependencies.setdefault(command[command.index('-o')+1],{})
        for word in shlex.split(output.replace('\\\n',' ').split(':',1)[1]):
            path=Path(word).resolve();digest=sha(path);dependencies[str(path)]=digest;current[str(path)]=digest
    for command in units:
        assert "-std=c++11" in command and "-pedantic-errors" in command and "-fsanitize=address,undefined" in command
    def normalized(command):
        value=command.copy();position=value.index('-o');del value[position:position+2];return tuple(value)
    reuse_record=None
    if args.reuse_overlay:
        prior_path=args.reuse_overlay.resolve(strict=True);cache={};seen=set();chain=[]
        # Reuse is per translation unit. A changed header invalidates every
        # object which currently includes it, while unrelated objects retain
        # their exact compiler recipe, object hash and input hashes.
        # Keep recipes inherited through earlier overlays. Otherwise a small
        # incremental build forgets most of its own verified cache next time.
        current=prior_path
        while current:
            assert current not in seen and len(seen)<32,'Invalid/budgeted overlay ancestry'
            seen.add(current);prior=json.loads(current.read_text());assert prior['sanitized']
            chain.append({'file':str(current),'sha256':sha(current)})
            for command in prior['commands']:
                if '-c' not in command:continue
                obj=Path(command[command.index('-o')+1]);expected=prior['compiled_objects'][str(obj)]
                if not obj.is_file() or sha(obj)!=expected:continue
                cache.setdefault(normalized(command),[]).append((obj,prior['dependencies']))
            parent=prior.get('reuse_overlay');current=Path(parent['file']).resolve(strict=True) if parent else None
            if parent:assert sha(current)==parent['sha256'],'Overlay ancestor receipt changed'
        pending=[]
        for command in units:
            inputs=unit_dependencies[command[command.index('-o')+1]]
            candidate=next((obj for obj,old_inputs in cache.get(normalized(command),[]) if all(old_inputs.get(path)==digest for path,digest in inputs.items())),None)
            if candidate is None:pending.append(command)
            else:
                obj=Path(command[command.index('-o')+1]);substitutions[obj]=candidate;reused[str(candidate)]=sha(candidate)
        units=pending;reuse_record={'file':str(prior_path),'sha256':sha(prior_path),'verified_ancestry':chain}
    def compile_one(command):
        obj=Path(command[command.index("-o")+1])
        with obj.with_suffix(".compile.log").open("w") as log:subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(compile_one,units))
    target=out/({'planning':'getdaq-ecc-server','remote':'native-ecc-remote-receiver-tests','successor':'ecc-successor-identity-tests',
                'network':'ecc-network-service-tests','handoff':'getdaq-ecc-handoff','preparation':'hardware-system-preparation-tests',
                'coordinator':'native-ecc-coordinator-tests','handoff-evidence':'remote-handoff-evidence-tests'}[args.target])
    link[1:1]=added
    def replacement(word):
        if not word.endswith('.o'):return word
        path=Path(word)
        while path in substitutions:path=substitutions[path]
        return str(path)
    link=[replacement(word) for word in link]
    link[link.index("-o")+1]=str(target)
    with (out/"link.log").open("w") as log:subprocess.run(link,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(sha(Path(path))==digest for path,digest in dependencies.items())
    assert all(sha(Path(path))==digest for path,digest in reused.items())
    tracked={path:digest for path,digest in dependencies.items() if root in Path(path).parents}
    extra=[Path(__file__).resolve()]+([fixture] if args.target!="planning" else [])
    for path in extra:tracked[str(path)]=sha(path)
    receipt={"schema":1,"sanitized":True,"compile_count":len(units),"reused_objects":reused,"dependencies":dependencies,
        "tracked_inputs":tracked,"commands":units+[link],"base_commands_sha256":sha(base/"commands.json"),"old_libraries_instrumented":False,
        "program_sha256":sha(target)}
    receipt['compiled_objects']={command[command.index('-o')+1]:sha(Path(command[command.index('-o')+1])) for command in units}
    receipt['reuse_overlay']=reuse_record
    (out/"build-receipt.json").write_text(json.dumps(receipt,indent=2)+"\n")
    print("PASS",args.target,"ASan/UBSan overlay:",len(units),"strict C++11 compiles,",len(reused),"objects with unchanged repository dependencies")


if __name__=="__main__":main()
