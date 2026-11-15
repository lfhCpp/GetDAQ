#!/usr/bin/env python3
"""Verify frozen path migration evidence without editing files or starting services."""
import argparse
import hashlib
import itertools
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
    parser.add_argument('--checkpoint', type=Path, default=ROOT / 'build-config-migration/checkpoint.json')
    parser.add_argument('--historical-artifacts-only', action='store_true')
    args = parser.parse_args()
    c = json.loads(args.checkpoint.read_text())
    assert c['schema'] == 'getdaq.source-path-migration:1' and c['phase'] == '70.68'
    assert c['goal_complete'] is False and c['file_path_migration_implemented'] is True
    assert c['raw_remaining'] == 0 and c['user_test_limit_bytes'] == 20 * 1024**3
    for key in ['unopened_referrers_checked', 'uncooperative_external_atomicity_provided', 'real_hardware_accepted',
                'ubuntu24_accepted', 'macos_accepted', 'independent_windows_accepted', 'sustained_throughput_accepted',
                'system_qt_instrumented', 'abrupt_child_lsan_verified']:
        assert c[key] is False, key
    for category in ['artifact_sha256'] + ([] if args.historical_artifacts_only else ['source_sha256']):
        for relative, digest in c[category].items():
            path = Path(relative)
            assert not path.is_absolute() and '..' not in path.parts
            path = ROOT / path
            assert path.is_file() and not path.is_symlink() and sha(path) == digest, str(path)
    for label, passed, skipped in [('core', 146, 0), ('sanitized-core', 146, 0), ('widgets', 98, 0), ('windows-core', 145, 1)]:
        r = c['regressions'][label]
        assert (r['passed'], r['failed'], r['skipped']) == (passed, 0, skipped)
        text = (ROOT / r['log']).read_text()
        assert f'Totals: {passed} passed, 0 failed, {skipped} skipped' in text and 'FAIL!' not in text and 'QFATAL' not in text
    expected = {'-'.join(parts) for parts in itertools.product(['root', 'part'], ['created', 'replaced', 'removed'], ['original', 'candidate'])}
    assert len(c['process_groups']) == 3
    for entry in c['process_groups']:
        r = json.loads((ROOT / entry['result']).read_text())
        assert r['passed'] and r['raw_files_created'] == 0 and r['active_writer_cases'] == 2
        assert {s['name'] for s in r['scenarios']} == expected and len(r['scenarios']) == 12
        assert all(s['crash_exit'] == 79 for s in r['scenarios'])
        assert r['probe_sha256'] == sha(ROOT / entry['archived_program'])
        assert r['runner_sha256'] == c['source_sha256']['tests/run_source_migration_tests.py']
    assert len(c['package_cli_groups']) == 2
    for entry in c['package_cli_groups']:
        r = json.loads((ROOT / entry['result']).read_text())
        for flag in ['passed', 'executed_from_package_paths', 'preview_did_not_write_sources',
                     'actual_migration_cli_checked', 'both_recovery_directions_checked', 'snapshot_checked_by_packaged_native_tool']:
            assert r[flag] is True
        assert r['raw_files_created'] == 0
    diagnostic = (ROOT / c['windows_publish_diagnostic']).read_text(errors='replace')
    assert 'close_only_success=0 error=32 destructor_closed_success=1 error=0' in diagnostic
    for package in c['packages']:
        folder = ROOT / package['directory']
        manifest = json.loads((folder / 'manifest.json').read_text())
        assert manifest['status'] == 'complete' and not (folder / 'BUILD_INCOMPLETE').exists()
        for relative, digest in manifest['files'].items():
            path = Path(relative)
            assert not path.is_absolute() and '..' not in path.parts
            assert sha(folder / path) == digest
    print('PASS source path migration checkpoint; archived evidence verified; full goal remains incomplete')


if __name__ == '__main__':
    main()
