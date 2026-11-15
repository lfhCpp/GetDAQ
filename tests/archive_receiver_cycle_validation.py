#!/usr/bin/env python3
"""Freeze FDT receiver cycles with actual programs, source inputs and cleaned evidence."""
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


def read(path):
    return json.loads(path.read_text())


def one(parent, pattern):
    matches = list(parent.glob(pattern))
    assert len(matches) == 1, (str(parent), pattern, len(matches))
    return matches[0]


def verify(target, current=None):
    manifest = read(target/'files-sha256.json')
    actual = {str(path.relative_to(target)) for path in target.rglob('*') if path.is_file()}
    assert actual == set(manifest) | {'files-sha256.json'}
    for name, digest in manifest.items():
        path = target/name
        assert not path.is_symlink() and path.suffix != '.graw' and sha(path) == digest, name
        assert name.startswith('verified-sources/') or 'credential' not in name.lower(), name
    inputs = read(target/'input-sha256.json')
    for name, digest in inputs.items():
        assert sha(target/'verified-sources'/name) == digest, name
        if current:
            assert sha(current/name) == digest, name
    sources = target/'verified-sources'
    for label in ('ordinary', 'sanitized'):
        folder = target/label
        for kind, count in [('receiver', 8), ('supervisor', 49)]:
            receipt = read(folder/(kind+'-build')/'build-receipt.json')
            assert receipt['sanitized'] == (label == 'sanitized') and receipt['compile_count'] == count
            for name, digest in receipt['program_sha256'].items():
                assert sha(folder/kind/name) == digest, name
            if kind == 'supervisor':
                for name, digest in read(folder/(kind+'-build')/'generated-sha256.json').items():
                    assert sha(folder/(kind+'-build')/'generated'/name) == digest
        receiver_sha = sha(folder/'receiver/getdaq-receiver')
        control = read(folder/'private-control/result.json')
        assert control['receiver_sha256'] == receiver_sha
        assert control['runner_sha256'] == sha(sources/'tests/receiver_cycle_control_test.py')
        assert control['generated_raw_deleted'] and control['wire_bytes'] == 16416
        cases = {row['case']: row for row in control['cases']}
        assert set(cases) == {'finish-active', 'controller-loss', 'abort-refused', 'abort-empty', 'finish-empty', 'invalid-request'}
        assert cases['finish-active']['success_seal'] and cases['abort-refused']['success_seal']
        assert not cases['controller-loss']['success_seal'] and cases['controller-loss']['final_status']['connections'] == 2
        assert not cases['controller-loss']['final_status']['cycle_control_connected']
        assert cases['abort-empty']['final_status']['cycle_startup_aborted']
        assert all(row['return_code'] == (0 if row['success_seal'] else 1) for row in cases.values())
        assert read(folder/'private-control/payload-cleanup.json')['remaining_raw'] == 0
        evidence = folder/'supervisor-evidence'
        result = read(evidence/'result.json')
        assert result['raw_files_remaining'] == 0 and result['maximum_wire_bytes'] == 10637568
        assert result['aggregate_limit_bytes'] == 20*1024**3 and not result['real_hardware']
        assert all(row['exit_code'] == 0 for row in result['runs'])
        assert result['receiver_sha256'] == receiver_sha
        assert result['runner_sha256'] == sha(sources/'tests/run_receiver_observation_tests.py')
        for name, digest in result['program_sha256'].items():
            assert sha(folder/'supervisor'/name) == digest
        native = one(evidence/'native', 'managed-file-naming-*')
        native_result = read(native/'result.json')
        assert len(native_result['cases']) == 27 and all(row['exit_code'] == 0 for row in native_result['cases'])
        assert native_result['generated_raw_deleted'] and native_result['receiver_sha256'] == receiver_sha
        assert native_result['runner_sha256'] == sha(sources/'tests/run_file_naming_supervisor_tests.py')
        assert all(row['input_protocol'] == 'FDT' for row in read(native/'fdt-cycle/deployment/plan.json')['sources'])
        http_cases = read(one(evidence/'http', 'remote-receiver-*')/'results.json')
        assert len(http_cases) == 22 and all(row['exit_code'] == 0 for row in http_cases)
        http = read(folder/'cycle-http/result.json')
        assert {row['case'] for row in http['cases']} == {'segments', 'active-timeout', 'empty-cycle'}
        assert all(row['passed'] for row in http['cases']) and http['generated_raw_deleted']
        assert http['wire_bytes'] == 32768 <= http['maximum_wire_bytes'] == 1024**2
        assert http['receiver_sha256'] == receiver_sha
        assert http['runner_sha256'] == sha(sources/'tests/receiver_cycle_http_test.py')
        for name, digest in http['program_sha256'].items():
            assert sha(folder/'supervisor'/name) == digest
        assert read(folder/'cycle-http/payload-cleanup.json')['remaining_raw'] == 0
        transports = read(folder/'transports/result.json')
        assert len(transports['cases']) == 12 and transports['generated_raw_deleted']
        assert sum(row['wire_bytes'] for row in transports['cases']) == 49739
        assert transports['receiver_sha256'] == receiver_sha
        assert (folder/'results-core.txt').read_text().startswith('PASS:')
    print('PASS FDT cycle archive:', len(manifest), 'files;', len(inputs),
          'inputs; actual program SHA; no raw/credentials; historical only', current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create', action='store_true')
    parser.add_argument('--historical-only', action='store_true')
    args = parser.parse_args()
    target = root/'build-fdt-cycle/validation-fdt-cycles'
    if not args.create:
        verify(target, None if args.historical_only else root)
        return
    assert not target.exists() and not args.historical_only
    files, inputs = {}, {}

    def add(path, name):
        assert path.is_file() and not path.is_symlink() and path.suffix != '.graw'
        assert name not in files and '..' not in Path(name).parts
        assert name.startswith('verified-sources/') or 'credential' not in name.lower()
        files[name] = path

    def merge(build):
        for name, digest in read(build/'source-sha256.json').items():
            assert sha(root/name) == digest and (name not in inputs or inputs[name] == digest), name
            inputs[name] = digest

    def tree(parent, prefix, pattern):
        folder = one(parent, pattern)
        assert not list(folder.rglob('*.graw')), str(folder)
        for path in folder.rglob('*'):
            relative = str(path.relative_to(folder))
            if path.is_file() and not path.is_symlink() and 'credential' not in relative.lower():
                add(path, prefix+'/'+relative)

    base = root/'build-fdt-cycle'
    for label, receiver, supervisor, native_parent, http_parent in [
        ('ordinary', root/'build-receiver', base, 'ordinary-supervisor-final-evidence', 'ordinary-http-final'),
        ('sanitized', Path('/tmp/getdaq-fdt-cycle-receiver-asan'), Path('/tmp/getdaq-fdt-cycle-supervisor-asan'),
         'sanitized-supervisor-evidence', 'sanitized-http-final')]:
        for kind, build in [('receiver', receiver), ('supervisor', supervisor)]:
            merge(build)
            for name, digest in read(build/'build-receipt.json')['program_sha256'].items():
                assert sha(build/name) == digest
                add(build/name, label+'/'+kind+'/'+name)
            for name in ['build-receipt.json', 'source-sha256.json', 'verified-compile-commands.json',
                         'results-configure-verified.txt', 'results-build-verified.txt']:
                add(build/name, label+'/'+kind+'-build/'+name)
            if kind == 'supervisor':
                add(build/'generated-sha256.json', label+'/'+kind+'-build/generated-sha256.json')
                for name, digest in read(build/'generated-sha256.json').items():
                    assert sha(build/name) == digest
                    add(build/name, label+'/'+kind+'-build/generated/'+name)
        for name, path in [
            ('results-core.txt', receiver/'results-fdt-cycle-core.txt'),
            ('results-transports.txt', receiver/'results-fdt-cycle-transports.txt'),
            ('results-control.txt', receiver/'results-fdt-cycle-control.txt'),
            ('results-http.txt', supervisor/'results-http-cycle-final.txt'),
            ('results-supervisor.txt', supervisor/('results-supervisor-final.txt' if label=='ordinary' else 'results-supervisor-current.txt'))]:
            add(path, label+'/'+name)
        tree(base/(label+'-control'), label+'/private-control', 'receiver-cycle-*')
        tree(base/native_parent, label+'/supervisor-evidence', 'receiver-observation-*')
        tree(base/http_parent, label+'/cycle-http', 'receiver-cycle-http-*')
        tree(base/(label+'-transports'), label+'/transports', 'receiver-transports-*')
    for name in [
        'tests/archive_receiver_cycle_validation.py', 'tests/receiver_cycle_control_test.py',
        'tests/receiver_cycle_http_test.py', 'tests/run_receiver_observation_tests.py',
        'tests/run_file_naming_supervisor_tests.py', 'tests/run_supervisor_control_tests.py',
        'tests/receiver_transport_integration.py', 'docs/FDT_RECEIVER_CYCLES.md',
        'docs/DATA_SERVICE.md', 'docs/REMOTE_RECEIVER_CONTROL.md', 'docs/RECEIVER_TRANSPORTS.md',
        'docs/REFACTOR_STATUS.md', 'docs/VALIDATION.md', 'README.md']:
        inputs[name] = sha(root/name)
    for name in inputs:
        add(root/name, 'verified-sources/'+name)
    target.mkdir()
    for name, path in files.items():
        destination = target/name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    (target/'input-sha256.json').write_text(json.dumps(inputs, indent=2)+'\n')
    manifest = {str(path.relative_to(target)): sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest, indent=2)+'\n')
    verify(target, root)


if __name__ == '__main__':
    main()
