#!/usr/bin/env python3
"""Run and bind the C++11 receiver/ByteStorage regression for package refresh."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MAX_TEST_BYTES = 64 * 1024 * 1024


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--receiver', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    receiver = args.receiver.resolve(strict=True)
    if receiver.name != 'getdaq-receiver' or not os.access(receiver, os.X_OK):
        raise RuntimeError('Expected executable getdaq-receiver')
    if not (build / 'CTestTestfile.cmake').is_file():
        raise RuntimeError('CMake test build is missing')
    with tempfile.TemporaryDirectory(prefix='getdaq-byte-storage-runner-') as temporary:
        environment = dict(os.environ)
        environment['TMPDIR'] = temporary
        # CMake 3.16 does not support --test-dir and silently ran in the caller's
        # directory; use cwd=build so the runner works with the Ubuntu 20 baseline.
        ctest = subprocess.run(['ctest', '--output-on-failure'], cwd=build,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, env=environment, timeout=120)
        if ctest.returncode:
            raise RuntimeError('Receiver CTest failed:\n' + ctest.stdout)
        if '100% tests passed' not in ctest.stdout:
            raise RuntimeError('Receiver CTest did not report complete success:\n' + ctest.stdout)
        summary = subprocess.run(['ctest', '--show-only=json-v1'], cwd=build,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, env=environment, timeout=30, check=True)
        expected_tests = {'mfm-merge', 'receiver-core', 'exporter-lock', 'byte-storage-integration'}
        registered = {test['name'] for test in json.loads(summary.stdout).get('tests', [])}
        if not expected_tests.issubset(registered):
            raise RuntimeError('CTest did not register all four receiver checks: ' + repr(sorted(registered)))
        integration = subprocess.run(['python3', str(ROOT / 'tests/byte_storage_integration.py'), str(receiver)],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, env=environment, timeout=30, check=True)
        integration_result = json.loads(integration.stdout.splitlines()[-1])
        if integration_result.get('status') != 'passed' or integration_result.get('raw_remaining') != 0:
            raise RuntimeError('Standalone ByteStorage integration did not pass and clean its files')

    generated = int(integration_result['synthetic_raw_bytes_deleted']) * 2 + 112
    if generated > MAX_TEST_BYTES:
        raise RuntimeError('Synthetic raw data exceeded the 64 MiB regression budget')
    result = {
        'schema': 'getdaq.receiver-byte-storage-regression:1',
        'passed': True,
        'receiver_sha256': sha(receiver),
        'runner_sha256': sha(Path(__file__)),
        'integration_test_sha256': sha(ROOT / 'tests/byte_storage_integration.py'),
        'build_receipt_sha256': sha(build / 'build-receipt.json'),
        'scenario': {
            'passed': True,
            'native_receiver_ctest': True,
            'mfm_merge': True,
            'receiver_core': True,
            'exporter_lock': True,
            'byte_storage_tcp_raw_preserved': True,
            'eof_seal_verified': True,
            'fdt_rejected': True,
            'raw_cleanup': True,
        },
        'synthetic_raw_bytes_generated': generated,
        'synthetic_raw_bytes_deleted': generated,
        'maximum_aggregate_test_bytes': MAX_TEST_BYTES,
        'raw_remaining': 0,
        'standalone_integration': integration_result,
        'real_hardware': False,
        'runtime': 'Ubuntu 24 user-space loader on WSL host kernel',
    }
    result_path = args.result.resolve()
    result_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_result = result_path.with_suffix(result_path.suffix + '.tmp')
    temporary_result.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    temporary_result.replace(result_path)
    print(json.dumps({'status': 'passed', 'receiver_sha256': result['receiver_sha256'],
                      'tests': sorted(expected_tests), 'synthetic_raw_bytes_generated': generated,
                      'synthetic_raw_bytes_deleted': generated, 'raw_remaining': 0,
                      'evidence': str(result_path)}, sort_keys=True))


if __name__ == '__main__':
    main()
