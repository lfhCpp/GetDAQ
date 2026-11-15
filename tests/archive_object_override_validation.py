#!/usr/bin/env python3
"""Verify current object override source, original models, and small evidence."""
import hashlib,json,re,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build-object-override'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    inputs={};programs={};evidence={}
    def record(path,group):
        path=path.resolve();assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',path
        group[str(path)]=sha(path)
    reference=json.loads((BUILD/'reference/build-receipt.json').read_text())
    assert reference['source_originals_unchanged'] and reference['new_compile_count']==13 and len(reference['reused_ccfg_objects'])==54
    for name,digest in reference['inputs'].items():assert sha(Path(name))==digest;inputs[name]=digest
    for recipe in reference['recipes']+reference['reused_ccfg_objects']:assert sha(Path(recipe['object']))==recipe['object_sha256']
    legacy=Path(reference['program']);assert sha(legacy)==reference['program_sha256'];record(legacy,programs);record(BUILD/'reference/build-receipt.json',evidence)
    for folder in [BUILD/'model',BUILD/'gui',Path('/tmp/getdaq-object-override-model-asan'),Path('/tmp/getdaq-object-override-config-asan')]:
        r=json.loads((folder/'build-receipt.json').read_text())
        for name,digest in json.loads((folder/'source-sha256.json').read_text()).items():assert sha(ROOT/name)==digest;inputs[str(ROOT/name)]=digest
        assert sha(folder/'Makefile')==r['makefile_sha256'] and sha(folder/r['target'])==r['program_sha256']
        for name,digest in json.loads((folder/'generated-sha256.json').read_text()).items():assert sha(folder/name)==digest
        record(folder/r['target'],programs)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','results-verified-build.txt']:record(folder/name,evidence)
    for name in ['GetDAQ','getdaq-sentry','getdaq-simulator','getdaq-config','getdaq-config-check']:record(ROOT/'bin'/name,programs)
    record(ROOT/'build-ice-compat/getdaq-system-config',programs)
    for folder,program in [('cli-ordinary-final',ROOT/'bin/getdaq-config'),('cli-sanitized-final',Path('/tmp/getdaq-object-override-config-asan/getdaq-config-probe'))]:
        s=json.loads((BUILD/folder/'scope.json').read_text())
        assert (s['cases'],s['override_cases'],s['no_op_cases'],s['processes'])==(16,13,3,151)
        assert s['program_sha256']==sha(program) and s['legacy_program_sha256']==sha(legacy)
        assert s['actual_old_model_and_override_undo_redo'] and s['temporary_inputs_outputs_deleted']
        for name in ['scope.json','processes.json']:record(BUILD/folder/name,evidence)
    logs=BUILD/'verified-test-logs';logs.mkdir(exist_ok=True)
    for path,total in [(BUILD/'results-model-final.txt',74),(Path('/tmp/getdaq-object-override-model-asan-final.txt'),74),(BUILD/'results-gui-final-current.txt',74),(BUILD/'results-http-isolated.txt',3)]:
        assert re.search(r'Totals: %d passed, 0 failed'%total,path.read_text()),path
        destination=logs/path.name;shutil.copyfile(path,destination);record(destination,evidence)
    record(BUILD/'results-gui-final.txt',evidence)
    for name in ['control-build.txt','control-build-final.txt','results-system-final.txt']:record(BUILD/name,evidence)
    assert '36 process cases; original source preserved' in (BUILD/'results-system-final.txt').read_text()
    export=json.loads((BUILD/'export-regression-final/scope.json').read_text());assert export['process_cases']==30 and export['program_sha256']==sha(ROOT/'bin/getdaq-config')
    record(Path('build-xml-export/reference/legacy-configuration-probe'),programs)
    assert export['legacy_program_sha256']==sha(ROOT/'build-xml-export/reference/legacy-configuration-probe')
    for name in ['scope.json','processes.json']:record(BUILD/'export-regression-final'/name,evidence)
    scopes=list((BUILD/'snapshot-regression-final').glob('effective-snapshot-*/scope.json'));assert len(scopes)==1
    s=json.loads(scopes[0].read_text());assert s['process_cases']==12 and s['program_sha256']==sha(ROOT/'bin/getdaq-config')
    for path in scopes[0].parent.glob('*.json'):record(path,evidence)
    for name in ['README.md','docs/CONFIGURATION_OBJECT_OVERRIDE.md','docs/CONFIGURATION_STRUCTURE_EDITOR.md','docs/CONFIGURATION_COMMAND_MIGRATION.md',
                 'docs/REFACTOR_STATUS.md','docs/VALIDATION.md','tests/archive_object_override_validation.py','tests/object_override_integration.py',
                 'tests/xml_export_integration.py','tests/effective_snapshot_integration.py','tests/system_experiment_integration.py','GetDAQ.pro',
                 'apps/envoy/envoy.pro','apps/configtool/configtool.pro']:record(ROOT/name,inputs)
    assert not list(BUILD.rglob('*.graw'))
    result={'schema':1,'scope':'source-file shallow object override, concrete projections and actual old model redo/undo/redo',
            'whole_refactor_complete':False,'portable_replay_archive':False,'old_library_instrumented':False,'system_qt_instrumented':False,
            'raw_data_transfer':False,'hardware_connected':False,'multiple_use_include_old_declaration_equivalence_proven':False,
            'inputs':inputs,'programs':programs,'evidence':evidence,
            'test_totals':{'model_each':74,'gui':74,'original_model_cases_each':16,'object_cli_processes_each':151,'export_regression':30,'snapshot_cli':12,'offline_system':36}}
    (BUILD/'checkpoint.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS object override checkpoint:',len(inputs),'inputs;',len(programs),'programs;',len(evidence),'evidence files; no raw payload')
if __name__=='__main__':main()

