#!/usr/bin/env python3
"""Verify an offline waveform viewer binary without creating DAQ/raw data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve(strict=True)
    help_result = subprocess.run([str(program), '--help'], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, timeout=10)
    if help_result.returncode != 0 or 'Usage: getdaq-waveform-viewer' not in help_result.stdout:
        raise RuntimeError('waveform viewer --help failed')

    environment = os.environ.copy()
    environment['QT_QPA_PLATFORM'] = 'offscreen'
    try:
        subprocess.run([str(program)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       env=environment, timeout=3, check=False)
    except subprocess.TimeoutExpired:
        window_remained_open = True
    else:
        window_remained_open = False
    if not window_remained_open:
        raise RuntimeError('offscreen waveform viewer exited before its GUI smoke timeout')

    receipt = {
        'schema': 'getdaq.waveform-viewer-regression:1',
        'passed': True,
        'runner_sha256': sha(Path(__file__).resolve()),
        'owned_test_roots_empty': True,
        'scenario': {'cli_help': True, 'offscreen_window_startup': True},
        'program_sha256': {'waveform_viewer': sha(program)},
        'maximum_aggregate_test_bytes': 0,
        'synthetic_raw_bytes_generated': 0,
        'synthetic_raw_bytes_deleted': 0,
        'raw_remaining': 0,
        'real_hardware': False,
    }
    args.result.parent.mkdir(parents=True, exist_ok=True)
    args.result.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print('PASS offline waveform viewer help and offscreen startup; no data files created')


if __name__ == '__main__':
    main()
