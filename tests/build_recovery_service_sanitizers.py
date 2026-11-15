#!/usr/bin/env python3
"""Instrument the new recovery HTTP/process/protocol boundary using the actual CMake recipes."""
import argparse,hashlib,json,shlex,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('sdk',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args()
    sdk=a.sdk.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);root=Path(__file__).resolve().parents[1]
    database=sdk/'compile_commands.json';rows=json.loads(database.read_text());selected=[r for r in rows if 'getdaq-handoff-service.dir/' in r['command']]
    assert len(selected)==4
    protocol=next(r for r in rows if Path(r['file']).resolve()==root/'common/control/coldhandoffprotocol.cpp');selected.append(protocol)
    objects={};dependencies={};commands=[]
    for i,row in enumerate(selected):
        command=shlex.split(row['command']);pos=command.index('-o');old=(Path(row['directory'])/command[pos+1]).resolve();obj=out/('unit-'+str(i)+'.o');objects[old]=obj;command[pos+1]=str(obj)
        command=[x for x in command if not x.startswith(('-O','-std=')) and x not in ('-DNDEBUG','-Wpedantic')]
        command+=['-O1','-g','-std=c++11','-pedantic-errors','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        dep=command.copy();dep.remove('-c');pos=dep.index('-o');del dep[pos:pos+2];dep+=['-MM','-MT','recovery']
        text=subprocess.check_output(dep,cwd=row['directory'],text=True)
        for name in shlex.split(text.replace('\\\n',' ').split(':',1)[1]):
            path=(Path(row['directory'])/name).resolve();dependencies[str(path)]=sha(path)
        with (out/('unit-'+str(i)+'.log')).open('w') as log:subprocess.run(command,cwd=row['directory'],stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append({'directory':row['directory'],'argv':command})
    directory=sdk/'ecc-service';link_file=directory/'CMakeFiles/getdaq-handoff-service.dir/link.txt';link=shlex.split(link_file.read_text());archive={}
    for i,word in enumerate(link):
        if word.endswith('.o'):link[i]=str(objects[(directory/word).resolve()])
        elif word.endswith('.a'):
            path=(directory/word).resolve();archive[str(path)]=sha(path);link[i]=str(path)
    # The direct instrumented object resolves this member ahead of its ordinary archive.
    link.insert(1,str(objects[(Path(protocol['directory'])/shlex.split(protocol['command'])[shlex.split(protocol['command']).index('-o')+1]).resolve()]))
    link[link.index('-o')+1]=str(out/'getdaq-handoff-service');link+=['-fsanitize=address,undefined']
    with (out/'link.log').open('w') as log:subprocess.run(link,cwd=directory,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(sha(p)==d for p,d in dependencies.items()) and all(sha(p)==d for p,d in archive.items())
    receipt={'schema':1,'sanitized':True,'scope':['recovery service','recovery main','generated recovery moc','HTTP server','cold handoff protocol'],
             'native_cli_instrumented':False,'old_libraries_instrumented':False,'commands':commands+[{'directory':str(directory),'argv':link}],
             'dependencies':dependencies,'baseline_archives':archive,'compiled_objects':{str(p):sha(p) for p in objects.values()},
             'recipe_inputs':{str(p):sha(p) for p in [database,link_file,Path(__file__).resolve()]},'program_sha256':sha(out/'getdaq-handoff-service')}
    (out/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS strict C++11 ASan/UBSan recovery service; five instrumented translation units')
if __name__=='__main__':main()
