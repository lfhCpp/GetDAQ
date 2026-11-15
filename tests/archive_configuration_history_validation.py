#!/usr/bin/env python3
"""Freeze configuration history tests, five Qt builds and offline CLI evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    manifest=json.loads((target/'files-sha256.json').read_text())
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():assert sha(target/name)==digest,name
    assert not any(p.suffix=='.graw' or 'credential' in p.name.lower() for p in target.rglob('*'))
    inputs=json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current:assert sha(current/name)==digest,name
    for label,count,total in [('model-ordinary',10,131),('model-sanitized',10,131),('gui',86,207),('console',83,204),('cli',34,155)]:
        folder=target/'builds'/label;receipt=json.loads((folder/'build-receipt.json').read_text())
        assert receipt['source_count']==count and receipt['tracked_input_count']==total
        assert receipt['sanitized']==(label=='model-sanitized') and not receipt['system_qt_instrumented']
        assert sha(folder/'program')==receipt['program_sha256'] and sha(folder/'Makefile')==receipt['makefile_sha256']
        for name,digest in json.loads((folder/'source-sha256.json').read_text()).items():assert inputs[name]==digest
        for name,digest in json.loads((folder/'generated-sha256.json').read_text()).items():assert sha(folder/'generated'/name)==digest
        for name,digest in json.loads((folder/'generated-supplement-sha256.json').read_text()).items():assert sha(folder/'generated'/name)==digest
        makefile=(folder/'Makefile').read_text();assert '-std=c++11' in makefile
        if label.startswith('model'):assert '-pedantic-errors' in makefile
        if label=='model-sanitized':assert '-fsanitize=address,undefined' in makefile
    for label in ('model-ordinary','model-sanitized'):
        log=(target/'builds'/label/'results-history-current.txt').read_text()
        assert 'Totals: 44 passed, 0 failed, 0 skipped' in log
        for operation in range(10):assert 'historyAllStructureOperations('+str(operation)+')' in log
    assert 'Totals: 67 passed, 0 failed, 0 skipped' in (target/'builds/gui/results-history-gui-current.txt').read_text()
    scope=json.loads((target/'snapshot-cli/scope.json').read_text())
    assert scope['offline_only'] and not scope['hardware_fixture'] and not scope['raw_data_transfer']
    assert scope['process_cases']==12 and scope['program_sha256']==sha(target/'builds/cli/program')
    assert len(json.loads((target/'snapshot-cli/processes.json').read_text()))==12
    assert 'Totals: 43 passed, 1 failed' in (target/'builds/model-ordinary/results-history-initial.txt').read_text()
    print('PASS configuration history archive:',len(manifest),'files;',len(inputs),'inputs; five builds; no raw payload/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true')
    args=parser.parse_args();target=root/'build-xcfg-history/validation-history'
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files,inputs={},{}
    def add(path,name):
        assert path.is_file() and name not in files and '..' not in Path(name).parts
        assert path.suffix!='.graw' and 'credential' not in path.name.lower();files[name]=path
    builds=[('model-ordinary',root/'build-xcfg-history'),('model-sanitized',Path('/tmp/getdaq-xcfg-history-asan')),
            ('gui',root/'build-suite-tests'),('console',root/'build-suite/apps/envoy'),('cli',root/'build-suite/apps/configtool')]
    for label,build in builds:
        prefix='builds/'+label+'/'
        receipt=json.loads((build/'build-receipt.json').read_text());assert sha(build/receipt['target'])==receipt['program_sha256']
        add(build/receipt['target'],prefix+'program')
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt']:add(build/name,prefix+name)
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest);inputs[name]=digest
        generated=json.loads((build/'generated-sha256.json').read_text())
        extra={path.name:sha(path) for path in build.glob('*.moc') if path.name not in generated}
        supplement=build/'generated-supplement-sha256.json';supplement.write_text(json.dumps(extra,indent=2)+'\n');add(supplement,prefix+supplement.name)
        for name in set(generated)|set(extra):add(build/name,prefix+'generated/'+name)
        if label.startswith('model'):add(build/'results-history-current.txt',prefix+'results-history-current.txt')
        if label=='model-ordinary':add(build/'results-history-initial.txt',prefix+'results-history-initial.txt')
        if label=='gui':add(build/'results-history-gui-current.txt',prefix+'results-history-gui-current.txt')
    roots=list((root/'build-xcfg-history/snapshot-cli-evidence').iterdir());assert len(roots)==1 and roots[0].is_dir()
    for path in roots[0].rglob('*'):
        if path.is_file():add(path,'snapshot-cli/'+str(path.relative_to(roots[0])))
    add(root/'build-xcfg-history/results-history-snapshot-cli.txt','results-history-snapshot-cli.txt')
    for name in ['tests/archive_configuration_history_validation.py','tests/effective_snapshot_integration.py',
                 'docs/CONFIGURATION_UNDO_REDO.md','docs/CONFIGURATION_STRUCTURE_EDITOR.md','docs/VALIDATION.md',
                 'docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir()
    for name,path in files.items():
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    verify(target,root)


if __name__=='__main__':main()
