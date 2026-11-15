#!/usr/bin/env python3
"""ECC选择阶段证据归档；历史包不覆盖，认证文件/值不保存。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    files=json.loads((target/'files-sha256.json').read_text())
    for name,digest in files.items():assert sha(target/name)==digest,name
    actual={str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
    assert actual==set(files)|{'files-sha256.json'}
    inputs=json.loads((target/'input-sha256.json').read_text())
    if current:
        for name,digest in inputs.items():assert sha(current/name)==digest,name
    assert not list(target.rglob('credential-private')) and not list(target.rglob('credential'))
    print('PASS configuration-selection archive:',len(files),'files;',len(inputs),'inputs; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];target=root/'build-suite-tests/validation-configuration-selection'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:
        verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files,inputs={},{}
    def add(path,name):
        assert path.is_file() and name not in files and '..' not in Path(name).parts,name
        files[name]=path
    def merge(build):
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest,name
            assert name not in inputs or inputs[name]==digest,name
            inputs[name]=digest
    qt=[('gui',root/'build-suite-tests',False,60),('console',root/'build-suite/apps/envoy',False,None),
        ('upload',root/'build-config-transfer',False,None),('upload-sanitized',Path('/tmp/getdaq-config-transfer-asan'),True,None)]
    for label,build,sanitized,count in qt:
        r=json.loads((build/'build-receipt.json').read_text());assert r['sanitized']==sanitized and sha(build/r['target'])==r['program_sha256']
        merge(build);add(build/r['target'],'qt/'+label+'/'+Path(r['target']).name)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt']:add(build/name,'qt/'+label+'/'+name)
        for name,digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name)==digest;add(build/name,'qt/'+label+'/generated/'+name)
        if count:
            result=build/'results-selection-tests-current.txt';assert f'Totals: {count} passed, 0 failed' in result.read_text();add(result,'qt/'+label+'/'+result.name)
    for label,build in [('ordinary',root/'build-config-repository'),('sanitized',Path('/tmp/getdaq-config-repository-asan'))]:
        r=json.loads((build/'build-receipt.json').read_text());assert r['sanitized']==(label=='sanitized') and r['compile_count']==27
        merge(build)
        for name,digest in r['program_sha256'].items():assert sha(build/name)==digest;add(build/name,'repository/'+label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt']:add(build/name,'repository/'+label+'/'+name)
        for name,digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name)==digest;add(build/name,'repository/'+label+'/generated/'+name)
    native=Path('/tmp/getdaq-selection-native-asan');merge(native)
    for name,digest in json.loads((native/'dependency-sha256.json').read_text()).items():assert sha(root/name)==digest,name
    recipes=json.loads((native/'commands.json').read_text());compiles=[r['command'] for r in recipes if '-c' in r['command']]
    assert len(compiles)==74 and all(all(flag in r for flag in ['-std=c++11','-pedantic-errors','-fsanitize=address,undefined']) for r in compiles)
    for name in ['getdaq-ecc-select','getdaq-ecc-server','fixed-ecc-configuration-tests','commands.json','source-sha256.json','dependency-sha256.json','generated-sha256.json']:
        add(native/name,'native-sanitized/'+name)
    for pattern in ['*.compile.txt','*.link.txt','*.generate.txt','moc-*.cpp']:
        for p in native.glob(pattern):add(p,'native-sanitized/'+p.name)
    for name in ['getdaq-ecc-select','getdaq-ecc-server','fixed-ecc-configuration-tests']:add(root/'build-ice-compat'/name,'native-ordinary/'+name)
    add(root/'build-ice-compat/experiment/configuration-selection-tests','pure/ordinary/configuration-selection-tests')
    add(Path('/tmp/getdaq-selection-core-asan/configuration-selection-tests'),'pure/sanitized/configuration-selection-tests')
    add(root/'build-ice-compat/experiment/getdaq-config-check','checker/getdaq-config-check')
    for name in ['results-selection-core-current.txt','results-selection-core-sanitized-current.txt','results-selection-checker-current.txt']:
        add(root/'build-ice-compat'/name,'results/'+name)
    assert '100% tests passed, 0 tests failed out of 1' in (root/'build-ice-compat/results-selection-core-current.txt').read_text()
    assert (root/'build-ice-compat/results-selection-core-sanitized-current.txt').read_text().startswith('PASS')
    secrets=[]
    def evidence(base,pattern,label,kind):
        scopes=list(base.glob(pattern+'/scope.json'));assert len(scopes)==1,(pattern,scopes)
        scope_path=scopes[0];s=json.loads(scope_path.read_text())
        assert not s['hardware_connected'] and not s['receiver_executed']
        if kind=='fixed':
            assert s['returncode']==0 and sha(Path(s['program']))==s['program_sha256']
            assert 'PASS fixed input/publication/selection binding' in (scope_path.parent/'result.txt').read_text()
        else:
            assert s['physical_host_count']==1 and not s['raw_mfm_transferred']
            for name,digest in s['program_sha256'].items():assert sha(Path(name))==digest,name
            script='ecc_configuration_selection_integration.py' if kind=='selection' else 'configuration_repository_network_test.py'
            assert sha(root/'tests'/script)==s['test_script_sha256']
            if kind=='selection':
                choice=json.loads((scope_path.parent/'selection.json').read_text())
                assert not choice['hardware_loaded_verified'] and sha(scope_path.parent/'selection.json')==s['selection_sha256']
                cached=json.loads((scope_path.parent/'ecc-configuration.json').read_text())
                assert cached['selection']==choice and not cached['input_binding_live_verified'] and not cached['hardware_loaded_verified']
            else:
                requests=json.loads((scope_path.parent/'ack-loss-requests.json').read_text())
                commits=[r for r in requests if r['path']=='/v1/commit'];assert len(commits)==1 and commits[0]['lost'] and commits[0]['status']==200
        for p in scope_path.parent.rglob('*'):
            if p.is_file():
                if p.name in s['excluded_evidence_files']:secrets.append(p.read_bytes().strip())
                else:add(p,'evidence/'+label+'/'+str(p.relative_to(scope_path.parent)))
    base=root/'build-ice-compat'
    evidence(base,'selection-fixed-final-*','fixed','fixed')
    evidence(base,'selection-fixed-sanitized-*','fixed-sanitized','fixed')
    evidence(base/'selection-system-final-evidence','selection-system-*','system','selection')
    evidence(base/'selection-system-sanitized-evidence','selection-system-*','system-sanitized','selection')
    evidence(root/'build-config-repository/selection-regression-final-evidence','repository-network-*','repository-regression','repository')
    evidence(root/'build-config-repository/selection-regression-sanitized-final-evidence','repository-network-*','repository-regression-sanitized','repository')
    for folder,names in [('build-ice-compat',['results-selection-system-final.txt','results-selection-system-sanitized-final.txt']),
                         ('build-config-repository',['results-selection-regression-final.txt','results-selection-regression-sanitized-final.txt'])]:
        for name in names:
            assert (root/folder/name).read_text().startswith('PASS');add(root/folder/name,'results/'+name)
    extras=['GetDAQ.pro','apps/configcheck/main.cpp','apps/ecc/native/CMakeLists.txt','common/experiment/CMakeLists.txt',
        'tests/archive_configuration_selection_validation.py','tests/configuration_selection_test.cpp','tests/ecc_configuration_selection_integration.py',
        'tests/configuration_repository_network_test.py','README.md','docs/ECC_CONFIGURATION_SELECTION.md','docs/CONFIGURATION_REPOSITORY.md',
        'docs/ARCHITECTURE.md','docs/ECC_NETWORK_SERVICE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    for name in extras:inputs[name]=sha(root/name)
    for name in sorted(inputs):add(root/name,'verified-sources/'+name)
    for name,p in files.items():
        data=p.read_bytes();assert all(secret not in data for secret in secrets if secret),'Credential value present'
        output=target/name;output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,output)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
