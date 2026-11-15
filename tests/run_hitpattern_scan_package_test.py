#!/usr/bin/env python3
"""Run the offline hit-pattern scan CLI regression and emit package evidence."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 1024 * 1024
SCENARIOS = ('cli_help', 'generic_structure_inspection', 'single_file_counts', 'multi_file_aggregation',
             'mixed_endian', 'mapping_marked_unverified',
             'descending_code_rejected', 'malformed_file_rejected',
             'temporary_raw_cleanup')


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def frame(channels, little=True):
    data = bytearray(80)
    data[0] = 0x80 if little else 0

    def put(offset, size, value):
        data[offset:offset + size] = value.to_bytes(size, 'little' if little else 'big')

    put(1, 3, 80)
    data[4] = 1
    put(5, 2, 1)
    data[7] = 5
    put(8, 2, 80)
    put(10, 2, 0)
    put(12, 4, 0)
    data[26] = 2
    data[27] = 1
    pattern = 31 + 2 * 9
    for channel in channels:
        byte = channel // 8
        offset = pattern + (byte if little else 8 - byte)
        data[offset] |= 1 << (channel % 8)
    return bytes(data)


def run(binary, *args):
    return subprocess.run([str(binary), *map(str, args)], cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, timeout=30, check=False)


def csv_rows(text):
    return list(csv.DictReader(line for line in text.splitlines()
                               if line and not line.startswith('#')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspect-binary', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    binary = args.inspect_binary.resolve(strict=True)
    if binary.name != 'getdaq-inspect' or not os.access(binary, os.X_OK):
        raise RuntimeError('Expected an executable getdaq-inspect binary')

    scenarios = {name: False for name in SCENARIOS}
    generated = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-hitpattern-scan-package-') as owned_root:
        root = Path(owned_root)
        a, b, c, malformed = (root / name for name in
                              ('a.graw', 'b.graw', 'c.graw', 'truncated.graw'))
        fixtures = ((a, frame((0, 7))), (b, frame((0, 8))),
                    (c, frame((8, 67), little=False)),
                    (malformed, frame((1,))[:40]))
        for path, payload in fixtures:
            path.write_bytes(payload)
            generated += len(payload)

        help_result = run(binary, '--help')
        scenarios['cli_help'] = (help_result.returncode == 0 and
                                 '--hit-pattern-scan' in help_result.stdout)

        generic = run(binary, a)
        scenarios['generic_structure_inspection'] = (
            generic.returncode == 0 and 'VALID_STRUCTURE frames=1 bytes=80' in generic.stdout and
            'CoBo2/AsAd1 frames=1' in generic.stdout)

        single = run(binary, '--hit-pattern', a, 2, 1, 2)
        single_rows = csv_rows(single.stdout)
        single_values = {int(row['channel']): int(row['hit_frames'])
                         for row in single_rows}
        scenarios['single_file_counts'] = (single.returncode == 0 and
            len(single_rows) == 68 and single_values[0] == 1 and
            single_values[7] == 1 and single_values[67] == 0)

        summary = run(binary, '--hit-pattern-scan', 2, 1, 2,
                      10, a, 10, b, 11, c)
        rows = csv_rows(summary.stdout)
        values = {(int(row['threshold_code']), int(row['channel'])):
                  (int(row['hit_frames']), int(row['matching_frames']))
                  for row in rows}
        scenarios['multi_file_aggregation'] = (summary.returncode == 0 and
            len(rows) == 136 and values.get((10, 0)) == (2, 2) and
            values.get((10, 8)) == (1, 2) and values.get((11, 67)) == (1, 1))
        scenarios['mixed_endian'] = values.get((11, 8)) == (1, 1)
        scenarios['mapping_marked_unverified'] = (
            'association was not verified' in summary.stdout)

        descending = run(binary, '--hit-pattern-scan', 2, 1, 2,
                         11, c, 10, a)
        scenarios['descending_code_rejected'] = (descending.returncode != 0 and
                                                  not descending.stdout)
        invalid = run(binary, '--hit-pattern-scan', 2, 1, 2,
                      10, malformed)
        scenarios['malformed_file_rejected'] = (invalid.returncode != 0 and
                                                  not invalid.stdout)

        for path, _ in fixtures:
            path.unlink()
        scenarios['temporary_raw_cleanup'] = not any(path.exists() for path, _ in fixtures)
        if not all(scenarios.values()):
            raise RuntimeError('Hit-pattern package regression failed: ' +
                               json.dumps(scenarios, sort_keys=True))
        if any(path.exists() for path, _ in fixtures):
            raise RuntimeError('Synthetic raw fixtures remain before temporary cleanup')

    result = {
        'schema': 'getdaq.hitpattern-scan-regression:1',
        'passed': True,
        'runner_sha256': sha(Path(__file__)),
        'program_sha256': {'inspect': sha(binary)},
        'scenario': scenarios,
        'maximum_aggregate_test_bytes': MAX_BYTES,
        'synthetic_raw_bytes_generated': generated,
        'synthetic_raw_bytes_deleted': generated,
        'raw_remaining': 0,
        'owned_test_roots_empty': not Path(owned_root).exists(),
        'real_hardware': False,
        'target_runtime': 'host build; Ubuntu 24 loader checked by package refresh tool'
    }
    args.result.parent.mkdir(parents=True, exist_ok=True)
    args.result.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n',
                           encoding='utf-8')
    print('PASS hit-pattern scan CLI regression; deleted {} synthetic raw bytes'.format(generated))


if __name__ == '__main__':
    main()
