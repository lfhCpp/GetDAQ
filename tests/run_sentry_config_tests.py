#!/usr/bin/env python3
"""Run the Sentry configuration rejection matrix against one exact binary."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--loader', required=True, type=Path,
                        help='ELF loader used for the target userspace')
    parser.add_argument('--libraries', required=True,
                        help='Target userspace library search path')
    parser.add_argument('--evidence', required=True, type=Path)
    args = parser.parse_args()
    binary, loader = args.binary.resolve(strict=True), args.loader.resolve(strict=True)
    if binary.name != 'getdaq-sentry' or not binary.is_file() or not loader.is_file():
        parser.error('A regular getdaq-sentry binary and target ELF loader are required')

    cases = [
        ('fractional_port', {'port': 56614.5}, 'port must be an integer'),
        ('string_port', {'port': '56614'}, 'port must be an integer'),
        ('fractional_settle', {'settle_ms': 25.5}, 'settle_ms must be an integer'),
        ('string_settle', {'settle_ms': '25'}, 'settle_ms must be an integer'),
        ('non_string_process', {'process_name': 7}, 'process_name must be a string'),
        ('non_string_disk', {'disk_name': []}, 'disk_name must be a string'),
        ('non_string_bind', {'bind': 127}, 'bind must be a string'),
        ('non_boolean_process_check', {'check_process': 'false'},
         'check_process must be boolean'),
    ]
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    result = {'schema': 'getdaq.sentry-config-strict-regression:1', 'passed': False,
              'scenario': {}, 'program_sha256': {'sentry': sha(binary)},
              'runner_sha256': sha(Path(__file__).resolve()), 'cases': [],
              'synthetic_raw_bytes_generated': 0, 'synthetic_raw_bytes_deleted': 0,
              'maximum_aggregate_test_bytes': 0, 'raw_remaining': 0,
              'real_hardware': False}
    with tempfile.TemporaryDirectory(prefix='getdaq-sentry-config-') as temporary:
        root = Path(temporary)
        data_dir = root / 'data'
        data_dir.mkdir()
        for name, fields, diagnostic in cases:
            config_path = root / (name + '.json')
            config = {'data_dir': str(data_dir), 'check_process': False}
            config.update(fields)
            config_path.write_text(json.dumps(config) + '\n', encoding='utf-8')
            command = [str(loader), '--library-path', args.libraries,
                       str(binary), '--config', str(config_path)]
            completed = subprocess.run(command, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, timeout=10)
            passed = (completed.returncode == 2 and diagnostic in completed.stdout
                      and 'listening on' not in completed.stdout)
            result['cases'].append({'name': name, 'exit_code': completed.returncode,
                                    'expected_diagnostic': diagnostic, 'passed': passed})
            if not passed:
                raise RuntimeError(name + ' was not rejected before service startup: ' +
                                   completed.stdout.strip())
        if list(root.rglob('*.graw')):
            raise RuntimeError('Sentry configuration regression unexpectedly generated raw data')
        if any(path.is_symlink() for path in root.rglob('*')):
            raise RuntimeError('Unexpected symlink in private Sentry test root')
    result['scenario'] = {'all_malformed_fields_rejected_before_listener': True,
                          'all_eight_cases_passed': len(result['cases']) == 8,
                          'owned_test_root_removed': not root.exists(),
                          'raw_cleanup': True}
    result['owned_test_roots_empty'] = not root.exists()
    result['passed'] = all(result['scenario'].values())
    args.evidence.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                             encoding='utf-8')
    print('PASS Sentry strict config:', len(result['cases']), 'cases; raw files generated: 0')


if __name__ == '__main__':
    main()
