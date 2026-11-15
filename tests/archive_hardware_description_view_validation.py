#!/usr/bin/env python3
"""Preserve shared-description model, viewer and legacy comparison evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    files=json.loads((target/'files-sha256.json').read_text())
    for name,digest in files.items(): assert sha(target/name)==digest,name
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    inputs=json.loads((target/'input-sha256.json').read_text())
    if current:
        for name,digest in inputs.items(): assert sha(current/name)==digest,name
    for label,sanitized in [('core',False),('core-sanitized',True)]:
        folder=target/label;receipt=json.loads((folder/'build-receipt.json').read_text())
        assert receipt['sanitized']==sanitized and receipt['compile_count']==21
        assert sha(folder/'hardware-description-tests')==receipt['program_sha256']
        assert (folder/'results-core-current.txt').read_text().startswith('PASS hardware description pure C++11')
    for label in ('gui','console','cli'):
        folder=target/'qt'/label;receipt=json.loads((folder/'build-receipt.json').read_text())
        assert sha(folder/'program')==receipt['program_sha256'] and not receipt['sanitized']
        if label=='gui':assert 'Totals: 65 passed, 0 failed, 0 skipped' in (folder/'results-hardware-view-current.txt').read_text()
    old=json.loads((target/'legacy/build-receipt.json').read_text())
    assert sha(target/'legacy/legacy-description')==old['program_sha256']
    comparison=json.loads((target/'results/results-legacy-comparison.json').read_text())
    assert comparison['legacy_program_sha256']==old['program_sha256']
    assert comparison['current_program_sha256']==sha(target/'core/hardware-description-tests')
    assert len(comparison['results'])==2 and all(r['bytes_equal'] for r in comparison['results'])
    records=json.loads((target/'compiled-reference/results.json').read_text())
    assert len(records)==3 and all(r['policy_bytes_equal'] for r in records)
    for r in records:assert sha(target/'compiled-reference'/(r['name']+'.json'))==r['compiled_sha256']
    assert (target/'results/results-hardware-model-current.txt').read_text().count('PASS ')==16
    assert 'PASS description pipeline' in (target/'results/results-description-integration-current.txt').read_text()
    assert not any(p.suffix=='.graw' or 'credential' in p.name.lower() for p in target.rglob('*'))
    print('PASS hardware-description viewer archive:',len(files),'files;',len(inputs),'inputs; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];target=root/'build-hardware-description/validation-view'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    copies,inputs={},{}
    def add(path,name):
        assert path.is_file() and name not in copies and '..' not in Path(name).parts
        copies[name]=path
    def merge(build):
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest,name
            assert name not in inputs or inputs[name]==digest,name
            inputs[name]=digest
    for label,build in [('core',root/'build-hardware-description'),('core-sanitized',Path('/tmp/getdaq-hardware-description-asan'))]:
        merge(build)
        for name in ['hardware-description-tests','build-receipt.json','source-sha256.json','verified-compile-commands.json',
                     'results-configure-verified.txt','results-build-verified.txt','results-core-current.txt']:
            add(build/name,label+'/'+name)
    for label,build in [('gui',root/'build-suite-tests'),('console',root/'build-suite/apps/envoy'),('cli',root/'build-suite/apps/configtool')]:
        merge(build);receipt=json.loads((build/'build-receipt.json').read_text())
        assert sha(build/receipt['target'])==receipt['program_sha256']
        add(build/receipt['target'],'qt/'+label+'/program')
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt']:
            add(build/name,'qt/'+label+'/'+name)
        for name,digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name)==digest;add(build/name,'qt/'+label+'/generated/'+name)
        if label=='gui':add(build/'results-hardware-view-current.txt','qt/gui/results-hardware-view-current.txt')
    for folder in ('legacy-reference','compiled-reference'):
        for p in (root/'build-hardware-description'/folder).rglob('*'):
            if p.is_file():add(p,('legacy' if folder=='legacy-reference' else folder)+'/'+str(p.relative_to(root/'build-hardware-description'/folder)))
    for name in ['results-legacy-comparison.json','results-description-integration-current.txt','native-current-receipt.json']:
        add(root/'build-hardware-description'/name,'results/'+name)
    add(root/'build-ice-compat/results-hardware-model-current.txt','results/results-hardware-model-current.txt')
    native=json.loads((root/'build-hardware-description/native-current-receipt.json').read_text())
    for name,digest in native['program_sha256'].items():
        assert sha(root/'build-ice-compat'/name)==digest
        add(root/'build-ice-compat'/name,'native-ordinary/'+name)
    for name,digest in native['source_sha256'].items():
        assert sha(root/name)==digest
        assert name not in inputs or inputs[name]==digest
        inputs[name]=digest
    extras=['tests/archive_hardware_description_view_validation.py','tests/build_native_ecc_sanitizers.py',
            'tests/description_integration.py','apps/hardware/CMakeLists.txt','common/hardware/CMakeLists.txt',
            'README.md','docs/HARDWARE_DESCRIPTION_VIEW.md','docs/HARDWARE_DESCRIPTIONS.md',
            'docs/DEPENDENCIES_AND_OFFLINE_BUILD.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md',
            'tests/data/cobo-hardware.json','tests/data/mutant-hardware.json']
    for name in extras:inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir(parents=True)
    for name,path in copies.items():
        output=target/name;output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,output)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    files={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
