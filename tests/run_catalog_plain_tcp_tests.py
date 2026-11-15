#!/usr/bin/env python3
"""Run the bounded plain TCP catalog regression under a selected Linux runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

MAX_BYTES = 64 * 1024 * 1024
TOTAL_LIMIT = 20 * 1024 ** 3


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def run_target(loader, libraries, program, *args, env):
    command = [str(loader), '--library-path', libraries, str(program), *map(str, args)]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True,
                            timeout=300, check=False)
    if result.returncode:
        raise RuntimeError('Command failed (%d): %s\n%s' %
                           (result.returncode, ' '.join(command), result.stdout))
    return result.stdout


def run(args):
    cap = min(args.max_test_bytes, MAX_BYTES, TOTAL_LIMIT)
    if cap <= 0:
        raise RuntimeError('Test data cap must be positive')
    loader = Path(args.loader).resolve(strict=True)
    libraries = args.library_path
    catalog = Path(args.catalog).resolve(strict=True)
    system_tests = Path(args.system_tests).resolve(strict=True)
    transfer_tests = Path(args.transfer_tests).resolve(strict=True)
    evidence_input = Path(os.path.abspath(args.evidence))
    if evidence_input.exists() or evidence_input.is_symlink():
        raise FileExistsError('Refusing to overwrite existing transfer evidence: ' + str(evidence_input))
    evidence_input.parent.mkdir(parents=True, exist_ok=True)
    evidence = evidence_input.parent.resolve() / evidence_input.name
    if evidence.exists() or evidence.is_symlink():
        raise FileExistsError('Refusing to overwrite existing transfer evidence: ' + str(evidence))
    tmp_parent = None
    shm_parent = None
    evidence_created = False
    passed = False
    try:
        tmp_parent = Path(tempfile.mkdtemp(prefix='getdaq-catalog-tcp-test-'))
        shm_parent = Path(tempfile.mkdtemp(prefix='getdaq-catalog-tcp-test-', dir='/dev/shm'))
        fixture_root = tmp_parent / 'fixtures'
        shm_root = shm_parent / 'shared'
        fixture_root.mkdir(mode=0o700)
        shm_root.mkdir(mode=0o700)
        env = os.environ.copy()
        for key in ('GETDAQ_CATALOG_TEST_REAL_SSH', 'GETDAQ_CATALOG_TEST_REAL_SSH_TARGET',
                    'GETDAQ_CATALOG_TEST_REMOTE_PROGRAM', 'GETDAQ_CATALOG_TEST_REMOTE_ROOT',
                    'GETDAQ_CATALOG_TEST_REAL_SSH_FILE_BYTES',
                    'GETDAQ_CATALOG_TEST_REAL_SSH_PAYLOAD_BUDGET_BYTES',
                    'GETDAQ_CATALOG_TEST_FAKE_SSH', 'GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS'):
            env.pop(key, None)
        env['GETDAQ_CATALOG_TEST_ROOT'] = str(fixture_root)
        env['GETDAQ_TRANSFER_TEST_SHM_ROOT'] = str(shm_root)
        system_output = run_target(loader, libraries, system_tests, env=env)
        transfer_output = run_target(loader, libraries, transfer_tests, catalog, env=env)
        expected = ('thread-local SIGPIPE isolation, all four oversized TCP metadata fields rejected before staging, '
                    'plain TCP new-transfer/interruption-resume/readback/completed-no-payload-resume/'
                    'CLI bounded connection refusal/listener-send separate-process/readback')
        if expected not in transfer_output:
            raise RuntimeError('Transfer test did not report every plain-TCP scenario')
        match = re.search(r'synthetic_raw_bytes=(\d+); owned raw cleaned', transfer_output)
        if not match:
            raise RuntimeError('Transfer test omitted its synthetic raw byte/cleanup result')
        generated = int(match.group(1))
        if generated > cap or generated > TOTAL_LIMIT:
            raise RuntimeError('Synthetic raw test data exceeded its predeclared budget')
        if any(fixture_root.iterdir()) or any(shm_root.iterdir()):
            raise RuntimeError('Owned transfer test directories remain after successful run')
        help_output = run_target(loader, libraries, catalog, '--help', env=env)
        if '--send-catalog-tcp' not in help_output or '--listen-catalog-tcp' not in help_output:
            raise RuntimeError('Refreshed catalog help omits the plain TCP endpoints')
        result = {
            'schema': 'getdaq.catalog-plain-tcp-regression:1',
            'status': 'passed',
            'passed': True,
            'candidate': args.candidate,
            'catalog_delivery_binary_sha256': sha(catalog),
            'program_sha256': {
                'catalog': sha(catalog),
                'system_experiment_tests': sha(system_tests),
                'catalog_transfer_tests': sha(transfer_tests),
            },
            'runner_sha256': sha(Path(__file__)),
            'scenario': {
                'thread_local_sigpipe_isolation': True,
                'oversized_metadata_fields_rejected_before_staging': True,
                'plain_tcp_no_authentication': True,
                'interrupted_partial_resumed': True,
                'receiver_readback_verified': True,
                'completed_resume_no_payload': True,
                'separate_cli_listener_send_readback': True,
                'cli_connection_refusal_bounded': True,
                'native_catalog_regressions': True,
            },
            'synthetic_raw_bytes_generated': generated,
            'synthetic_raw_bytes_deleted': generated,
            'synthetic_payload_max_bytes': cap,
            'maximum_aggregate_test_bytes': cap,
            'raw_remaining': 0,
            'raw_files_remaining': 0,
            'owned_test_roots_empty': True,
            'real_hardware': False,
            'runtime': args.runtime,
            'catalog_help_sha256': hashlib.sha256(help_output.encode()).hexdigest(),
            'system_test_output_sha256': hashlib.sha256(system_output.encode()).hexdigest(),
            'transfer_test_output_sha256': hashlib.sha256(transfer_output.encode()).hexdigest(),
        }
        with evidence.open('x', encoding='utf-8', newline='\n') as stream:
            evidence_created = True
            stream.write(json.dumps(result, indent=2, sort_keys=True) + '\n')
        passed = True
        print(json.dumps(result, indent=2, sort_keys=True))
    finally:
        # The two private roots were created by this runner; remove only those roots.
        if shm_parent is not None:
            shutil.rmtree(shm_parent, ignore_errors=False)
        if tmp_parent is not None:
            shutil.rmtree(tmp_parent, ignore_errors=False)
        if not passed and evidence_created and evidence.exists():
            evidence.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader', required=True)
    parser.add_argument('--library-path', required=True)
    parser.add_argument('--catalog', required=True)
    parser.add_argument('--system-tests', required=True)
    parser.add_argument('--transfer-tests', required=True)
    parser.add_argument('--evidence', required=True)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--runtime', required=True,
                        help='Exact user-space/runtime used for the test; recorded verbatim in evidence')
    parser.add_argument('--max-test-bytes', type=int, default=MAX_BYTES)
    run(parser.parse_args())


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        raise SystemExit('catalog plain-TCP regression failed: ' + str(error))
