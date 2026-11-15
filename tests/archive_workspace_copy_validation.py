#!/usr/bin/env python3
"""Verify current editable workspace-copy implementation and bounded evidence."""
import hashlib,json,re,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build-workspace-copy'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    inputs={};programs={};evidence={}
    def record(path,group):
        path=path.resolve();assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',path
        group[str(path)]=sha(path)
    for folder in [BUILD/'model',BUILD/'gui',BUILD/'config-cli',Path('/tmp/getdaq-workspace-copy-model-asan'),Path('/tmp/getdaq-workspace-copy-config-asan')]:
        r=json.loads((folder/'build-receipt.json').read_text())
        for name,digest in json.loads((folder/'source-sha256.json').read_text()).items():assert sha(ROOT/name)==digest;inputs[str(ROOT/name)]=digest
        assert sha(folder/'Makefile')==r['makefile_sha256'] and sha(folder/r['target'])==r['program_sha256']
        for name,digest in json.loads((folder/'generated-sha256.json').read_text()).items():assert sha(folder/name)==digest
        record(folder/r['target'],programs)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','results-verified-build.txt']:record(folder/name,evidence)
    for name in ['GetDAQ','getdaq-sentry','getdaq-simulator','getdaq-config','getdaq-config-check']:record(ROOT/'bin'/name,programs)
    record(ROOT/'build-ice-compat/getdaq-system-config',programs)
    for mode,program in [('ordinary',ROOT/'bin/getdaq-config'),('sanitized',Path('/tmp/getdaq-workspace-copy-config-asan/getdaq-config-probe'))]:
        folder=BUILD/('cli-'+mode+'-final');s=json.loads((folder/'scope.json').read_text())
        assert s['process_cases']==19 and s['program_sha256']==sha(program)
        assert s['initial_values_preserved'] and s['originals_unmodified'] and s['original_directory_moved_then_copies_loaded'] and s['temporary_xml_json_directories_deleted']
        for name in ['scope.json','processes.json']:record(folder/name,evidence)
    for folder,old_program,count in [('object-regression-final',ROOT/'build-object-override/reference/legacy-object-override',151),
                                      ('export-regression-final',ROOT/'build-xml-export/reference/legacy-configuration-probe',30)]:
        s=json.loads((BUILD/folder/'scope.json').read_text())
        assert s['program_sha256']==sha(ROOT/'bin/getdaq-config') and s['legacy_program_sha256']==sha(old_program)
        assert s.get('processes',s.get('process_cases'))==count
        record(old_program,programs)
        for name in ['scope.json','processes.json']:record(BUILD/folder/name,evidence)
    logs=BUILD/'verified-test-logs';logs.mkdir(exist_ok=True)
    for path,total in [(BUILD/'results-model-final.txt',78),(Path('/tmp/getdaq-workspace-copy-model-asan-final.txt'),78),(BUILD/'results-gui-final.txt',76)]:
        assert re.search(r'Totals: %d passed, 0 failed'%total,path.read_text()),path
        dest=logs/path.name;shutil.copyfile(path,dest);record(dest,evidence)
    assert '36 process cases; original source preserved' in (BUILD/'results-system-final.txt').read_text()
    record(BUILD/'results-system-final.txt',evidence);record(BUILD/'control-build-final.txt',evidence)
    scopes=list((BUILD/'snapshot-regression-final').glob('effective-snapshot-*/scope.json'));assert len(scopes)==1
    s=json.loads(scopes[0].read_text());assert s['process_cases']==12 and s['program_sha256']==sha(ROOT/'bin/getdaq-config')
    for path in scopes[0].parent.glob('*.json'):record(path,evidence)
    for name in ['README.md','docs/CONFIGURATION_WORKSPACE_COPY.md','docs/CONFIGURATION_COMMAND_MIGRATION.md','docs/CONFIGURATION_EDITOR.md',
                 'docs/REFACTOR_STATUS.md','docs/VALIDATION.md','tests/archive_workspace_copy_validation.py','tests/workspace_copy_integration.py',
                 'tests/object_override_integration.py','tests/xml_export_integration.py','tests/effective_snapshot_integration.py','tests/system_experiment_integration.py',
                 'GetDAQ.pro','apps/envoy/envoy.pro','apps/configtool/configtool.pro']:record(ROOT/name,inputs)
    assert not list(BUILD.rglob('*.graw'))
    c={'schema':1,'scope':'editable include workspace copy and per-reference isolation, GUI/CLI/source checks',
       'whole_refactor_complete':False,'immutable_publication':False,'portable_replay_archive':False,'system_qt_instrumented':False,
       'raw_data_transfer':False,'hardware_connected':False,'inputs':inputs,'programs':programs,'evidence':evidence,
       'test_totals':{'model_each':78,'gui':76,'workspace_cli_each':19,'object_regression':151,'export_regression':30,'snapshot_cli':12,'offline_system':36}}
    (BUILD/'checkpoint.json').write_text(json.dumps(c,indent=2)+'\n')
    print('PASS workspace copy checkpoint:',len(inputs),'inputs;',len(programs),'programs;',len(evidence),'small evidence files; no raw payload')
if __name__=='__main__':main()

