#!/usr/bin/env python3
"""Preserve file-limit regression evidence and programs, excluding raw test data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target, current=None):
    manifest = json.loads((target / 'files-sha256.json').read_text())
    for name, digest in manifest.items(): assert sha(target / name) == digest, name
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()} == set(manifest) | {'files-sha256.json'}
    assert not any(p.suffix == '.graw' for p in target.rglob('*'))
    inputs = json.loads((target / 'input-sha256.json').read_text())
    if current:
        for name, digest in inputs.items(): assert sha(current / name) == digest, name
    for label in ('before', 'ordinary', 'sanitized'):
        folder = target / label
        result = json.loads((folder / 'evidence/result.json').read_text())
        assert sha(folder / 'getdaq-receiver') == result['receiver_sha256']
        assert result['runner_sha256'] == sha(target / 'verified-sources/tests/receiver_file_limit_test.py')
        assert result['generated_raw_deleted'] and not result['real_hardware']
        assert result['legacy_demonstration'] == (label == 'before')
        assert len(result['cases']) == (1 if label == 'before' else 4)
        for case in result['cases']:
            case_dir = folder / 'evidence' / case['case']
            cleanup = json.loads((case_dir / 'payload-cleanup.json').read_text())
            assert cleanup['deleted_bytes'] == case['partial_file_bytes'] <= 4096
            if label == 'before':
                assert case['legacy_signal_abort_confirmed'] and case['return_code'] == -25
                continue
            final = case['final_status']
            success = case['expected_success']
            assert case['return_code'] == (0 if success else 1)
            assert final['stream_sealed'] == success
            assert (not final['error']) if success else 'File too large' in final['error']
            assert (case_dir / '.getdaq-receiver-seal.json').exists() == success
        if label != 'before':
            receipt = json.loads((folder / 'build-receipt.json').read_text())
            assert receipt['program_sha256']['getdaq-receiver'] == result['receiver_sha256']
            assert receipt['sanitized'] == (label == 'sanitized')
            assert receipt['compile_count'] == 8 and not receipt['raw_path_uses_qt']
    print('PASS receiver file-limit archive:', len(manifest), 'files;', len(inputs),
          'inputs; no raw payload; historical only', current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    target = root / 'build-receiver/validation-file-limit'
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

    for label in ('before', 'ordinary', 'sanitized'):
        matches = list((root / ('build-receiver/file-limit-' + label)).glob('receiver-file-limit-*'))
        assert len(matches) == 1
        for path in matches[0].rglob('*'):
            if path.is_file(): add(path, label + '/evidence/' + str(path.relative_to(matches[0])))
        if label == 'before':
            add(root / 'build-receiver/validation-performance/ordinary/getdaq-receiver', label + '/getdaq-receiver')
            continue
        build = root / 'build-receiver' if label == 'ordinary' else Path('/tmp/getdaq-receiver-performance-asan')
        add(build / 'getdaq-receiver', label + '/getdaq-receiver')
        for name in ['build-receipt.json', 'source-sha256.json', 'verified-compile-commands.json',
                     'results-configure-verified.txt', 'results-build-verified.txt', 'results-file-limit-current.txt']:
            add(build / name, label + '/' + name)
        for name, digest in json.loads((build / 'source-sha256.json').read_text()).items():
            assert sha(root / name) == digest
            assert name not in inputs or inputs[name] == digest
            inputs[name] = digest
    for name in ['tests/receiver_file_limit_test.py', 'tests/archive_receiver_file_limit_validation.py',
                 'docs/RECEIVER_WRITE_FAILURES.md', 'docs/DATA_SERVICE.md', 'docs/VALIDATION.md', 'docs/REFACTOR_STATUS.md']:
        inputs[name] = sha(root / name)
    for name in inputs: add(root / name, 'verified-sources/' + name)
    target.mkdir(parents=True)
    for name, source in files.items():
        output = target / name
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, output)
    (target / 'input-sha256.json').write_text(json.dumps(inputs, indent=2) + '\n')
    manifest = {str(p.relative_to(target)): sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target / 'files-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    verify(target, root)


if __name__ == '__main__': main()
