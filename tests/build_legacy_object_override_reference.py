#!/usr/bin/env python3
"""Build unchanged ConfigWidgets model + actual Override command against verified old CCfg objects."""
import argparse,hashlib,json,shlex,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,default=ROOT/'build-xml-export/reference/build-receipt.json')
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build-object-override/reference')
    args=parser.parse_args();build=args.build_dir.resolve();build.mkdir(parents=True,exist_ok=True)
    base=json.loads(args.base.read_text())
    assert base['original_source_unchanged_during_build']
    base_objects=[r for r in base['recipes'] if '/source/cconfig/' in r['source']]
    assert len(base_objects)==54
    for r in base_objects:assert sha(Path(r['object']))==r['object_sha256'],r['object']
    for name,digest in base['inputs'].items():assert sha(Path(name))==digest,('Changed base reference input',name)
    widget=ROOT/'source/cconfig/ConfigWidgets-release-20210208/src'
    utilities=ROOT/'source/mordicus/Utilities-release-20210208/src'
    includes=[f for f in base['flags'] if f.startswith('-I')]
    qt_flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','Qt5Widgets'],text=True))
    qt_libraries=shlex.split(subprocess.check_output(['pkg-config','--libs','Qt5Widgets'],text=True))
    flags=['-std=c++11','-O1','-g','-fPIC']+includes+['-I'+str(widget),'-I'+str(utilities)]+qt_flags
    generated=build/'moc_ConfigModelAccessor.cpp'
    moc_command=['moc']+includes+['-I'+str(widget)]+qt_flags+[str(widget/'CCfg/Model/ConfigModelAccessor.h'),'-o',str(generated)]
    subprocess.run(moc_command,check=True)
    files=['CCfg/Model/ConfigNode.cpp','CCfg/Model/ConfigModelAccessor.cpp','CCfg/Model/AbstractConfigModelAccessor.cpp',
           'CCfg/Model/AbstractCommand.cpp','CCfg/Model/Command/AbstractConfigModelCommand.cpp',
           'CCfg/Model/Command/OverrideConfigNode.cpp','Utils/Error/Exception.cpp','Utils/Error/Reporter.cpp',
           'Utils/Error/AbstractPrinter.cpp','Utils/Error/AbstractFormatter.cpp']
    sources=[widget/f for f in files]+[utilities/'utl/Logging.cpp',generated,ROOT/'tests/legacy_object_override_probe.cpp']
    recipes=[];inputs={Path(p):h for p,h in base['inputs'].items()}
    inputs[Path(__file__).resolve()]=sha(Path(__file__).resolve())
    for i,source in enumerate(sources):
        obj=build/('%02d_%s.o'%(i,source.stem));command=['g++']+flags+['-c',str(source),'-o',str(obj)]
        dependency_command=['g++']+flags+['-MM',str(source)]
        dependency_output=subprocess.check_output(dependency_command,text=True).replace('\\\n',' ')
        dependencies=[Path(p).resolve() for p in shlex.split(dependency_output.split(':',1)[1])]
        for p in dependencies:
            h=sha(p)
            if p in inputs:assert inputs[p]==h,p
            inputs[p]=h
        recipes.append({'source':str(source),'object':str(obj),'command':command,'dependency_command':dependency_command,'dependencies':[str(p) for p in dependencies]})
    before=dict(inputs)
    def compile_one(recipe):
        r=subprocess.run(recipe['command'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        log=Path(recipe['object']).with_suffix('.log');log.write_text(r.stdout);recipe['log']=str(log)
        if r.returncode:raise RuntimeError(str(recipe['source'])+'\n'+r.stdout[-6000:])
        recipe['object_sha256']=sha(Path(recipe['object']));return recipe
    with ThreadPoolExecutor(max_workers=2) as pool:recipes=list(pool.map(compile_one,recipes))
    program=build/'legacy-object-override'
    link=['g++']+[r['object'] for r in recipes]+[r['object'] for r in base_objects]+['-o',str(program)]+qt_libraries
    subprocess.run(link,check=True)
    assert all(sha(p)==h for p,h in before.items()),'Original/reference input changed during build'
    receipt={'scope':'unchanged ConfigWidgets ConfigNode/accessor/OverrideConfigNode redo+undo+redo; unchanged original CCfg/TinyXml',
             'source_originals_unchanged':True,'old_libraries_instrumented':False,'flags':flags,'moc_command':moc_command,
             'new_compile_count':len(recipes),'reused_ccfg_objects':base_objects,'recipes':recipes,'link_command':link,
             'inputs':{str(p):h for p,h in sorted(before.items())},'program':str(program),'program_sha256':sha(program),
             'qt_version':subprocess.check_output(['qmake','-query','QT_VERSION'],text=True).strip()}
    (build/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Built actual old model/Override command:',len(recipes),'compilations;',len(base_objects),'verified CCfg objects;',len(inputs),'inputs;',program)
if __name__=='__main__':main()

