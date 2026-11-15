#!/usr/bin/env python3
"""Archive native receiver validation without generated raw payload or credentials."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target, current=None):
    manifest = json.loads((target / 'files-sha256.json').read_text())
    actual = {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
    assert actual == set(manifest) | {'files-sha256.json'}
    for name, digest in manifest.items(): assert sha(target / name) == digest, name
    assert not any(p.suffix == '.graw' for p in target.rglob('*'))
    inputs = json.loads((target / 'input-sha256.json').read_text())
    if current:
        for name, digest in inputs.items(): assert sha(current / name) == digest, name
    for label in ('ordinary', 'sanitized'):
        build = target / label
        receipt = json.loads((build / 'build-receipt.json').read_text())
        assert receipt['sanitized'] == (label == 'sanitized')
        assert receipt['compile_count'] == 8 and not receipt['raw_path_uses_qt']
        for name, digest in receipt['program_sha256'].items(): assert sha(build / name) == digest
        compiles = json.loads((build / 'verified-compile-commands.json').read_text())
        assert len(compiles) == 8
        for entry in compiles:
            assert '-std=c++11' in entry['command'] and '-pedantic-errors' in entry['command']
            if label == 'sanitized': assert '-fsanitize=address,undefined' in entry['command']
        assert (build / 'results-integration-performance-current.txt').read_text().count('PASS:') == 5
        core = (build / 'results-core-performance-current.txt').read_text()
        assert ('100% tests passed, 0 tests failed out of 1' in core) if label == 'ordinary' else core.startswith('PASS:')
    comparison = target / 'framer'
    record = json.loads((comparison / 'result.json').read_text())
    assert record['disk_payload_bytes'] == 0 and len(record['cases']) == 4
    assert sha(comparison / 'baseline-benchmark') == record['program_sha256']['baseline']
    assert sha(target / 'ordinary/mfm-stream-benchmark') == record['program_sha256']['current']
    for name, digest in record['baseline_source_sha256'].items(): assert sha(comparison / name) == digest
    for case in record['cases']:
        for label in ('baseline', 'current'):
            assert len(case['trials'][label]) == 3
            for trial in case['trials'][label]:
                assert trial['frames'] == trial['total_bytes'] // case['frame_bytes']
                assert trial['borrowed_frame_bytes'] + trial['buffered_frame_bytes'] == trial['total_bytes']
    receiver = json.loads((target / 'ordinary/build-receipt.json').read_text())['program_sha256']['getdaq-receiver']
    for label, total, counts in [('parallel', 1024**3, [8, 8]), ('rotation', 12 * 1024**2, [12])]:
        folder = target / 'load' / label
        result = json.loads((folder / 'result.json').read_text())
        cleanup = json.loads((folder / 'payload-cleanup.json').read_text())
        assert result['receiver_sha256'] == receiver
        assert result['test_script_sha256'] == sha(target / 'verified-sources/tests/receiver_sustained_load.py')
        assert result['maximum_total_test_payload_bytes'] == 20 * 1024**3
        assert total == result['total_bytes'] == cleanup['deleted_bytes'] <= 20 * 1024**3
        assert cleanup['remaining_generated_payload_files'] == 0
        assert not result['real_hardware'] and result['physical_hosts'] == 1 and not result['qt_raw_path']
        assert [len(stream['files']) for stream in result['streams']] == counts
        for stream in result['streams']:
            final = stream['final_status']
            assert final['received_bytes'] == final['accepted_file_bytes'] == final['sealed_bytes'] == stream['bytes']
            assert final['frames'] == stream['frames'] and final['stream_sealed'] and final['sender_eof_confirmed']
            seal = json.loads((folder / ('stream-' + str(stream['stream'])) / '.getdaq-receiver-seal.json').read_text())
            assert seal['complete'] and seal['durable'] and int(seal['received_bytes']) == stream['bytes']
            assert {entry['name']: int(entry['bytes']) for entry in seal['files']} == {entry['name']: entry['bytes'] for entry in stream['files']}
    print('PASS receiver performance archive:', len(manifest), 'files;', len(inputs),
          'inputs; no raw payload; historical only', current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    target = root / 'build-receiver/validation-performance'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create', action='store_true')
    parser.add_argument('--historical-only', action='store_true')
    args = parser.parse_args()
    if not args.create:
        verify(target, None if args.historical_only else root)
        return
    assert not target.exists() and not args.historical_only
    files, inputs = {}, {}

    def add(path, name):
        assert path.is_file() and name not in files and '..' not in Path(name).parts
        assert path.suffix != '.graw' and 'credential' not in path.name.lower()
        files[name] = path

    for label, build in [('ordinary', root / 'build-receiver'), ('sanitized', Path('/tmp/getdaq-receiver-performance-asan'))]:
        for name, digest in json.loads((build / 'source-sha256.json').read_text()).items():
            assert sha(root / name) == digest
            assert name not in inputs or inputs[name] == digest
            inputs[name] = digest
        receipt = json.loads((build / 'build-receipt.json').read_text())
        for name, digest in receipt['program_sha256'].items():
            assert sha(build / name) == digest
            add(build / name, label + '/' + name)
        for name in ['build-receipt.json', 'source-sha256.json', 'verified-compile-commands.json',
                     'results-configure-verified.txt', 'results-build-verified.txt',
                     'results-core-performance-current.txt', 'results-integration-performance-current.txt']:
            add(build / name, label + '/' + name)
    comparison = root / 'build-receiver/framer-comparison-current'
    for path in comparison.iterdir():
        if path.is_file(): add(path, 'framer/' + path.name)
    add(root / 'build-receiver/results-performance-safety-current.txt', 'results/safety-current.txt')
    assert 'Ran 3 tests' in (root / 'build-receiver/results-performance-safety-current.txt').read_text()
    for label in ('parallel', 'rotation'):
        matches = list((root / ('build-receiver/performance-' + label)).glob('receiver-load-*'))
        assert len(matches) == 1, 'Use one unambiguous actual load invocation per evidence group'
        for path in matches[0].rglob('*'):
            if path.is_file(): add(path, 'load/' + label + '/' + str(path.relative_to(matches[0])))
        add(root / ('build-receiver/results-load-performance-' + label + '.txt'), 'results/load-' + label + '.txt')
    for name in ['tests/receiver_sustained_load.py', 'tests/receiver_load_safety_test.py',
                 'tests/receiver_integration.py', 'tests/receiver_framer_comparison.py',
                 'tests/archive_receiver_performance_validation.py', 'README.md', 'docs/DATA_SERVICE.md',
                 'docs/RECEIVER_PERFORMANCE.md', 'docs/ARCHITECTURE.md', 'docs/REFACTOR_STATUS.md', 'docs/VALIDATION.md']:
        inputs[name] = sha(root / name)
    for name in inputs: add(root / name, 'verified-sources/' + name)
    target.mkdir(parents=True)
    for name, source in files.items():
        output = target / name
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, output)
    (target / 'input-sha256.json').write_text(json.dumps(inputs, indent=2) + '\n')
    manifest = {str(path.relative_to(target)): sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target / 'files-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    verify(target, root)


if __name__ == '__main__':
    main()
