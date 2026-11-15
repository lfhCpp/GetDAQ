#!/usr/bin/env python3
"""Actual CLI export, independent old CCfg export, and temporary-file cleanup."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program',type=Path)
    parser.add_argument('legacy_export',type=Path)
    parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args();program=args.program.resolve();legacy=args.legacy_export.resolve()
    args.evidence.mkdir(parents=True,exist_ok=True);traces=[]

    def run(executable,arguments,success=True):
        result=subprocess.run([str(executable)]+[str(v) for v in arguments],capture_output=True,text=True,timeout=15)
        record={'program':str(executable),'arguments':[str(v) for v in arguments],
                'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
        traces.append(record);assert (result.returncode==0)==success,record

    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-xml-export-') as directory:
            root=Path(directory);main=root/'main.xcfg';external=root/'template.xcfg'
            original=b'<Setup><!-- main --><Board id="*" include="template.xcfg"/><Board id="0"/><Board id="1"><gain unit="mV">3</gain></Board></Setup>'
            included=b'<Template><!-- included --><gain type="int" unit="V" range="[0,9]">2</gain><group id="0"><enabled>true</enabled><name type="string">fast slow</name></group><Empty/></Template>'
            main.write_bytes(original);external.write_bytes(included)
            before=root/'before.json';run(program,['snapshot',main,before]);before_snapshot=json.loads(before.read_bytes())
            for mode in ('declarations','effective'):
                output=root/(mode+'.xcfg');run(program,['export-xml',main,'Setup',mode,output])
                result=output.read_bytes();assert b'include=' not in result
                assert (b'id="*"' in result)==(mode=='declarations')
                snapshot=root/(mode+'.json');run(program,['snapshot',output,snapshot])
                assert json.loads(snapshot.read_bytes())['effective_sha256']==before_snapshot['effective_sha256']
                run(program,['export-xml',main,'Setup',mode,output],False);assert output.read_bytes()==result
            # Three actual original exports; compare independent output semantics.
            reference_comparisons=0
            for index,(path,old_path) in enumerate([('Setup','root'),('Setup/Board[0]','Board[0]'),('Setup/Board[1]','Board[1]')]):
                new_output=root/('new-%d.xcfg'%index);old_output=root/('old-%d.xcfg'%index)
                run(program,['export-xml',main,path,'declarations',new_output]);run(legacy,[main,old_path,old_output])
                new_snapshot=root/('new-%d.json'%index);old_snapshot=root/('old-%d.json'%index)
                run(program,['snapshot',new_output,new_snapshot]);run(program,['snapshot',old_output,old_snapshot])
                assert json.loads(new_snapshot.read_bytes())['effective_sha256']==json.loads(old_snapshot.read_bytes())['effective_sha256']
                reference_comparisons+=1
            concrete=root/'board-effective.xcfg';run(program,['export-xml',main,'Setup/Board[0]','effective',concrete])
            concrete_snapshot=root/'board-effective.json';run(program,['snapshot',concrete,concrete_snapshot])
            effective=json.loads(concrete_snapshot.read_bytes());assert len(effective['scalars'])==3
            gain=next(s for s in effective['scalars'] if s['path']=='Board[0]/gain');assert gain['unit']=='mV' and gain['normalized_value']=='2'
            # Explicit default-root export: only its standalone root loses '*'.
            template=root/'exported-template.xcfg';run(program,['export-xml',main,'Setup/Board[*]','declarations',template])
            assert template.read_bytes().startswith(b'<Board>')
            for path,mode,destination in [('Setup/Board[*]','effective',root/'bad-template.xcfg'),
                    ('Setup/Board[0]/gain','declarations',root/'bad-scalar.xcfg'),
                    ('Setup/Board[8]','effective',root/'bad-instance.xcfg'),
                    ('Setup','bad-mode',root/'bad-mode.xcfg'),
                    ('Setup','declarations',root/'missing-dir/output.xcfg')]:
                run(program,['export-xml',main,path,mode,destination],False);assert not destination.exists()
            run(program,['export-xml',main,'Setup','declarations',main],False)
            assert main.read_bytes()==original and external.read_bytes()==included
            # Significant whitespace survives new output; not an old-writer claim.
            spaced=root/'spaced.xcfg';spaced.write_bytes(b'<Setup><label type="string"><![CDATA[  a   b  ]]></label></Setup>')
            spaced_output=root/'spaced-output.xcfg';spaced_snapshot=root/'spaced.json'
            run(program,['export-xml',spaced,'Setup','effective',spaced_output]);run(program,['snapshot',spaced_output,spaced_snapshot])
            assert json.loads(spaced_snapshot.read_bytes())['scalars'][0]['normalized_value']=='  a   b  '
    finally:
        (args.evidence/'processes.json').write_text(json.dumps(traces,indent=2)+'\n')
    scope={'process_cases':len(traces),'original_export_comparisons':reference_comparisons,
           'program_sha256':hashlib.sha256(program.read_bytes()).hexdigest(),
           'legacy_program_sha256':hashlib.sha256(legacy.read_bytes()).hexdigest(),
           'temporary_xml_json_deleted':True,'raw_data_transfer':False,'hardware_commands':False,
           'old_export_all_inputs_compatible':False,'file_format':'local self-contained XML'}
    (args.evidence/'scope.json').write_text(json.dumps(scope,indent=2)+'\n')
    print('PASS XML export CLI: %d processes, %d actual original export comparisons; temporary XML/JSON deleted' % (len(traces),reference_comparisons))


if __name__=='__main__':main()
