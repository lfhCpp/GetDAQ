#!/usr/bin/env python3
"""Actual editable workspace graph copy, isolated edits, and cleanup."""
import argparse,hashlib,json,subprocess,tempfile
from pathlib import Path
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('program',type=Path);parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args();program=args.program.resolve();args.evidence.mkdir(parents=True,exist_ok=True);traces=[]
    def run(arguments,success=True):
        r=subprocess.run([str(program)]+[str(v) for v in arguments],capture_output=True,text=True,timeout=20)
        traces.append({'args':[str(v) for v in arguments],'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        assert (r.returncode==0)==success,traces[-1];return json.loads(r.stdout) if r.stdout.strip().startswith('{') else None
    def snapshot(source,target):
        run(['snapshot',source,target]);return json.loads(target.read_bytes())
    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-workspace-copy-') as directory:
            root=Path(directory);original=root/'original';original.mkdir();nested=original/'nested';nested.mkdir()
            main=original/'main.xcfg';shared=nested/'shared.xcfg';leaf=original/'leaf.xcfg'
            main.write_bytes(b'<Setup><!-- keep --><Board id="0" include="nested/shared.xcfg"/><Board id="1" include="nested/shared.xcfg"/></Setup>')
            shared.write_bytes(b'<Template><Module id="*" include="../leaf.xcfg"/><Module id="0"/></Template>')
            leaf.write_bytes(b'<Other><Group><n type="int" range="[0,9]">2</n><label type="string"><![CDATA[  a   b  ]]></label></Group></Other>')
            original_bytes={p:p.read_bytes() for p in [main,shared,leaf]}
            baseline=snapshot(main,root/'before.json')
            outputs={}
            for mode,count in [('shared',3),('isolated',5)]:
                target=root/mode
                preview=run(['preview-workspace-copy',main,mode,target]);assert preview['saved'] is False and not target.exists()
                assert len(preview['initial_inventory']['files'])==count
                receipt=run(['copy-workspace',main,mode,target]);outputs[mode]=target
                assert receipt['file_count']==count and receipt['hardware_loaded_verified'] is False
                assert not (target/'workspace.incomplete').exists()
                inventory=json.loads((target/'workspace.json').read_bytes())
                assert inventory['complete'] and inventory['sharing']==mode and inventory['root_file']=='main.xcfg'
                for entry in inventory['files']:
                    data=(target/entry['stored']).read_bytes()
                    assert len(data)==int(entry['output_bytes']) and hashlib.sha256(data).hexdigest()==entry['output_sha256']
                copied=snapshot(target/'main.xcfg',root/(mode+'-snapshot.json'))
                assert copied['effective_sha256']==baseline['effective_sha256']
                assert all(Path(p['path']).is_relative_to(target) for p in copied['sources'])
                saved=(target/'workspace.json').read_bytes()
                run(['copy-workspace',main,mode,target],False);assert (target/'workspace.json').read_bytes()==saved
            for mode in ['bad-mode']:
                target=root/'bad-mode';run(['copy-workspace',main,mode,target],False);assert not target.exists()
            run(['copy-workspace',main,'shared',root/'missing-parent/copy'],False)
            assert all(p.read_bytes()==b for p,b in original_bytes.items())
            # Moving originals proves that generated include links stay inside each copy.
            original.rename(root/'original-moved')
            for mode,target in outputs.items():
                moved=snapshot(target/'main.xcfg',root/(mode+'-moved.json'));assert moved['effective_sha256']==baseline['effective_sha256']
                proposal=run(['preview-object-override',target/'main.xcfg','Setup/Board[0]/Module[0]/Group'])
                assert len(proposal['localized_paths'])==(2 if mode=='shared' else 1)
                run(['override-object',target/'main.xcfg','Setup/Board[0]/Module[0]/Group'])
                current=snapshot(target/'main.xcfg',root/(mode+'-edited.json'))
                assert current['effective_sha256']==baseline['effective_sha256']
                # Origin-only inventory hashes are deliberately not immutable runtime bindings.
                inventory=json.loads((target/'workspace.json').read_bytes())
                assert any(hashlib.sha256((target/e['stored']).read_bytes()).hexdigest()!=e['output_sha256'] for e in inventory['files'])
            for p,b in original_bytes.items():
                moved=root/'original-moved'/p.relative_to(original);assert moved.read_bytes()==b
    finally:
        (args.evidence/'processes.json').write_text(json.dumps(traces,indent=2)+'\n')
    scope={'process_cases':len(traces),'program_sha256':hashlib.sha256(program.read_bytes()).hexdigest(),
           'sharing_modes':['shared','isolated'],'initial_values_preserved':True,'originals_unmodified':True,
           'original_directory_moved_then_copies_loaded':True,'temporary_xml_json_directories_deleted':True,
           'hardware_commands':False,'raw_data_transfer':False,'immutable_publication':False}
    (args.evidence/'scope.json').write_text(json.dumps(scope,indent=2)+'\n')
    print('PASS workspace copy CLI:',len(traces),'processes; include graph, isolation, original relocation, edits and cleanup')
if __name__=='__main__':main()

