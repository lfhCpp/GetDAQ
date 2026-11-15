#!/usr/bin/env python3
"""Freeze verified process-observation builds and small evidence without credentials/raw."""
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
    for name, digest in manifest.items():
        assert sha(target/name) == digest, name
        assert not name.endswith('.graw')
        assert name.startswith('verified-sources/') or 'credential' not in name.lower()
    inputs = json.loads((target/'input-sha256.json').read_text())
    for name, digest in inputs.items():
        assert sha(target/'verified-sources'/name) == digest
        if current:
            assert sha(current/name) == digest, name
    for label in ('ordinary','sanitized'):
        folder = target/label
        receipt = json.loads((folder/'build-receipt.json').read_text())
        assert receipt['sanitized'] == (label == 'sanitized') and receipt['compile_count'] == 49
        assert not receipt['raw_data_path_uses_qt'] and not receipt['system_qt_instrumented']
        for name, digest in receipt['program_sha256'].items():
            assert sha(folder/name) == digest
        evidence = folder/'evidence'; result = json.loads((evidence/'result.json').read_text())
        assert result['raw_files_remaining'] == 0 and not result['real_hardware'] and not result['throughput_benchmark']
        assert result['maximum_wire_bytes'] == 8257536 < result['aggregate_limit_bytes'] == 20*1024**3
        assert result['receiver_sha256'] == sha(folder/'getdaq-receiver')
        assert result['runner_sha256'] == sha(target/'verified-sources/tests/run_receiver_observation_tests.py')
        for name, digest in result['program_sha256'].items():
            assert sha(folder/name) == digest
        native = list((evidence/'native').glob('managed-file-naming-*')); assert len(native) == 1
        native_result = json.loads((native[0]/'result.json').read_text())
        assert len(native_result['cases']) == 21 and native_result['generated_raw_deleted']
        assert all(case['exit_code'] == 0 for case in native_result['cases'])
        for name in ('observe-clean','observe-error','observe-signal','observe-lost'):
            observed = json.loads((native[0]/name/'observation.json').read_text())
            assert not observed['busy'] and len(observed['receivers']) == 2
            if name == 'observe-clean':
                assert observed['lifecycle']['state'] == 'stopped'
                assert all(row['state'] == 'reaped' and row['exit_code'] == 0 for row in observed['receivers'])
            elif name == 'observe-lost':
                assert observed['receivers'][0]['state'] == 'unavailable'
            else:
                assert observed['lifecycle']['state'] == 'failed' and observed['receivers'][0]['state'] == 'reaped'
                assert observed['receivers'][0]['exit_code'] == (1 if name == 'observe-error' else 137)
        http = list((evidence/'http').glob('remote-receiver-*')); assert len(http) == 1
        http_results = json.loads((http[0]/'results.json').read_text())
        assert len(http_results) == 22 and all(row['exit_code'] == 0 for row in http_results)
        transport = json.loads((http[0]/'native-transport/results.json').read_text())
        assert len(transport) == 8 and all((row['exit_code'] == 0) == row['expected_success'] for row in transport)
        for name in ('observe-clean','observe-error','observe-signal','observe-restart'):
            observed = json.loads((http[0]/name/'actual-observation.json').read_text())
            assert not observed['busy'] and observed['lifecycle']['state'] == 'ready'
            assert all(row['state'] == 'exited' for row in observed['receivers'])
        assert json.loads((evidence/'payload-cleanup.json').read_text())['raw_files_remaining'] == 0
    integrated = json.loads((target/'integrated-normal/receipt.json').read_text())
    assert not integrated['sanitized'] and not integrated['real_hardware'] and not integrated['runtime_tested_in_this_build']
    for name, program in integrated['programs'].items():
        assert sha(target/'integrated-normal'/name) == program['sha256']
    assert sha(target/'integrated-normal/results-build.txt') == integrated['log_sha256']
    print('PASS receiver observation archive:', len(manifest),'files;',len(inputs),'inputs; no credentials/raw; historical only',current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create', action='store_true'); parser.add_argument('--historical-only', action='store_true')
    args = parser.parse_args(); target = root/'build-receiver-observation/validation-observation'
    if not args.create:
        verify(target, None if args.historical_only else root); return
    assert not target.exists() and not args.historical_only
    files = {}; inputs = {}
    def add(path, name):
        assert path.is_file() and not path.is_symlink() and path.suffix != '.graw' and name not in files
        assert 'credential' not in path.name.lower() and '..' not in Path(name).parts
        files[name] = path
    for label, build, receiver_build in [('ordinary',root/'build-receiver-observation',root/'build-receiver'),
                                        ('sanitized',Path('/tmp/getdaq-receiver-observation-asan'),Path('/tmp/getdaq-receiver-io-asan'))]:
        for source_build, prefix in [(build,label),(receiver_build,label+'/receiver-build')]:
            tracked = json.loads((source_build/'source-sha256.json').read_text())
            for name, digest in tracked.items():
                assert sha(root/name) == digest and (name not in inputs or inputs[name] == digest)
                inputs[name] = digest
            for name in ('build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt'):
                add(source_build/name,prefix+'/'+name)
        receipt = json.loads((build/'build-receipt.json').read_text())
        for name, digest in receipt['program_sha256'].items():
            assert sha(build/name) == digest; add(build/name,label+'/'+name)
        add(receiver_build/'getdaq-receiver',label+'/getdaq-receiver')
        add(build/'results-observation-current.txt',label+'/results-observation-current.txt')
        evidence = list((root/'build-receiver-observation'/(label+'-evidence')).glob('receiver-observation-*'))
        assert len(evidence) == 1 and not list(evidence[0].rglob('*.graw'))
        for path in evidence[0].rglob('*'):
            relative = str(path.relative_to(evidence[0]))
            if path.is_file() and not path.is_symlink() and 'credential' not in relative.lower():
                add(path,label+'/evidence/'+relative)
        add(build/'generated-sha256.json',label+'/generated-sha256.json')
        for name, digest in json.loads((build/'generated-sha256.json').read_text()).items():
            assert sha(build/name) == digest; add(build/name,label+'/generated/'+name)
            # mocs_compilation.cpp包含其他moc源码；一并冻结实际包含文件。
            for path in (build/name).parent.rglob('*'):
                relative = label+'/generated/'+str(path.relative_to(build))
                if path.is_file() and path.suffix in ('.h','.cpp') and relative not in files:
                    add(path,relative)
    integrated_path = root/'build-receiver-observation/integrated-normal-receipt.json'
    integrated = json.loads(integrated_path.read_text()); add(integrated_path,'integrated-normal/receipt.json')
    add(root/'build-ice-compat/results-receiver-observation-current-build.txt','integrated-normal/results-build.txt')
    for name, program in integrated['programs'].items():
        assert sha(root/program['path']) == program['sha256']; add(root/program['path'],'integrated-normal/'+name)
    for name in ['tests/archive_receiver_observation_validation.py','tests/run_receiver_observation_tests.py','tests/run_file_naming_supervisor_tests.py','tests/run_supervisor_control_tests.py',
                 'docs/RECEIVER_PROCESS_OBSERVATION.md','docs/REMOTE_RECEIVER_CONTROL.md','docs/DATA_SERVICE.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md']:
        inputs[name] = sha(root/name)
    for name in inputs:
        # Header paths named credential are public source, not private credential files.
        files['verified-sources/'+name] = root/name
    target.mkdir()
    for name, path in files.items():
        dest = target/name; dest.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(path, dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest = {str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    verify(target,root)


if __name__ == '__main__':
    main()
