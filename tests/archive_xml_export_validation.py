#!/usr/bin/env python3
"""Bind verified XML export results to current source/program hashes."""
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-xml-export'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    inputs={};programs={};evidence={}
    def record(path,group):
        path=path.resolve();assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',path
        group[str(path)]=sha(path)
    for folder in [BUILD/'model',BUILD/'gui',Path('/tmp/getdaq-xml-export-model-asan'),Path('/tmp/getdaq-xml-export-config-asan')]:
        receipt=json.loads((folder/'build-receipt.json').read_text())
        for name,digest in json.loads((folder/'source-sha256.json').read_text()).items():
            path=ROOT/name;assert sha(path)==digest,('Changed Qt input',path);inputs[str(path)]=digest
        assert sha(folder/'Makefile')==receipt['makefile_sha256']
        assert sha(folder/receipt['target'])==receipt['program_sha256'];record(folder/receipt['target'],programs)
        for name,digest in json.loads((folder/'generated-sha256.json').read_text()).items():assert sha(folder/name)==digest
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','results-verified-build.txt']:record(folder/name,evidence)
    reference=BUILD/'reference';receipt=json.loads((reference/'build-receipt.json').read_text())
    assert len(receipt['recipes'])==55 and receipt['original_source_unchanged_during_build']
    for name,digest in receipt['inputs'].items():assert sha(Path(name))==digest;inputs[name]=digest
    for recipe in receipt['recipes']:assert sha(Path(recipe['object']))==recipe['object_sha256']
    old=Path(receipt['program']);assert sha(old)==receipt['program_sha256'];record(old,programs);record(reference/'build-receipt.json',evidence)
    for name in ['GetDAQ','getdaq-sentry','getdaq-simulator','getdaq-config','getdaq-config-check']:record(ROOT/'bin'/name,programs)
    record(ROOT/'build-ice-compat/getdaq-system-config',programs)
    for folder,program in [('cli-ordinary-final',ROOT/'bin/getdaq-config'),('cli-sanitized-final',Path('/tmp/getdaq-xml-export-config-asan/getdaq-config-probe'))]:
        scope=json.loads((BUILD/folder/'scope.json').read_text())
        assert scope['process_cases']==30 and scope['original_export_comparisons']==3
        assert scope['program_sha256']==sha(program) and scope['legacy_program_sha256']==sha(old)
        assert scope['temporary_xml_json_deleted'] and not scope['raw_data_transfer'] and not scope['hardware_commands']
        for name in ['scope.json','processes.json']:record(BUILD/folder/name,evidence)
    verified=BUILD/'verified-test-logs';verified.mkdir(exist_ok=True)
    for path,total in [(BUILD/'results-model-initial.txt',67),(Path('/tmp/getdaq-xml-export-model-asan-results.txt'),67),(BUILD/'results-gui-final-current.txt',73)]:
        assert re.search(r'Totals: %d passed, 0 failed'%total,path.read_text()),path
        destination=verified/path.name;shutil.copyfile(path,destination);record(destination,evidence)
    assert '36 process cases; original source preserved' in (BUILD/'results-system-final.txt').read_text();record(BUILD/'results-system-final.txt',evidence)
    scopes=list((BUILD/'snapshot-cli-final').glob('effective-snapshot-*/scope.json'));assert len(scopes)==1
    snapshot_scope=json.loads(scopes[0].read_text());assert snapshot_scope['process_cases']==12 and snapshot_scope['program_sha256']==sha(ROOT/'bin/getdaq-config')
    for path in scopes[0].parent.glob('*.json'):record(path,evidence)
    for name in ['control-build.txt','control-build-final.txt']:record(BUILD/name,evidence)
    for name in ['README.md','docs/VALIDATION.md','docs/REFACTOR_STATUS.md','docs/CONFIGURATION_EDITOR.md',
                 'docs/CONFIGURATION_XML_EXPORT.md','docs/CONFIGURATION_COMMAND_MIGRATION.md',
                 'tests/xml_export_integration.py','tests/archive_xml_export_validation.py',
                 'tests/effective_snapshot_integration.py','tests/system_experiment_integration.py',
                 'apps/envoy/envoy.pro','apps/configtool/configtool.pro','GetDAQ.pro',
                 'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/Command/Commands.h',
                 'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/Command/OverrideConfigNode.cpp',
                 'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/ConfigModelAccessor.cpp',
                 'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/ConfigModel.cpp',
                 'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Control/ConfigEditorController.cpp']:
        record(ROOT/name,inputs)
    assert not list(BUILD.rglob('*.graw'))
    checkpoint={'schema':1,'scope':'current self-contained XML export/Save As editor and original export comparison',
                'not_a_portable_replay_archive':True,'whole_refactor_complete':False,
                'hardware_commands':False,'raw_data_transfer':False,'old_library_instrumented':False,'system_qt_instrumented':False,
                'inputs':inputs,'programs':programs,'evidence':evidence,
                'test_totals':{'model_each':67,'gui':73,'xml_cli_each':30,'original_export_comparisons_each':3,'snapshot_cli':12,'offline_system':36}}
    (BUILD/'checkpoint.json').write_text(json.dumps(checkpoint,indent=2)+'\n')
    print('PASS current XML export checkpoint: %d inputs; %d programs; %d small evidence files; no raw payload'%(len(inputs),len(programs),len(evidence)))


if __name__=='__main__':main()
