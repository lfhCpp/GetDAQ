#!/usr/bin/env python3
"""Bounded remote-manager cleanup proof; synthetic loopback, no ECC recovery claim."""
import argparse
import json
import signal
import subprocess
import tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture', 'receiver', 'service', 'evidence'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='sealed-peer-cleanup-', dir=args.evidence)).resolve()
    programs = {n: getattr(args, n).resolve() for n in ('fixture', 'receiver', 'service')}
    maximum = 4 * 1024**2
    assert maximum <= 20 * 1024**3
    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    process = None
    results, deleted = [], []
    try:
        for scenario in ('failed-cleanup-sealed-0', 'failed-cleanup-sealed-1'):
            case = folder / scenario
            command = [str(programs['fixture']), str(root / 'tests/data/cobo-hardware.json'),
                       str(root / 'tests/data/mutant-hardware.json'), str(programs['receiver']),
                       str(programs['service']), str(folder / (scenario + '-audits.jsonl')),
                       '--scenario', scenario, str(case)]
            try:
                with (folder / (scenario + '.log')).open('w') as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    code = process.wait(timeout=180)
                results.append({'scenario': scenario, 'exit_code': code})
                assert code == 0, (folder / (scenario + '.log')).read_text()[-4000:]
                proof = json.loads((case / 'sealed-cleanup-proof.json').read_text())
                assert proof['passed'] and proof['sealed_node_cleanup_posts'] == 0
                assert proof['failed_node_cleanup_posts'] == 1 and proof['sealed_bytes_preserved']
                assert not proof['ecc_cleanup_entry_exercised']
            finally:
                stop_owned_group(process)
                process = None
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            if path.is_file() and not path.is_symlink():
                deleted.append({'path': str(path.relative_to(folder)), 'bytes': path.stat().st_size, 'sha256': sha(path)})
                path.unlink()
        (folder / 'payload-cleanup.json').write_text(json.dumps({'deleted': deleted, 'remaining_raw': len(list(folder.rglob('*.graw')))}, indent=2) + '\n')
        (folder / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    assert sum(r['bytes'] for r in deleted) <= maximum and not list(folder.rglob('*.graw'))
    (folder / 'scope.json').write_text(json.dumps({'passed': True, 'cases': 2,
        'maximum_aggregate_test_bytes': maximum, 'raw_deleted': True,
        'real_hardware': False, 'ecc_cleanup_entry_exercised': False,
        'programs': {n: {'path': str(p), 'sha256': sha(p)} for n, p in programs.items()}}, indent=2) + '\n')
    print('PASS two remote-manager sealed-peer cleanup cases; raw removed;', folder)


if __name__ == '__main__':
    main()
