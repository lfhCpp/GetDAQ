#!/usr/bin/env python3
"""Bounded native transfer/real-receiver tests; private roots and catchable-interrupt cleanup."""
import argparse
import json
import os
import signal
import subprocess
import tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture', 'catalog', 'regression', 'lifecycle', 'receiver', 'evidence'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='catalog-transfer-', dir=args.evidence)).resolve()
    shared = Path(tempfile.mkdtemp(prefix='getdaq-transfer-', dir='/dev/shm'))
    temporary = folder / 'temporary'
    temporary.mkdir(mode=0o700)
    # Existing hardlink regression includes two sparse 512 MiB sources and their
    # hardlink names. Conservatively count every input/output alias and wire copy.
    maximum = 4 * 1024**3
    assert maximum <= 20 * 1024**3
    env = dict(os.environ, GETDAQ_CATALOG_TEST_ROOT=str(temporary),
               GETDAQ_TRANSFER_TEST_SHM_ROOT=str(shared))
    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    programs = {n: getattr(args, n).resolve() for n in ('fixture', 'catalog', 'regression', 'lifecycle', 'receiver')}
    commands = [
        ('copy', [str(programs['fixture']), str(programs['catalog'])]),
        ('hardlink-regression', [str(programs['regression'])]),
        ('real-receivers', [str(programs['lifecycle']), str(programs['receiver']), str(folder / 'real-receivers'), 'catalog-copy']),
    ]
    process = None
    rows, deleted = [], []
    try:
        for name, command in commands:
            try:
                with (folder / (name + '.log')).open('w') as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                               env=env, start_new_session=True)
                    code = process.wait(timeout=180)
                rows.append({'name': name, 'exit_code': code})
                assert code == 0, (folder / (name + '.log')).read_text()[-4000:]
            finally:
                stop_owned_group(process)
                process = None
        proof = json.loads((folder / 'real-receivers/fixture-result.json').read_text())
        assert proof['passed'] and proof['actual_receiver_processes']
        receipt = json.loads((folder / 'real-receivers/transfer-result.json').read_text())
        assert receipt['complete'] and receipt['content_verified'] and receipt['source_preserved']
    finally:
        stop_owned_group(process)
        for parent in (folder, shared):
            for path in parent.rglob('*.graw'):
                if path.is_file() and not path.is_symlink():
                    # Native fixtures already remove their RAII directories. Only
                    # leftover live-process evidence belongs to this wrapper.
                    deleted.append({'root': str(parent), 'path': str(path.relative_to(parent)),
                                    'bytes': path.stat().st_size, 'sha256': sha(path)})
                    path.unlink()
        (folder / 'payload-cleanup.json').write_text(json.dumps({'deleted': deleted,
            'remaining_raw': [str(p) for parent in (folder, shared) for p in parent.rglob('*.graw')],
            'native_raii_fixtures_clean_their_raw': True}, indent=2) + '\n')
        (folder / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        # Remove only empty directories in the uniquely created shm test root.
        for path in sorted(shared.rglob('*'), key=lambda p: len(p.parts), reverse=True):
            if path.is_dir() and not path.is_symlink():
                try:
                    path.rmdir()
                except OSError:
                    pass
        try:
            shared.rmdir()
        except OSError:
            pass
    assert sum(r['bytes'] for r in deleted) <= maximum
    assert not list(folder.rglob('*.graw')) and not list(shared.rglob('*.graw'))
    (folder / 'scope.json').write_text(json.dumps({'passed': True, 'maximum_aggregate_test_bytes': maximum,
        'aggregate_limit_bytes': 20 * 1024**3, 'raw_deleted': True,
        'real_hardware': False, 'throughput_benchmark': False,
        'programs': {n: {'path': str(p), 'sha256': sha(p)} for n, p in programs.items()}}, indent=2) + '\n')
    print('PASS native catalog copy, hardlink regression, two real receivers; owned raw removed;', folder)


if __name__ == '__main__':
    main()
