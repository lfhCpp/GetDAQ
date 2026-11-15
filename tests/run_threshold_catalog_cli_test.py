#!/usr/bin/env python3
"""Exercise the packaged catalog threshold-analysis CLI with a tiny sealed fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

TOTAL_LIMIT = 20 * 1024 ** 3
FIXTURE_BYTES = 320


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def run(args):
    driver = Path(args.test_driver).resolve(strict=True)
    catalog = Path(args.catalog).resolve(strict=True)
    runner = Path(__file__).resolve()
    evidence = Path(args.evidence).absolute()
    if evidence.exists() or evidence.is_symlink():
        raise FileExistsError('Refusing to overwrite threshold CLI evidence: ' + str(evidence))
    evidence.parent.mkdir(parents=True, exist_ok=True)
    owned = Path(tempfile.mkdtemp(prefix='getdaq-threshold-cli-'))
    passed = False
    try:
        env = os.environ.copy()
        env['GETDAQ_CATALOG_TEST_ROOT'] = str(owned)
        if args.loader:
            loader = Path(args.loader).resolve(strict=True)
            library_path = args.library_path
            command = [str(loader), '--library-path', library_path, str(driver),
                       '--threshold-cli-smoke-with-loader', str(loader), library_path, str(catalog)]
            runtime = args.runtime or 'explicit target loader'
        else:
            command = [str(driver), '--threshold-cli-smoke', str(catalog)]
            runtime = args.runtime or 'native host runtime'
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=120, check=False)
        expected = 'PASS threshold catalog command-line aggregation; synthetic_raw_bytes=320; owned raw cleaned'
        if result.returncode or expected not in result.stdout or 'getdaq.threshold-hit-pattern-analysis' not in result.stdout:
            raise RuntimeError('Threshold catalog CLI smoke failed:\n' + result.stdout)
        if any(owned.iterdir()):
            raise RuntimeError('Owned threshold fixture directories remain after CLI smoke')
        record = {
            'schema': 'getdaq.threshold-catalog-analysis-regression:1',
            'status': 'passed', 'passed': True,
            'candidate': args.candidate,
            'program_sha256': {'catalog': sha(catalog), 'test_driver': sha(driver)},
            'catalog_delivery_binary_sha256': sha(catalog),
            'runner_sha256': sha(runner),
            'scenario': {
                'bound_scan_point_to_sealed_catalog': True,
                'packaged_cli_produced_expected_channel_counts': True,
                'catalog_report_not_overwritten': True,
                'fixture_root_removed': True,
            },
            'synthetic_raw_bytes_generated': FIXTURE_BYTES,
            'synthetic_raw_bytes_deleted': FIXTURE_BYTES,
            'maximum_aggregate_test_bytes': FIXTURE_BYTES,
            'raw_remaining': 0, 'owned_test_roots_empty': True,
            'real_hardware': False, 'runtime': runtime,
            'smoke_output_sha256': hashlib.sha256(result.stdout.encode()).hexdigest(),
        }
        with evidence.open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(record, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write('\n')
        passed = True
        print(json.dumps(record, ensure_ascii=False, indent=2, sort_keys=True))
    finally:
        shutil.rmtree(owned)
        if not passed and evidence.exists():
            evidence.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-driver', required=True)
    parser.add_argument('--catalog', required=True)
    parser.add_argument('--candidate', default='')
    parser.add_argument('--evidence', required=True)
    parser.add_argument('--loader')
    parser.add_argument('--library-path', default='')
    parser.add_argument('--runtime', default='')
    args = parser.parse_args()
    if args.loader and not args.library_path:
        parser.error('--library-path is required with --loader')
    if FIXTURE_BYTES > TOTAL_LIMIT:
        raise RuntimeError('Threshold CLI fixture exceeds 20 GiB raw budget')
    run(args)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit('threshold catalog CLI regression failed: ' + str(error))
