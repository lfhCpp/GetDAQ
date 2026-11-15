#!/usr/bin/env python3
"""Archive native JSON dependency migration, excluding credentials and raw data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    files=json.loads((target/'files-sha256.json').read_text())
    for name,digest in files.items():assert sha(target/name)==digest,name
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    assert not any(p.suffix=='.graw' for p in target.rglob('*'))
    for folder in ('network','fixed'):
        assert not any('credential' in p.name.lower() for p in (target/folder).rglob('*') if p.is_file())
    inputs=json.loads((target/'input-sha256.json').read_text())
    if current:
        for name,digest in inputs.items():assert sha(current/name)==digest,name
    for label,sanitized in [('journal',False),('journal-sanitized',True)]:
        folder=target/label;receipt=json.loads((folder/'build-receipt.json').read_text())
        assert receipt['sanitized']==sanitized and receipt['compile_count']==24
        for name,digest in receipt['program_sha256'].items():assert sha(folder/name)==digest
        for name in ('journal','state','cli'):assert (folder/('results-json-'+name+'-current.txt')).read_text().startswith('PASS')
    commands=[r['command'] for r in json.loads((target/'native-sanitized/commands.json').read_text())]
    assert len([c for c in commands if '-c' in c])==75
    assert not any('yaml' in str(c).lower() for c in commands)
    compatibility=json.loads((target/'compatibility/results.json').read_text())
    assert compatibility['inspection_bytes_equal'] and compatibility['read_only_unchanged']
    assert sha(target/'journal/getdaq-ecc-journal')==compatibility['current_program_sha256']
    assert sha(target/'legacy/legacy-journal')==compatibility['legacy_program_sha256']
    assert sha(target/'compatibility/history.jsonl')==compatibility['journal_sha256']
    for label in ('ordinary','sanitized'):
        folder=target/'network'/label;results=json.loads((folder/'results.json').read_text())
        assert len(results)==2 and all(r['exit_code']==0 for r in results)
        cleanup=json.loads((folder/'payload-cleanup.json').read_text());assert cleanup['deleted_bytes']==131072
        for result in results:
            scope=json.loads((folder/result['scenario']/'scope.json').read_text())
            assert scope['passed'] and scope['real_receivers'] and not scope['real_hardware']
    print('PASS native JSON archive:',len(files),'files;',len(inputs),'inputs; no raw data/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];target=root/'build-ecc-json/validation-native-json'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    copies,inputs={},{}
    secrets=[]
    def add(path,name):
        assert path.is_file() and name not in copies and '..' not in Path(name).parts
        assert path.suffix!='.graw'
        if name.startswith(('network/','fixed/')):assert 'credential' not in path.name.lower()
        copies[name]=path
    def merge(build):
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest,name
            assert name not in inputs or inputs[name]==digest,name
            inputs[name]=digest
    for label,build in [('journal',root/'build-ecc-json'),('journal-sanitized',Path('/tmp/getdaq-ecc-json-asan'))]:
        merge(build);receipt=json.loads((build/'build-receipt.json').read_text())
        for name in receipt['program_sha256']:add(build/name,label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt']:
            add(build/name,label+'/'+name)
        for name in ('journal','state','cli'):add(build/('results-json-'+name+'-current.txt'),label+'/results-json-'+name+'-current.txt')
    native=Path('/tmp/getdaq-native-json-asan');merge(native)
    for name,digest in json.loads((native/'dependency-sha256.json').read_text()).items():assert sha(root/name)==digest
    for name in ['getdaq-ecc-server','ecc-network-service-tests','fixed-ecc-configuration-tests','topology-json-tests',
                 'source-sha256.json','dependency-sha256.json','commands.json','generated-sha256.json',
                 'topology-json-build-commands.json','results-native-json-topology-current.txt','results-native-json-fixed-current.txt']:
        add(native/name,'native-sanitized/'+name)
    for pattern in ('*.compile.txt','*.link.txt','*.generate.txt','moc-*.cpp'):
        for path in native.glob(pattern):add(path,'native-sanitized/'+path.name)
    for label,folder in [('ordinary','native-json-network-evidence'),('sanitized','native-json-network-sanitized-evidence')]:
        parent=root/'build-ice-compat'/folder;matches=list(parent.glob('ecc-network-system-*'));assert len(matches)==1
        base=matches[0]
        for path in base.rglob('*'):
            if path.is_file() and 'credential' in path.name.lower():secrets.append(path.read_bytes().strip())
            if path.is_file() and path.suffix!='.graw' and 'credential' not in path.name.lower():add(path,'network/'+label+'/'+str(path.relative_to(base)))
        add(parent/'payload-cleanup.json','network/'+label+'/payload-cleanup.json')
    for label,name in [('ordinary','native-json-fixed-current'),('sanitized','native-json-fixed-sanitized-current')]:
        base=root/'build-ice-compat'/name
        for path in base.rglob('*'):
            if path.is_file() and 'credential' in path.name.lower():secrets.append(path.read_bytes().strip())
            elif path.is_file():add(path,'fixed/'+label+'/'+str(path.relative_to(base)))
    for name in ['getdaq-ecc-server','ecc-network-service-tests','getdaq-supervisor','getdaq-align','fixed-ecc-configuration-tests','hardware-alignment-tests','topology-json-tests']:
        add(root/'build-ice-compat'/name,'native-ordinary/'+name)
    add(root/'build-receiver/getdaq-receiver','native-ordinary/getdaq-receiver')
    for path in (root/'build-ecc-json/legacy-reference').rglob('*'):
        if path.is_file():add(path,'legacy/'+str(path.relative_to(root/'build-ecc-json/legacy-reference')))
    for path in (root/'build-ecc-json/compatibility-evidence').rglob('*'):
        if path.is_file():add(path,'compatibility/'+path.name)
    for name in ['results-native-json-build.txt','results-native-json-fixture-build.txt','results-json-topology-build.txt',
                 'results-native-json-topology-current.txt','results-native-json-fixed-current.txt','results-native-json-alignment-current.txt',
                 'results-native-json-network-current.txt','results-native-json-network-sanitized-current.txt']:
        add(root/'build-ice-compat'/name,'results/'+name)
    extras=['apps/ecc/CMakeLists.txt','apps/hardware/CMakeLists.txt','third_party/ice-compat/CMakeLists.txt',
            'tests/topology_json_test.cpp','tests/build_ecc_json_validation.py','tests/archive_native_json_validation.py',
            'tests/ecc_journal_test.cpp','tests/ecc_journal_integration.py','tests/run_ecc_network_service_tests.py',
            'third_party/dependencies.json','third_party/README.md','README.md','docs/NATIVE_JSON_DEPENDENCIES.md',
            'docs/ECC_JOURNAL.md','docs/SYSTEM_ALIGNMENT.md','docs/DEPENDENCIES_AND_OFFLINE_BUILD.md','docs/ARCHITECTURE.md',
            'docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    for name in extras:inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir(parents=True)
    for name,path in copies.items():
        assert all(secret not in path.read_bytes() for secret in secrets if secret),'Credential value present in '+name
        output=target/name;output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,output)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    files={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
