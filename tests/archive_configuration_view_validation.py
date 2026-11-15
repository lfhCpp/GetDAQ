#!/usr/bin/env python3
"""配置查看/Run绑定阶段证据归档；保留旧包，排除实际认证值。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    manifest=json.loads((target/'files-sha256.json').read_text())
    for name,digest in manifest.items():assert sha(target/name)==digest,name
    actual={str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
    assert actual==set(manifest)|{'files-sha256.json'}
    inputs=json.loads((target/'input-sha256.json').read_text())
    if current:
        for name,digest in inputs.items():assert sha(current/name)==digest,name
    assert not any('credential' in p.name.lower() and p.is_file() for p in (target/'network').rglob('*'))
    print('PASS configuration-view archive:',len(manifest),'files;',len(inputs),'inputs; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];target=root/'build-suite-tests/validation-configuration-view'
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
            assert name not in inputs or inputs[name]==digest
            inputs[name]=digest
    qt=[('gui',root/'build-suite-tests',False,61,'current'),
        ('console',root/'build-suite/apps/envoy',False,None,''),
        ('client',root/'build-ecc-system-client',False,18,'current'),
        ('client-sanitized',Path('/tmp/getdaq-configuration-view-client-asan'),True,18,'current'),
        ('run',root/'build-native-gui',False,14,'final'),
        ('run-sanitized',Path('/tmp/getdaq-configuration-view-run-asan'),True,14,'final'),
        ('probe',root/'build-native-gui-probe',False,None,''),
        ('probe-sanitized',Path('/tmp/getdaq-configuration-view-probe-asan'),True,None,'')]
    for label,build,sanitized,count,suffix in qt:
        receipt=json.loads((build/'build-receipt.json').read_text())
        assert receipt['sanitized']==sanitized and sha(build/receipt['target'])==receipt['program_sha256'],label
        merge(build);add(build/receipt['target'],'qt/'+label+'/'+Path(receipt['target']).name)
        for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt']:add(build/name,'qt/'+label+'/'+name)
        for name,digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name)==digest;add(build/name,'qt/'+label+'/generated/'+name)
        if count:
            result=build/('results-configuration-view-'+suffix+'.txt')
            assert f'Totals: {count} passed, 0 failed' in result.read_text(),label
            add(result,'qt/'+label+'/'+result.name)
    native=Path('/tmp/getdaq-configuration-view-native-asan');merge(native)
    recipes=json.loads((native/'commands.json').read_text());compiles=[r['command'] for r in recipes if '-c' in r['command']]
    assert len(compiles)==74 and all(all(f in r for f in ['-std=c++11','-pedantic-errors','-fsanitize=address,undefined']) for r in compiles)
    for name,digest in json.loads((native/'dependency-sha256.json').read_text()).items():assert sha(root/name)==digest
    for name in ['getdaq-ecc-server','ecc-network-service-tests','source-sha256.json','dependency-sha256.json','commands.json','generated-sha256.json']:add(native/name,'native-sanitized/'+name)
    for pattern in ['*.compile.txt','*.link.txt','*.generate.txt','moc-*.cpp']:
        for p in native.glob(pattern):add(p,'native-sanitized/'+p.name)
    for name in ['getdaq-ecc-server','ecc-network-service-tests','getdaq-supervisor']:add(root/'build-ice-compat'/name,'native-ordinary/'+name)
    add(root/'build-receiver/getdaq-receiver','programs/getdaq-receiver-ordinary')
    add(root/'build-ice-compat/control-protocol/ecc-protocol-tests','protocol/ordinary/ecc-protocol-tests')
    add(Path('/tmp/getdaq-configuration-view-protocol-asan/ecc-protocol-tests'),'protocol/sanitized/ecc-protocol-tests')
    assert '100% tests passed, 0 tests failed out of 1' in (root/'build-ice-compat/results-configuration-view-protocol-current.txt').read_text()
    assert (root/'build-ice-compat/results-configuration-view-protocol-sanitized-current.txt').read_text().startswith('PASS')
    secrets=[]
    for label,folder in [('ordinary','configuration-view-network-evidence'),('sanitized','configuration-view-network-sanitized-final-evidence')]:
        bases=list((root/'build-ice-compat'/folder).glob('ecc-network-system-*'));assert len(bases)==1
        base=bases[0];results=json.loads((base/'results.json').read_text());assert len(results)==2 and all(r['exit_code']==0 for r in results)
        for name,record in json.loads((base/'program-sha256.json').read_text()).items():assert sha(Path(record['path']))==record['sha256'],name
        for result in results:
            scope=json.loads((base/result['scenario']/'scope.json').read_text())
            assert scope['passed'] and scope['real_qt_runcontrol'] and scope['real_receivers'] and scope['physical_hosts']==1 and not scope['real_hardware']
            assert scope['startup_configuration_recorded'] and scope['synthetic_reviewed_publication']
        for p in base.rglob('*'):
            if p.is_file():
                if 'credential' in p.name.lower():secrets.append(p.read_bytes().strip())
                else:add(p,'network/'+label+'/'+str(p.relative_to(base)))
    resultNames=['results-configuration-view-protocol-current.txt','results-configuration-view-protocol-sanitized-current.txt',
                 'results-configuration-view-network-current.txt','results-configuration-view-network-sanitized-final.txt',
                 'results-configuration-view-network-check-final.txt','results-configuration-view-network-sanitized-check-final.txt',
                 'results-configuration-view-native-build.txt','results-configuration-view-native-sanitized-build.txt']
    for name in resultNames:add(root/'build-ice-compat'/name,'results/'+name)
    for name in ['results-configuration-view-network-check-final.txt','results-configuration-view-network-sanitized-check-final.txt']:
        assert 'PASS independent configuration-view network validation' in (root/'build-ice-compat'/name).read_text()
    extras=['GetDAQ.pro','common/control/CMakeLists.txt','common/experiment/CMakeLists.txt',
            'tests/ecc_protocol_test.cpp','tests/test_native_gui.cpp','tests/native_gui_probe.cpp','tests/check_configuration_view_network.py',
            'tests/run_ecc_network_service_tests.py','tests/archive_configuration_view_validation.py',
            'config/envoy.native.example.yml','README.md','docs/ECC_CONFIGURATION_VIEW.md','docs/ECC_GUI_CLIENT.md',
            'docs/ECC_CONFIGURATION_SELECTION.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    for name in extras:inputs[name]=sha(root/name)
    for name in sorted(inputs):add(root/name,'verified-sources/'+name)
    for name,p in files.items():
        data=p.read_bytes();assert all(secret not in data for secret in secrets if secret),'Credential value present'
        output=target/name;output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,output)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
