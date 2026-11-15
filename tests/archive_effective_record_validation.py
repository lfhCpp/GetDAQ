#!/usr/bin/env python3
"""归档标准有效快照语义核心的同版证据；保留旧包，排除运行凭据。"""
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]; target=root/'build-suite-tests/validation-effective-record'
    assert not target.exists(),'Evidence already exists; do not overwrite'
    files={};inputs={}
    def add(path,name):
        assert path.is_file(),path
        assert name not in files,name
        files[name]=path
    qt=[(root/'build-suite-tests',59),(root/'build-xcfg-shared',19),(Path('/tmp/getdaq-xcfg-shared-asan'),19),
        (root/'build-publication-qtcore',5),(Path('/tmp/getdaq-publication-qtcore-asan'),5)]
    for index,(build,count) in enumerate(qt):
        receipt=json.loads((build/'build-receipt.json').read_text())
        assert receipt['sanitized']==(index in (2,4)) and sha(build/receipt['target'])==receipt['program_sha256']
        assert f'Totals: {count} passed, 0 failed, 0 skipped' in (build/'results-effective-record-tests-current.txt').read_text()
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest,name
            assert name not in inputs or inputs[name]==digest,name
            inputs[name]=digest
        for name,digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name)==digest;add(build/name,f'qt/{index}/{name}')
        for name in [receipt['target'],'build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt','results-effective-record-tests-current.txt']:
            add(build/name,f'qt/{index}/{name}')
    native=Path('/tmp/getdaq-effective-record-native-asan')
    for name,digest in json.loads((native/'source-sha256.json').read_text()).items():
        assert sha(root/name)==digest,name
        assert name not in inputs or inputs[name]==digest,name
        inputs[name]=digest
    for name,digest in json.loads((native/'dependency-sha256.json').read_text()).items():assert sha(root/name)==digest,name
    recipes=json.loads((native/'commands.json').read_text());compiles=[r['command'] for r in recipes if '-c' in r['command']]
    assert len(compiles)==71 and all(all(f in row for f in ['-std=c++11','-pedantic-errors','-fsanitize=address,undefined']) for row in compiles)
    for name in ['getdaq-ecc-server','fixed-ecc-configuration-tests','source-sha256.json','dependency-sha256.json','commands.json','generated-sha256.json']:add(native/name,'native/'+name)
    for pattern in ['*.compile.txt','*.link.txt','moc-*.cpp']:
        for path in native.glob(pattern):add(path,'native/'+path.name)
    for name in ['GetDAQ','getdaq-config','getdaq-config-check','getdaq-sentry','getdaq-simulator']:add(root/'build-suite/bin'/name,'programs/'+name)
    for name in ['getdaq-system-config','getdaq-ecc-server','fixed-ecc-configuration-tests']:add(root/'build-ice-compat'/name,'programs/'+name)
    for index,build in enumerate([root/'build-experiment',Path('/tmp/getdaq-publication-core-asan')]):
        for name in ['getdaq-config-check','system-experiment-tests','effective-configuration-record-tests']:add(build/name,f'pure-core/{index}/{name}')
        result=build/'results-effective-record-tests-current.txt';assert '100% tests passed, 0 tests failed out of 2' in result.read_text();add(result,f'pure-core/{index}/{result.name}')
    for folder,limit in [('effective-record-semantic-publication-evidence',20),('effective-record-semantic-publication-sanitized-evidence',20)]:
        scopes=list((root/'build-suite-tests'/folder).glob('configuration-publication-*/scope.json'));assert len(scopes)==1,scopes
        scope=json.loads(scopes[0].read_text());assert scope['offline_only'] and scope['process_cases']==limit and not scope['hardware_connected'] and not scope['receiver_started']
        for name,digest in scope['program_sha256'].items():assert sha(Path(name))==digest,name
        for path in scopes[0].parent.rglob('*'):
            if path.is_file() and path.name not in scope['excluded_evidence_files']:add(path,'cli/'+folder+'/'+str(path.relative_to(scopes[0].parent)))
    for folder,build in [('effective-record-cli-evidence',root/'build-experiment'),('effective-record-cli-sanitized-evidence',Path('/tmp/getdaq-publication-core-asan'))]:
        scopes=list((root/'build-experiment'/folder).glob('effective-record-*/scope.json'));assert len(scopes)==1,scopes
        scope=json.loads(scopes[0].read_text());assert scope['process_cases']==16 and scope['program_sha256']==sha(build/'getdaq-config-check')
        for path in scopes[0].parent.iterdir():add(path,'checker-cli/'+folder+'/'+path.name)
    roots=list((root/'build-ice-compat').glob('effective-record-fixed-*'))
    for mode in ['ordinary','sanitized']:
        matches=[p for p in roots if p.name.startswith('effective-record-fixed-sanitized-')==(mode=='sanitized')];assert len(matches)==1,matches
        evidence=matches[0];scope=json.loads((evidence/'scope.json').read_text());program=native/'fixed-ecc-configuration-tests' if mode=='sanitized' else root/'build-ice-compat/fixed-ecc-configuration-tests'
        assert scope['returncode']==0 and scope['program_sha256']==sha(program)
        for name in ['scope.json','result.txt']:add(evidence/name,'fixed/'+mode+'/'+name)
        add(evidence/'evidence/published-controller/snapshot.json','fixed/'+mode+'/snapshot.json')
    for folder,pattern in [('build-suite-tests','results-effective-record-*.txt'),('build-suite','results-effective-record-*.txt'),('build-experiment','results-effective-record-*.txt'),('build-ice-compat','results-effective-record-*.txt')]:
        for path in (root/folder).glob(pattern):add(path,'results/'+folder+'/'+path.name)
    extra=['GetDAQ.pro','apps/configcheck/main.cpp','apps/configcheck/configcheck.pro','common/experiment/CMakeLists.txt','tests/effective_configuration_record_test.cpp','tests/effective_record_cli_integration.py','tests/archive_effective_record_validation.py',
        'README.md','docs/EFFECTIVE_CONFIGURATION_SNAPSHOTS.md','docs/CONFIGURATION_PUBLICATION.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    for name in extra:inputs[name]=sha(root/name)
    for name in sorted(inputs):add(root/name,'verified-sources/'+name)
    for name,path in files.items():
        destination=target/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()};(target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name,digest in manifest.items():assert sha(target/name)==digest,name
    for name,digest in inputs.items():assert sha(root/name)==digest,name
    assert not list(target.rglob('credential-private'))
    print(f'PASS {len(manifest)} effective-record evidence files; {len(inputs)} current inputs; five Qt builds, 71 native strict compiles, 40 publication and 32 checker processes; credentials excluded')


if __name__=='__main__':main()
