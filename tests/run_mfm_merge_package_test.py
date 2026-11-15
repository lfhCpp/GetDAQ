#!/usr/bin/env python3
"""Run the native MFM merge regression and emit SHA-bound package evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 20 * 1024 ** 3
EXPECTED = (
    'event_sort', 'same_event_grouping', 'input_order_preserved',
    'unsupported_type_skipped', 'embedded_bytes_preserved', 'mfm_outer_header',
    'existing_output_not_overwritten', 'temporary_inputs_removed',
)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def cobo_frame(frame_type, event, marker):
    frame = bytearray(28)
    frame[3] = 28
    frame[5:7] = frame_type.to_bytes(2, 'big')
    frame[7] = 5 if frame_type == 2 else 1
    frame[8:10] = (28).to_bytes(2, 'big')
    frame[21] = marker
    frame[22:26] = event.to_bytes(4, 'big')
    frame[26] = 1
    return bytes(frame)


def verify_cli_output(path):
    payload = path.read_bytes()
    frames = []
    offset = 0
    while offset < len(payload):
        header = payload[offset:offset + 20]
        if len(header) != 20 or int.from_bytes(header[5:7], 'big') != 0xff01:
            raise RuntimeError('CLI output is not an MFM 0xFF01 layered frame')
        block = 1 << (header[0] & 15)
        size = int.from_bytes(header[1:4], 'big') * block
        hsize = int.from_bytes(header[8:10], 'big') * block
        count = int.from_bytes(header[12:16], 'big')
        event = int.from_bytes(header[16:20], 'big')
        if size < hsize or offset + size > len(payload):
            raise RuntimeError('CLI output contains an invalid outer frame size')
        children = []
        child_offset = offset + hsize
        for _ in range(count):
            prefix = payload[child_offset:child_offset + 8]
            if len(prefix) != 8:
                raise RuntimeError('CLI output contains a truncated embedded frame')
            child_size = int.from_bytes(prefix[1:4], 'big') * (1 << (prefix[0] & 15))
            children.append(payload[child_offset:child_offset + child_size])
            child_offset += child_size
        if child_offset > offset + size:
            raise RuntimeError('Embedded frames exceed the outer frame boundary')
        frames.append((event, children))
        offset += size
    if [(event, len(children)) for event, children in frames] != [(2, 1), (5, 2)]:
        raise RuntimeError('CLI output has incorrect event sorting/grouping')
    if [children[0][21] for _, children in frames] != [0xb2, 0xa5] or frames[1][1][1][21] != 0xc5:
        raise RuntimeError('CLI output did not preserve embedded frame bytes/order')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--merge-binary', type=Path, required=True)
    parser.add_argument('--test-binary', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    merge_binary = args.merge_binary.resolve(strict=True)
    test_binary = args.test_binary.resolve(strict=True)
    if merge_binary.name != 'getdaq-merge' or test_binary.name != 'merge-tests':
        raise RuntimeError('Expected getdaq-merge and merge-tests binaries')
    if not os.access(merge_binary, os.X_OK) or not os.access(test_binary, os.X_OK):
        raise RuntimeError('Regression inputs must be executable')

    with tempfile.TemporaryDirectory(prefix='getdaq-mfm-merge-package-') as owned_root:
        environment = dict(os.environ)
        environment['TMPDIR'] = owned_root
        result = subprocess.run([str(test_binary)], cwd=ROOT, env=environment,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=120, check=False)
        output = result.stdout
        if result.returncode:
            raise RuntimeError('mfm-merge test failed:\n' + output)
        scenario = {name: name in output.split() for name in EXPECTED}
        if not all(scenario.values()):
            raise RuntimeError('mfm-merge test omitted required scenario markers:\n' + output)
        if 'synthetic_raw_bytes=112' not in output.split():
            raise RuntimeError('mfm-merge test did not report the expected 112-byte raw fixture')
        help_result = subprocess.run([str(merge_binary), '--help'], stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, timeout=30, check=False)
        if help_result.returncode or 'Usage: getdaq-merge' not in help_result.stdout:
            raise RuntimeError('getdaq-merge --help failed:\n' + help_result.stdout)
        scenario['cli_help'] = True
        cli_root = Path(owned_root) / 'cli'
        cli_root.mkdir()
        input1 = cli_root / 'one.graw'
        input2 = cli_root / 'two.graw'
        output = cli_root / 'merged.gmer'
        input1.write_bytes(cobo_frame(1, 5, 0xa5) + cobo_frame(1, 2, 0xb2))
        input2.write_bytes(cobo_frame(2, 5, 0xc5) + cobo_frame(3, 1, 0x33))
        raw_generated = input1.stat().st_size + input2.stat().st_size
        merge_result = subprocess.run([str(merge_binary), '-o', str(output), '--max-file-mib', '0',
                                       str(input1), str(input2)], stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, text=True, timeout=30, check=False)
        if merge_result.returncode or 'merged 3 frame(s) into 2 event(s)' not in merge_result.stdout:
            raise RuntimeError('getdaq-merge CLI conversion failed:\n' + merge_result.stdout)
        verify_cli_output(output)
        scenario['cli_merge_output'] = True
        input1.unlink(); input2.unlink(); output.unlink(); cli_root.rmdir()
        if list(Path(owned_root).iterdir()):
            raise RuntimeError('Owned merge-test root is not empty after the CLI test')
        raw_deleted = raw_generated

    evidence = {
        'schema': 'getdaq.mfm-merge-regression:1',
        'passed': True,
        'scenario': scenario,
        'program_sha256': {'merge': sha(merge_binary)},
        'test_binary_sha256': sha(test_binary),
        'runner_sha256': sha(Path(__file__)),
        'synthetic_raw_bytes_generated': 112 + raw_generated,
        'synthetic_raw_bytes_deleted': 112 + raw_deleted,
        'maximum_aggregate_test_bytes': 112 + raw_generated,
        'raw_remaining': 0,
        'owned_test_roots_empty': True,
        'real_hardware': False,
        'runtime': 'native C++11 test binary; no network transport',
    }
    if evidence['maximum_aggregate_test_bytes'] > MAX_BYTES:
        raise RuntimeError('Synthetic test budget exceeds 20 GiB')
    args.result.parent.mkdir(parents=True, exist_ok=True)
    args.result.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(evidence, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        print('MFM merge package regression failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
