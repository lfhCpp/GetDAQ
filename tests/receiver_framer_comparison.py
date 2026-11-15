#!/usr/bin/env python3
"""Compare the archived buffered framer and current framer using memory only.

This benchmark samples headers and one payload byte, so its logical byte rate
must never be described as network, disk or production DAQ throughput.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=root / 'build-receiver')
    parser.add_argument('--baseline', type=Path, required=True,
                        help='Archived verified-sources root, containing the original mfmstream files')
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    build, evidence = args.build.resolve(), args.evidence.resolve()
    assert not evidence.exists(), 'Use a new evidence directory; preserve previous measurements'
    receipt = json.loads((build / 'build-receipt.json').read_text())
    assert not receipt['sanitized'], 'Do not compare instrumented and release benchmarks'
    current = build / 'mfm-stream-benchmark'
    assert sha(current) == receipt['program_sha256'][current.name]
    inputs = json.loads((build / 'source-sha256.json').read_text())
    for name, digest in inputs.items(): assert sha(root / name) == digest, name
    # Identify the measured original implementation by its pre-change source hashes.
    baseline_hashes = {
        'mfmstream.cpp': '9802b3e74735a98d82dfcb71265e0f9885cfddbb470d9e2565c08e46d60ebea3',
        'mfmstream.h': 'aa369894ca08b2947055777267a3aef6320a0c3e5e6d3bf44b867275575d8fb1',
    }
    source = args.baseline.resolve() / 'apps/datareceiver'
    for name, digest in baseline_hashes.items(): assert sha(source / name) == digest, name
    evidence.mkdir(parents=True)
    for name in baseline_hashes: shutil.copy2(source / name, evidence / name)
    benchmark = root / 'tests/mfm_stream_benchmark.cpp'
    shutil.copy2(benchmark, evidence / benchmark.name)
    command = ['c++', '-std=c++11', '-pedantic-errors', '-O3', '-DNDEBUG',
               '-Wall', '-Wextra', '-Wpedantic', '-I', str(evidence),
               str(evidence / 'mfmstream.cpp'), str(evidence / benchmark.name),
               '-o', str(evidence / 'baseline-benchmark')]
    with (evidence / 'baseline-build.txt').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    records = []
    for frame, chunk in ((4096, 262144), (65536, 262144), (1048576, 262144), (4096, 127)):
        trials = {'baseline': [], 'current': []}
        for repeat in range(3):
            for label, program in (('baseline', evidence / 'baseline-benchmark'), ('current', current)):
                value = json.loads(subprocess.check_output([str(program), '1024', str(frame), str(chunk)], text=True))
                assert value['total_bytes'] == 1024**3
                assert value['frames'] == value['total_bytes'] // frame
                borrowed = label == 'current' and frame <= chunk
                assert value['borrowed_frame_bytes'] == (1024**3 if borrowed else 0)
                assert value['buffered_frame_bytes'] == (0 if borrowed else 1024**3)
                trials[label].append(value)
        old = statistics.median(value['seconds'] for value in trials['baseline'])
        new = statistics.median(value['seconds'] for value in trials['current'])
        records.append({'frame_bytes': frame, 'feed_chunk_bytes': chunk, 'trials': trials,
                        'baseline_median_seconds': old, 'current_median_seconds': new,
                        'elapsed_time_ratio_baseline_over_current': old / new})
    result = {'schema': 1, 'scope': __doc__, 'disk_payload_bytes': 0,
              'live_input_buffer_bytes': 8 * 1024**2, 'repeat_count': 3,
              'baseline_source_sha256': baseline_hashes, 'current_source_sha256': inputs,
              'benchmark_source_sha256': sha(benchmark), 'runner_sha256': sha(Path(__file__)),
              'program_sha256': {'baseline': sha(evidence / 'baseline-benchmark'), 'current': sha(current)},
              'baseline_compile_command': command, 'cases': records}
    (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS four framer comparisons; no raw files created; memory-only measurements:', evidence)


if __name__ == '__main__':
    main()
