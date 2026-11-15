#!/usr/bin/env python3
"""Actual original model/Override/undo/redo versus new source-preserving shallow override."""
import argparse,hashlib,json,subprocess,tempfile
from pathlib import Path
import xml.etree.ElementTree as ET

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program',type=Path);parser.add_argument('legacy',type=Path);parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args();program=args.program.resolve();legacy=args.legacy.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    traces=[];cases=[]
    def add(name,body,tail,suffix):
        cases.append({'name':name,'files':{'main.xcfg':'<Setup><Board id="*">'+body+'</Board><Board id="0">'+tail+'</Board></Setup>'},
                      'path':'Setup/Board[0]/'+suffix,'override':True})
    add('simple','<Module><gain>2</gain></Module>','','Module')
    add('nested','<Module><Group><gain>2</gain></Group><Other/></Module>','','Module/Group')
    add('deep','<Module><Group><Inner><gain>2</gain></Inner></Group></Module>','','Module/Group/Inner')
    add('default-siblings','<Module id="*"><gain>2</gain></Module><Module id="1"/><Module id="2"/>','','Module[*]')
    add('concrete-sibling','<Module id="*"><gain>2</gain></Module><Module id="1"/><Module id="2"/>','','Module[1]')
    add('source-order','<Module id="2"/><Other/><Module id="10"/><Module id="*"><gain>2</gain></Module><Module id="1"/>','','Module[*]')
    add('existing-local-sibling','<Module id="*"><gain>2</gain></Module><Module id="1"/><Module id="2"/>','<Module id="1"><gain>4</gain></Module>','Module[*]')
    add('nested-default-siblings','<Module><Group id="*"><gain>2</gain></Group><Group id="1"/><Group id="2"/></Module>','','Module/Group[*]')
    add('both-default-levels','<Module id="*"><Group id="*"><gain>2</gain></Group><Group id="1"/><Group id="2"/></Module><Module id="1"/><Module id="2"/>','','Module[*]/Group[*]')
    add('comments','<!-- pre --><Module><gain type="int" unit="mV" range="[0,9]">2</gain><!-- post --></Module>','','Module')
    for name,xml,path,foreign,enabled in [
        ('inherited-include','<Setup><Board id="*" include="template.xcfg"/><Board id="0"/></Setup>','Setup/Board[0]/Module',
         '<Template><Module><gain>2</gain></Module></Template>',True),
        ('clear-cloned-include','<Setup><Board id="*"><Module include="template.xcfg"/></Board><Board id="0"/></Setup>','Setup/Board[0]/Module',
         '<Template><Group><gain>2</gain></Group></Template>',True),
        ('foreign-parent','<Setup><Board id="0" include="template.xcfg"/></Setup>','Setup/Board[0]/Module[0]/Group',
         '<Template><Module id="*"><Group><gain>2</gain></Group></Module><Module id="0"/></Template>',True),
        ('direct-include-no-op','<Setup><Board id="0" include="template.xcfg"/></Setup>','Setup/Board[0]/Module',
         '<Template><Module><gain>2</gain></Module></Template>',False),
        ('direct-no-op','<Setup><Board id="0"><Module><gain>2</gain></Module></Board></Setup>','Setup/Board[0]/Module','<Template/>',False),
        ('root-no-op','<Setup><gain>2</gain></Setup>','Setup','<Template/>',False)]:
        cases.append({'name':name,'files':{'main.xcfg':xml,'template.xcfg':foreign},'path':path,'override':enabled})

    def run(executable,arguments,success=True):
        r=subprocess.run([str(executable)]+[str(v) for v in arguments],capture_output=True,text=True,timeout=20)
        traces.append({'program':str(executable),'args':[str(v) for v in arguments],'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        assert (r.returncode==0)==success,traces[-1];return r.stdout
    def snapshot(path,output):
        run(program,['snapshot',path,output]);return json.loads(output.read_bytes())
    def objects(path):
        paths=[]
        def visit(node,prefix):
            segment=node.tag+('['+node.attrib['id']+']' if 'id' in node.attrib else '')
            key=prefix+'/'+segment if prefix else segment
            scalar='type' in node.attrib or (len(node)==0 and bool((node.text or '').strip()))
            if not scalar:
                paths.append(key)
                for child in node:visit(child,key)
        visit(ET.parse(path).getroot(),'');return paths
    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-object-override-') as directory:
            root=Path(directory)
            for case in cases:
                folder=root/case['name'];folder.mkdir();source={}
                for name,text in case['files'].items():
                    p=folder/name;p.write_text(text);source[p]=p.read_bytes()
                main=folder/'main.xcfg';before=snapshot(main,folder/'before.json')
                old_prefix=folder/'old'
                old_stdout=run(legacy,[main,case['path'].replace('/','.'),old_prefix])
                assert ('override succeeded=1' in old_stdout)==case['override'],case['name']
                # Old undo/redo operate in the actual unchanged model. Disk originals remain unchanged.
                assert all(p.read_bytes()==b for p,b in source.items())
                undo=snapshot(folder/'old.undo.xcfg',folder/'old-undo.json')
                after=snapshot(folder/'old.after.xcfg',folder/'old-after.json')
                redo=snapshot(folder/'old.redo.xcfg',folder/'old-redo.json')
                assert before['effective_sha256']==undo['effective_sha256']==after['effective_sha256']==redo['effective_sha256'],case['name']
                assert objects(folder/'old.after.xcfg')==objects(folder/'old.redo.xcfg'),case['name']
                if not case['override']:
                    run(program,['preview-object-override',main,case['path']],False)
                    run(program,['override-object',main,case['path']],False)
                    assert all(p.read_bytes()==b for p,b in source.items());continue
                preview=json.loads(run(program,['preview-object-override',main,case['path']]))
                assert not preview['saved'] and preview['created_paths'] and not preview['review']['effective_changed']
                assert all(p.read_bytes()==b for p,b in source.items())
                applied=json.loads(run(program,['override-object',main,case['path']]))
                assert applied['saved'] and applied['created_paths']==preview['created_paths']
                actual=snapshot(main,folder/'new-after.json')
                assert actual['effective_sha256']==before['effective_sha256'],case['name']
                exported=folder/'new-export.xcfg';run(program,['export-xml',main,'Setup','declarations',exported])
                assert objects(exported)==objects(folder/'old.after.xcfg'),(case['name'],objects(exported),objects(folder/'old.after.xcfg'))
                destination=Path(applied['destination_file'])
                assert destination in source and all(p.read_bytes()==b for p,b in source.items() if p!=destination)
                run(program,['override-object',main,case['path']],False)
    finally:
        (args.evidence/'processes.json').write_text(json.dumps(traces,indent=2)+'\n')
    scope={'cases':len(cases),'override_cases':sum(c['override'] for c in cases),'no_op_cases':sum(not c['override'] for c in cases),
           'processes':len(traces),'program_sha256':hashlib.sha256(program.read_bytes()).hexdigest(),
           'legacy_program_sha256':hashlib.sha256(legacy.read_bytes()).hexdigest(),
           'actual_old_model_and_override_undo_redo':True,'temporary_inputs_outputs_deleted':True,
           'hardware_commands':False,'raw_data_transfer':False,'multiple_use_include_declaration_equivalence_tested':False}
    (args.evidence/'scope.json').write_text(json.dumps(scope,indent=2)+'\n')
    print('PASS object override: %d cases, %d actual operations, %d direct no-ops, %d processes; original model/undo/redo and ordered declaration trees; temp files deleted'%
          (len(cases),scope['override_cases'],scope['no_op_cases'],len(traces)))
if __name__=='__main__':main()

