#!/usr/bin/env python3
"""Check frozen configuration transaction evidence; does not start services or edit data."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, default=ROOT / 'build-config-transactions/checkpoint.json')
    parser.add_argument('--historical-artifacts-only', action='store_true',
                        help='Source changes are expected in later work; verify only frozen artifacts')
    args = parser.parse_args()
    checkpoint = json.loads(args.checkpoint.read_text())
    assert checkpoint['schema'] == 'getdaq.config-source-transactions:1'
    assert checkpoint['phase'] == '70.67' and checkpoint['goal_complete'] is False
    for key in ['real_hardware_accepted', 'ubuntu24_accepted', 'macos_accepted',
                'independent_windows_accepted', 'sustained_throughput_accepted',
                'uncooperative_external_atomicity_provided', 'file_path_migration_implemented']:
        assert checkpoint[key] is False, key
    assert checkpoint['raw_remaining'] == 0
    assert checkpoint['user_test_limit_bytes'] == 20 * 1024**3
    categories = ['artifact_sha256'] + ([] if args.historical_artifacts_only else ['source_sha256'])
    for category in categories:
        for relative, digest in checkpoint[category].items():
            path = Path(relative)
            assert not path.is_absolute() and '..' not in path.parts
            path = ROOT / path
            assert path.is_file() and not path.is_symlink() and sha(path) == digest, str(path)
    expected = {'core': (135, 0), 'sanitized-core': (135, 0), 'widgets': (95, 0), 'windows-core': (134, 1)}
    for name, (passed, skipped) in expected.items():
        record = checkpoint['regressions'][name]
        assert record['passed'] == passed and record['failed'] == 0 and record['skipped'] == skipped
        text = (ROOT / record['log']).read_text()
        assert f'Totals: {passed} passed, 0 failed, {skipped} skipped' in text
        assert 'FAIL!' not in text and 'QFATAL' not in text
    for record in checkpoint['process_groups']:
        result = json.loads((ROOT / record['result']).read_text())
        assert result['passed'] and result['raw_files_created'] == 0
        assert result['runner_sha256'] == checkpoint['source_sha256']['tests/run_config_transaction_tests.py']
        assert result['parallel_readers_supported'] and result['dead_reader_reservation_reconciled']
        assert result['crashed_writer_remains_pending'] and result['explicit_original_candidate_recovery']
        assert result['probe_sha256'] == sha(ROOT / record['archived_program'])
        runs = result['runs']
        assert runs[0]['exit_code'] == 3 and 'transaction busy' in runs[0]['output']
        assert json.loads(runs[1]['output']) == {'a': '2', 'b': '2'}
        assert json.loads(runs[2]['output']) == {'a': '1', 'b': '1'}
        assert runs[3]['exit_code'] == 3 and 'unfinished Save All' in runs[3]['output']
        assert runs[4]['mode'] == 'recover' and runs[4]['exit_code'] == 0
        assert json.loads(runs[5]['output']) == {'a': '2', 'b': '2'}
    assert len(checkpoint['process_groups']) == 3
    for record in checkpoint['package_cli_groups']:
        result = json.loads((ROOT / record['result']).read_text())
        assert result['passed'] and result['executed_from_package_paths'] and result['raw_files_created'] == 0
        assert result['active_writer_refused'] and result['pending_crash_refused']
        assert result['explicit_recovery_then_snapshot_validated']
    assert len(checkpoint['package_cli_groups']) == 2
    for record in checkpoint['packages']:
        folder = ROOT / record['directory']
        manifest = json.loads((folder / 'manifest.json').read_text())
        assert manifest['status'] == 'complete' and not (folder / 'BUILD_INCOMPLETE').exists()
        for relative, digest in manifest['files'].items():
            path = Path(relative)
            assert not path.is_absolute() and '..' not in path.parts
            assert sha(folder / path) == digest
    print('PASS configuration transaction checkpoint; frozen artifacts verified; goal remains incomplete')


if __name__ == '__main__':
    main()
