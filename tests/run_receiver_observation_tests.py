#!/usr/bin/env python3
"""Run real native/HTTP receiver observation checks; delete all owned raw payload."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda: file.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def interrupted(signum, unused):
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    raise KeyboardInterrupt('receiver observation test interrupted')


def stop_owned_group(process):
    if process is None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    if process.poll() is None:
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            pass
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=3)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('receiver', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args(); build = args.build.resolve(); receiver = args.receiver.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='receiver-observation-', dir=args.evidence))
    signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
    # Each runner has <= 29 cases; generous whole-group bound includes FDT controls.
    maximum_wire_bytes = 2*29*2*2*12*4104
    assert maximum_wire_bytes < 20*1024**3
    commands = [
        ['python3', str(root/'tests/run_file_naming_supervisor_tests.py'), str(build/'data-supervisor-tests'), str(receiver), str(folder/'native')],
        ['python3', str(root/'tests/run_supervisor_control_tests.py'), str(build/'getdaq-supervisor'), str(build/'supervisor-control-tests'), str(receiver), '--output-parent', str(folder/'http'), '--jobs', '1']]
    runs = []; deleted = []; process = None
    try:
        for index, command in enumerate(commands):
            try:
                with (folder/('runner-'+str(index)+'.log')).open('w') as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    code = process.wait(timeout=240)
                assert code == 0, (folder/('runner-'+str(index)+'.log')).read_text()
                runs.append({'command': command, 'exit_code': code})
            finally:
                stop_owned_group(process); process = None
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            deleted.append({'path': str(path.relative_to(folder)), 'bytes': path.stat().st_size, 'sha256': sha(path)})
            path.unlink()
        cleanup = {'deleted': deleted, 'raw_files_remaining': len(list(folder.rglob('*.graw')))}
        (folder/'payload-cleanup.json').write_text(json.dumps(cleanup, indent=2)+'\n')
    receipt = {'schema': 1, 'runs': runs, 'maximum_wire_bytes': maximum_wire_bytes, 'aggregate_limit_bytes': 20*1024**3,
               'raw_files_remaining': cleanup['raw_files_remaining'], 'real_hardware': False, 'throughput_benchmark': False,
               'program_sha256': {name: sha(build/name) for name in ('data-supervisor-tests','supervisor-control-tests','getdaq-supervisor','getdaq-supervisor-query')},
               'receiver_sha256': sha(receiver), 'runner_sha256': sha(Path(__file__))}
    (folder/'result.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('PASS native/HTTP owned process observations and lifecycle regression; raw payload deleted;', folder)


if __name__ == '__main__':
    main()
