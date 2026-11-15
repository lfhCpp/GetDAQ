#!/usr/bin/env python3
"""Run bounded .gmat conversion tests and write binary-bound package evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 20 * 1024 ** 3
MATRIX_BYTES = 4 * 68 * 512 * 8


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def fixture():
    item_count = 4 * 68 * 512
    block_bytes = 256
    header_bytes = block_bytes
    size = ((header_bytes + item_count * 4 + block_bytes - 1) // block_bytes) * block_bytes
    frame = bytearray(size)
    frame[0] = 8
    frame[1:4] = (size // block_bytes).to_bytes(3, 'big')
    frame[5:7] = (1).to_bytes(2, 'big')
    frame[7] = 5
    frame[8:10] = (1).to_bytes(2, 'big')
    frame[10:12] = (4).to_bytes(2, 'big')
    frame[12:16] = item_count.to_bytes(4, 'big')
    offset = header_bytes
    for aget in range(4):
        for channel in range(68):
            for sequence in range(512):
                bucket = (sequence + 100) % 512
                value = (3 + aget * 700 + channel * 7 + bucket % 7) & 0xfff
                word = (aget << 30) | (channel << 23) | (bucket << 14) | value
                frame[offset:offset + 4] = word.to_bytes(4, 'big')
                offset += 4
    return bytes(frame)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matrix-binary', type=Path, required=True)
    parser.add_argument('--test-binary', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    matrix_binary = args.matrix_binary.resolve(strict=True)
    test_binary = args.test_binary.resolve(strict=True)
    if matrix_binary.name != 'getdaq-matrix' or test_binary.name != 'matrix-tests':
        raise RuntimeError('Expected getdaq-matrix and matrix-tests binaries')
    if not os.access(matrix_binary, os.X_OK) or not os.access(test_binary, os.X_OK):
        raise RuntimeError('Regression inputs must be executable')

    with tempfile.TemporaryDirectory(prefix='getdaq-gmat-package-') as owned_root:
        environment = dict(os.environ, TMPDIR=owned_root)
        unit = subprocess.run([str(test_binary)], cwd=ROOT, env=environment,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=120, check=False)
        if (unit.returncode or 'PASS gmat_layout' not in unit.stdout or 'raw_removed=true' not in unit.stdout or
                'unsupported_revision_rejection' not in unit.stdout):
            raise RuntimeError('gmat-conversion unit test failed:\n' + unit.stdout)
        if 'synthetic_raw_bytes=' not in unit.stdout:
            raise RuntimeError('unit test did not report synthetic input size')
        unit_raw = int(unit.stdout.split('synthetic_raw_bytes=', 1)[1].split()[0])

        case = Path(owned_root) / 'cli'
        case.mkdir()
        source = case / 'sample.graw'
        destination = case / 'sample.gmat'
        raw = fixture()
        source.write_bytes(raw)
        converted = subprocess.run([str(matrix_binary), '--input', str(source), '--output', str(destination),
                                    '--count', '1'], cwd=ROOT, env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, timeout=60, check=False)
        if converted.returncode or destination.stat().st_size != MATRIX_BYTES:
            raise RuntimeError('getdaq-matrix CLI did not create one complete matrix:\n' + converted.stdout)
        output = destination.read_bytes()
        observed = [struct.unpack_from('<d', output, offset * 8)[0]
                    for offset in (0, 511, 512, 68 * 512, 4 * 68 * 512 - 1)]
        expected = [5.0, 4.0, 12.0, 705.0, float((3 + 3 * 700 + 67 * 7 + 99 % 7) & 0xfff)]
        if observed != expected:
            raise RuntimeError('CLI matrix samples have wrong order/values: %r != %r' % (observed, expected))
        existing = subprocess.run([str(matrix_binary), '--input', str(source), '--output', str(destination)],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True, timeout=60, check=False)
        if existing.returncode == 0 or destination.stat().st_size != MATRIX_BYTES:
            raise RuntimeError('CLI replaced an existing output path')
        capped = subprocess.run([str(matrix_binary), '--input', str(source), '--output', str(case / 'capped.gmat'),
                                 '--max-output-bytes', str(MATRIX_BYTES - 1)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=60, check=False)
        if capped.returncode == 0 or (case / 'capped.gmat').exists():
            raise RuntimeError('CLI did not enforce its output size limit')
        source.unlink(); destination.unlink(); case.rmdir()
        if list(Path(owned_root).iterdir()):
            raise RuntimeError('Owned gmat regression directory is not empty')

    evidence = {
        'schema': 'getdaq.gmat-conversion-regression:1',
        'passed': True,
        'scenario': {'library_layout_and_rejections': True, 'unsupported_revision_rejected': True, 'cli_help': False,
                     'cli_binary64_little_endian': True, 'cli_channel_and_bucket_order': True,
                     'cli_no_replace': True, 'cli_output_limit': True, 'synthetic_raw_cleanup': True},
        'program_sha256': {'matrix': sha(matrix_binary)},
        'test_binary_sha256': sha(test_binary),
        'runner_sha256': sha(Path(__file__)),
        'synthetic_raw_bytes_generated': unit_raw + len(raw),
        'synthetic_raw_bytes_deleted': unit_raw + len(raw),
        'maximum_aggregate_test_bytes': unit_raw + len(raw) + MATRIX_BYTES,
        'raw_remaining': 0,
        'owned_test_roots_empty': True,
        'real_hardware': False,
        'runtime': 'native C++11, synthetic local files only; no network transport',
    }
    if evidence['maximum_aggregate_test_bytes'] > MAX_BYTES:
        raise RuntimeError('Synthetic test budget exceeds 20 GiB')
    help_result = subprocess.run([str(matrix_binary), '--help'], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, timeout=30, check=False)
    if help_result.returncode or 'Usage: getdaq-matrix' not in help_result.stdout:
        raise RuntimeError('getdaq-matrix --help failed:\n' + help_result.stdout)
    evidence['scenario']['cli_help'] = True
    args.result.parent.mkdir(parents=True, exist_ok=True)
    args.result.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(evidence, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
