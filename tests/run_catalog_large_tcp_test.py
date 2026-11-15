#!/usr/bin/env python3
"""Run one bounded large-file plain-TCP catalog transfer and verify cleanup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import shutil
import subprocess
import tempfile

MAX_TOTAL_RAW_BYTES = 16 * 1024 ** 3
DEFAULT_FILE_BYTES = 512 * 1024 ** 2
FIXED_RAW_BYTES = 4 * 4096


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def run(args):
    loader = Path(args.loader).resolve(strict=True)
    tests = Path(args.transfer_tests).resolve(strict=True)
    catalog = Path(args.catalog).resolve(strict=True)
    catalog_launcher = Path(getattr(args, 'catalog_launcher', None) or catalog).resolve(strict=True)
    file_bytes = args.file_bytes
    total_raw = 4 * file_bytes + FIXED_RAW_BYTES  # source catalog plus received copy
    if file_bytes <= 0 or total_raw > MAX_TOTAL_RAW_BYTES:
        raise RuntimeError('The source and destination raw files must fit the 16 GiB test budget')
    if args.timeout_seconds <= 0:
        raise RuntimeError('Timeout must be positive')
    evidence_input = Path(os.path.abspath(args.evidence))
    if evidence_input.exists() or evidence_input.is_symlink():
        raise FileExistsError('Refusing to overwrite evidence: ' + str(evidence_input))
    evidence_input.parent.mkdir(parents=True, exist_ok=True)
    evidence = evidence_input.parent.resolve() / evidence_input.name
    if evidence.exists() or evidence.is_symlink():
        raise FileExistsError('Refusing to overwrite evidence: ' + str(evidence))

    temporary = Path(tempfile.mkdtemp(prefix='getdaq-catalog-large-tcp-'))
    raw_remaining = None
    passed = False
    try:
        required_free = total_raw + 256 * 1024 ** 2
        free = shutil.disk_usage(temporary).free
        if free < required_free:
            raise RuntimeError('Insufficient free space for the predeclared raw budget plus 256 MiB margin')
        private_root = temporary / 'fixtures'
        private_root.mkdir(mode=0o700)
        env = os.environ.copy()
        for key in list(env):
            if key.startswith('GETDAQ_CATALOG_TEST_') or key == 'GETDAQ_TRANSFER_TEST_SHM_ROOT':
                env.pop(key, None)
        env['GETDAQ_CATALOG_TEST_ROOT'] = str(private_root)
        command = [str(loader), '--library-path', args.library_path, str(tests),
                   '--large-tcp-loopback', str(file_bytes), str(catalog_launcher)]
        process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True,
                                    start_new_session=True)
        try:
            output, _ = process.communicate(timeout=args.timeout_seconds)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                output, _ = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                output, _ = process.communicate()
            raise RuntimeError('Large transfer timed out; the owned process group was terminated: ' + output[-4096:])
        if process.returncode:
            raise RuntimeError('Large transfer failed (%d): %s' % (process.returncode, output))
        match = re.search(
            r'aggregate_raw_bytes=(\d+); largest_raw_file_bytes=(\d+); '
            r'test_max_rss_kib process=(\d+) sender=(\d+) receiver=(\d+); owned raw cleaned',
            output)
        if not match:
            raise RuntimeError('Transfer did not report byte accounting, process RSS, and cleanup')
        observed_total, largest, parent_rss, sender_rss, receiver_rss = map(int, match.groups())
        if observed_total != total_raw or largest != file_bytes:
            raise RuntimeError('Observed raw sizes differ from the predeclared transfer budget')
        if max(parent_rss, sender_rss, receiver_rss) >= args.rss_limit_kib:
            raise RuntimeError('A transfer process exceeded the configured peak RSS limit')
        leftovers = list(private_root.iterdir())
        if leftovers:
            raw_remaining = sum(p.stat().st_size for p in private_root.rglob('*.graw') if p.is_file())
            raise RuntimeError('Test fixture root still contains entries: ' + ', '.join(str(p) for p in leftovers[:5]))
        raw_remaining = 0
        record = {
            'schema': 'getdaq.catalog-large-plain-tcp:1',
            'status': 'passed',
            'candidate': args.candidate,
            'runtime': args.runtime,
            'catalog_sha256': sha(catalog),
            'catalog_launcher_sha256': sha(catalog_launcher),
            'catalog_launcher_is_product_binary': catalog_launcher == catalog,
            'catalog_transfer_tests_sha256': sha(tests),
            'runner_sha256': sha(Path(__file__)),
            'per_source_large_file_bytes': file_bytes,
            'largest_raw_file_bytes': largest,
            'source_and_destination_raw_bytes': observed_total,
            'raw_budget_bytes': MAX_TOTAL_RAW_BYTES,
            'raw_files_remaining': raw_remaining,
            'private_test_root_empty': True,
            'hardware_accessed': False,
            'memory_peak_rss_kib': {
                'test_process': parent_rss,
                'sender_cli': sender_rss,
                'receiver_cli': receiver_rss,
                'limit': args.rss_limit_kib,
            },
            'test_output_sha256': hashlib.sha256(output.encode()).hexdigest(),
        }
        with evidence.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(record, indent=2, sort_keys=True) + '\n')
        passed = True
        print(json.dumps(record, indent=2, sort_keys=True))
    finally:
        if temporary.exists():
            raw_remaining = sum(p.stat().st_size for p in temporary.rglob('*.graw') if p.is_file())
            shutil.rmtree(temporary)
        if not passed and evidence.exists():
            evidence.unlink()
        if raw_remaining not in (None, 0):
            raise RuntimeError('Owned test raw remained after cleanup')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader', required=True)
    parser.add_argument('--library-path', required=True)
    parser.add_argument('--transfer-tests', required=True)
    parser.add_argument('--catalog', required=True)
    parser.add_argument('--catalog-launcher',
                        help='Optional executable wrapper used to launch the separately hashed catalog binary')
    parser.add_argument('--evidence', required=True)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--runtime', required=True)
    parser.add_argument('--file-bytes', type=int, default=DEFAULT_FILE_BYTES)
    parser.add_argument('--timeout-seconds', type=int, default=1800)
    parser.add_argument('--rss-limit-kib', type=int, default=256 * 1024)
    run(parser.parse_args())


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        raise SystemExit('catalog large plain-TCP test failed: ' + str(error))
