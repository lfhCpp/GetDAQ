#!/usr/bin/env python3
"""配置仓库同版证据归档/核对；不覆盖旧包，不归档认证文件。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_history(target, current=None):
    manifest = json.loads((target/'files-sha256.json').read_text())
    for name, digest in manifest.items():
        path = target/name
        assert path.is_file() and sha(path) == digest, name
    actual = {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
    assert actual == set(manifest)|{'files-sha256.json'}, 'Unexpected/missing archive file'
    assert not list(target.rglob('credential-private'))
    inputs = json.loads((target/'input-sha256.json').read_text())
    if current:
        for name, digest in inputs.items():
            assert sha(current/name) == digest, name
    print('PASS repository archive:',len(manifest),'files;',len(inputs),'inputs; historical only',current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create', action='store_true')
    parser.add_argument('--historical-only', action='store_true')
    args = parser.parse_args()
    target = root/'build-suite-tests/validation-configuration-repository'
    if not args.create:
        check_history(target, None if args.historical_only else root)
        return
    assert not args.historical_only and not target.exists(), 'Do not replace existing evidence'
    files, inputs = {}, {}
    def add(path, name):
        assert path.is_file() and name not in files, name
        assert '..' not in Path(name).parts, name
        files[name] = path
    def merge(build):
        for name, digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name) == digest, name
            assert name not in inputs or inputs[name] == digest, name
            inputs[name] = digest
    qt = [('upload',root/'build-config-transfer',None,False),
          ('upload-sanitized',Path('/tmp/getdaq-config-transfer-asan'),None,True),
          ('gui',root/'build-suite-tests',60,False),
          ('console',root/'build-suite/apps/envoy',None,False),
          ('ecc-client',root/'build-ecc-system-client',16,False),
          ('ecc-client-sanitized',Path('/tmp/getdaq-ecc-system-client-repository-asan'),16,True),
          ('run',root/'build-native-gui',11,False),
          ('run-sanitized',Path('/tmp/getdaq-native-gui-repository-asan'),11,True)]
    for label, build, count, sanitized in qt:
        receipt = json.loads((build/'build-receipt.json').read_text())
        assert receipt['sanitized'] == sanitized and sha(build/receipt['target']) == receipt['program_sha256'], label
        merge(build)
        add(build/receipt['target'],'qt/'+label+'/'+Path(receipt['target']).name)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt']:
            add(build/name,'qt/'+label+'/'+name)
        for name, digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name) == digest, name
            add(build/name,'qt/'+label+'/generated/'+name)
        if count:
            result = build/'results-repository-tests-final.txt'
            assert f'Totals: {count} passed, 0 failed, 0 skipped' in result.read_text(),label
            add(result,'qt/'+label+'/results-repository-tests-final.txt')
    native = [('ordinary',root/'build-config-repository'),('sanitized',Path('/tmp/getdaq-config-repository-asan'))]
    for label, build in native:
        receipt = json.loads((build/'build-receipt.json').read_text())
        assert receipt['sanitized'] == (label == 'sanitized')
        recipes = json.loads((build/'verified-compile-commands.json').read_text())
        assert len(recipes) == receipt['compile_count'] == 26
        for recipe in recipes:
            assert '-std=c++11' in recipe['command'] and '-pedantic-errors' in recipe['command']
            if label == 'sanitized':
                assert '-fsanitize=address,undefined' in recipe['command']
        merge(build)
        for name, digest in receipt['program_sha256'].items():
            assert sha(build/name) == digest, name
            add(build/name,'native/'+label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','verified-compile-commands.json',
                     'results-configure-verified.txt','results-build-verified.txt']:
            add(build/name,'native/'+label+'/'+name)
        for name, digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name) == digest,name
            add(build/name,'native/'+label+'/generated/'+name)
    evidence = [('core','core-verified-evidence',False),('core-sanitized','core-sanitized-verified-evidence',False),
                ('network','network-archive-evidence',True),('network-sanitized','network-sanitized-archive-evidence',True)]
    secrets = []
    for label, folder, network in evidence:
        scopes = list((root/'build-config-repository'/folder).glob('*/scope.json'))
        assert len(scopes) == 1,folder
        scope_path = scopes[0]
        scope = json.loads(scope_path.read_text())
        script = 'configuration_repository_network_test.py' if network else 'configuration_repository_core_runner.py'
        assert scope['test_script_sha256'] == sha(root/'tests'/script)
        assert not scope['hardware_connected'] and not scope['receiver_executed']
        if network:
            assert scope['physical_host_count'] == 1 and scope['configuration_http_executed'] and not scope['raw_mfm_transferred']
            for name,digest in scope['program_sha256'].items():
                assert sha(Path(name)) == digest,name
            requests = json.loads((scope_path.parent/'ack-loss-requests.json').read_text())
            commits = [r for r in requests if r['path'] == '/v1/commit']
            assert len(commits) == 1 and commits[0]['lost'] and commits[0]['status'] == 200
            processes = json.loads((scope_path.parent/'processes.json').read_text())
            assert any(p['returncode'] != 0 and 'upload' in p['args'] for p in processes)
        else:
            assert scope['returncode'] == 0 and not scope['network_executed']
            assert sha(Path(scope['program'])) == scope['program_sha256']
            assert sha(Path(scope['manifest'])) == scope['manifest_sha256']
            assert 'PASS configuration repository core:' in (scope_path.parent/'result.txt').read_text()
        for path in scope_path.parent.rglob('*'):
            if not path.is_file():
                continue
            if path.name in scope['excluded_evidence_files']:
                secrets.append(path.read_bytes().strip())
            else:
                add(path,'evidence/'+label+'/'+str(path.relative_to(scope_path.parent)))
    for name in ['results-repository-core-final.txt','results-repository-core-sanitized-final.txt',
                 'results-repository-network-final.txt','results-repository-network-sanitized-final.txt']:
        path=root/'build-config-repository'/name
        assert path.read_text().startswith('PASS'),name
        add(path,'results/'+name)
    extras = ['tests/archive_configuration_repository_validation.py','tests/build_configuration_repository_validation.py',
              'tests/configuration_repository_core_runner.py','tests/configuration_repository_network_test.py',
              'tests/ecc_system_client_probe.pro','GetDAQ.pro','README.md','docs/CONFIGURATION_REPOSITORY.md',
              'docs/CONFIGURATION_PUBLICATION.md','docs/ARCHITECTURE.md','docs/OPERATIONS.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    for name in extras:
        inputs[name] = sha(root/name)
    for name in sorted(inputs):
        add(root/name,'verified-sources/'+name)
    for name,path in files.items():
        data=path.read_bytes()
        assert all(secret not in data for secret in secrets if secret),'Credential value present in evidence'
        destination=target/name
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,destination)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    check_history(target,root)


if __name__ == '__main__':
    main()
