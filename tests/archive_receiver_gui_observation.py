#!/usr/bin/env python3
"""Freeze final ECC/Qt process-observation evidence; exclude credentials and raw payload."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda: file.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def verify(target, current=None):
    manifest = json.loads((target/'files-sha256.json').read_text())
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()} == set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        assert sha(target/name) == digest, name
        assert not name.endswith('.graw')
        assert name.startswith('verified-sources/') or 'credential' not in name.lower()
    inputs = json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name) == digest
        if current:
            assert sha(current/name) == digest, name
    for label in ('ordinary','sanitized'):
        folder = target/label
        for kind,count in [('native-control',16),('client',20)]:
            receipt = json.loads((folder/kind/'build-receipt.json').read_text())
            assert receipt['sanitized'] == (label == 'sanitized') and not receipt['system_qt_instrumented']
            assert sha(folder/kind/'program') == receipt['program_sha256']
            log = (folder/kind/'results-current.txt').read_text()
            assert f'Totals: {count} passed, 0 failed' in log and 'FAIL!' not in log
        evidence = folder/'network'
        result = json.loads((evidence/'result.json').read_text())
        assert result['passed'] and result['generated_raw_deleted'] and not result['real_hardware'] and not result['qt_gui_executed']
        assert result['maximum_wire_bytes'] == 3145728 < result['aggregate_limit_bytes'] == 20*1024**3
        assert len(result['results']) == 3 and all(row['exit_code'] == 0 for row in result['results'])
        assert result['runner_sha256'] == sha(target/'verified-sources/tests/run_receiver_gui_observation_tests.py')
        for name,digest in result['program_sha256'].items():
            assert sha(folder/'programs'/name) == digest
        root = next(evidence.glob('ecc-network-system-*'))
        for scenario in ('normal-0','normal-1','truncated'):
            fresh = json.loads((root/scenario/'fresh-receiver-observation.json').read_text())
            ready = json.loads((root/scenario/'ready-receiver-observation.json').read_text())
            assert len(fresh['nodes']) == len(ready['nodes']) == 2
            assert all(node['available'] and not node['observation']['receivers'] for node in fresh['nodes'])
            assert all(node['available'] and not node['observation']['busy'] and len(node['observation']['receivers']) == 1 and node['observation']['receivers'][0]['state'] == 'running' for node in ready['nodes'])
        partial = json.loads((root/'normal-0/partial-receiver-observation.json').read_text())
        timed = json.loads((root/'normal-0/timed-receiver-observation.json').read_text())
        assert [node['available'] for node in partial['nodes']] == [False,True]
        assert all(not node['available'] and node['observation'] is None for node in timed['nodes'])
        cleanup = json.loads((evidence/'payload-cleanup.json').read_text())
        assert cleanup['raw_files_remaining'] == 0 and len(cleanup['deleted']) == 10 and sum(row['bytes'] for row in cleanup['deleted']) == 180224
    for kind in ('full-ui','console'):
        receipt = json.loads((target/'ordinary'/kind/'build-receipt.json').read_text())
        assert not receipt['sanitized'] and sha(target/'ordinary'/kind/'program') == receipt['program_sha256']
    assert 'Totals: 67 passed, 0 failed' in (target/'ordinary/full-ui/results-current.txt').read_text()
    integrated=json.loads((target/'ordinary/integrated-normal-receipt.json').read_text())
    for name,program in integrated['programs'].items():
        archived={'getdaq-ecc-server':'ecc_service','getdaq-supervisor':'data_service','ecc-network-service-tests':'fixture','ecc-protocol-tests':'protocol'}[name]
        assert sha(target/'ordinary/programs'/archived)==program['sha256']
    commands = json.loads((target/'sanitized/native-build/commands.json').read_text())
    compiled = [x if isinstance(x,list) else x['command'] for x in commands]
    compiled = [x for x in compiled if '-c' in x]
    assert len(compiled) == 75 and all('-std=c++11' in x and '-pedantic-errors' in x and '-fsanitize=address,undefined' in x for x in compiled)
    for label in ('ordinary','sanitized'):
        assert (target/label/'results-protocol-current.txt').read_text().startswith('PASS portable C++11 ECC codec:')
    print('PASS receiver GUI observation archive:',len(manifest),'files;',len(inputs),'inputs; final ordinary/sanitized evidence; no raw/credentials; historical only',current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true')
    args = parser.parse_args(); target = root/'build-receiver-gui/validation-gui-observation'
    if not args.create:
        verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files = {}; inputs = {}
    def add(path,name):
        assert path.is_file() and not path.is_symlink() and path.suffix != '.graw' and name not in files
        assert '..' not in Path(name).parts and 'credential' not in name.lower()
        files[name] = path
    def merge_receipt(build):
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name) == digest and (name not in inputs or inputs[name] == digest),name
            inputs[name] = digest
    qt_builds = [('ordinary','native-control',root/'build-receiver-gui/native-control'),('ordinary','client',root/'build-receiver-gui/client'),
                 ('ordinary','full-ui',root/'build-receiver-gui/full-ui'),('ordinary','console',root/'build-receiver-gui/console'),
                 ('sanitized','native-control',Path('/tmp/getdaq-receiver-gui-native-asan')),('sanitized','client',Path('/tmp/getdaq-receiver-gui-client-asan'))]
    for label,kind,build in qt_builds:
        merge_receipt(build); prefix=label+'/'+kind
        receipt = json.loads((build/'build-receipt.json').read_text()); program = (build/receipt['target']).resolve()
        assert sha(program) == receipt['program_sha256']; add(program,prefix+'/program')
        for name in ('build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt'):
            add(build/name,prefix+'/'+name)
        if kind != 'console':add(build/'results-current.txt',prefix+'/results-current.txt')
        # qmake测试的*.moc也必须冻结，不能只复制moc_*.cpp。
        for pattern in ('moc_*.cpp','ui_*.h','*.moc'):
            for path in build.glob(pattern):add(path,prefix+'/generated/'+path.name)
    native = Path('/tmp/getdaq-ecc-receiver-gui-asan');merge_receipt(native)
    for name in ('commands.json','source-sha256.json','generated-sha256.json','dependency-sha256.json','ecc-protocol-tests.receipt.json','ecc-protocol-tests.build.txt'):
        add(native/name,'sanitized/native-build/'+name)
    for path,digest in json.loads((native/'generated-sha256.json').read_text()).items():
        assert sha(Path(path)) == digest;add(Path(path),'sanitized/native-build/generated/'+Path(path).name)
    for path in native.glob('*.compile.txt'):
        if 'credential' not in path.name.lower():add(path,'sanitized/native-build/logs/'+path.name)
    for path in native.glob('*.link.txt'):add(path,'sanitized/native-build/logs/'+path.name)
    add(native/'ecc-protocol-tests','sanitized/programs/protocol')
    add(native/'results-protocol-current.txt','sanitized/results-protocol-current.txt')
    add(root/'build-ice-compat/control-protocol/ecc-protocol-tests','ordinary/programs/protocol')
    add(root/'build-ice-compat/results-receiver-gui-protocol-current.txt','ordinary/results-protocol-current.txt')
    for label,build,receiver_build,data_build in [('ordinary',root/'build-ice-compat',root/'build-receiver',root/'build-receiver-observation'),
                                               ('sanitized',native,Path('/tmp/getdaq-receiver-io-asan'),Path('/tmp/getdaq-receiver-observation-asan'))]:
        merge_receipt(receiver_build);merge_receipt(data_build)
        roots = list((root/'build-receiver-gui'/(label+'-final-evidence')).glob('receiver-gui-service-*'))
        assert len(roots) == 1 and not list(roots[0].rglob('*.graw'))
        for path in roots[0].rglob('*'):
            relative = str(path.relative_to(roots[0]))
            if path.is_file() and not path.is_symlink() and 'credential' not in relative.lower():add(path,label+'/network/'+relative)
        data_program=build/'getdaq-supervisor' if label=='ordinary' else data_build/'getdaq-supervisor'
        for name,path in [('fixture',build/'ecc-network-service-tests'),('ecc_service',build/'getdaq-ecc-server'),
                          ('receiver',receiver_build/'getdaq-receiver'),('data_service',data_program)]:
            add(path,label+'/programs/'+name)
        for prefix,source_build in [('receiver',receiver_build),('data',data_build)]:
            for name in ('build-receipt.json','source-sha256.json','verified-compile-commands.json'):
                add(source_build/name,label+'/'+prefix+'-build/'+name)
    integrated = root/'build-receiver-gui/integrated-normal-receipt.json'
    add(integrated,'ordinary/integrated-normal-receipt.json')
    add(root/'build-ice-compat/results-receiver-gui-final-build.txt','ordinary/results-integrated-build.txt')
    for name in ['tests/archive_receiver_gui_observation.py','tests/run_receiver_gui_observation_tests.py','tests/run_ecc_network_service_tests.py','tests/run_receiver_observation_tests.py',
                 'tests/build_native_ecc_sanitizers.py','tests/build_qt_control_validation.py','tests/ecc_protocol_test.cpp','config/envoy.native.example.yml',
                 'docs/RECEIVER_GUI_OBSERVATION.md','docs/RECEIVER_PROCESS_OBSERVATION.md','docs/REMOTE_RECEIVER_CONTROL.md','docs/ECC_GUI_CLIENT.md','docs/ECC_NETWORK_SERVICE.md',
                 'docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md']:
        inputs[name] = sha(root/name)
    for name in inputs:files['verified-sources/'+name] = root/name
    target.mkdir()
    for name,path in files.items():
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest = {str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    verify(target,root)


if __name__ == '__main__':main()
