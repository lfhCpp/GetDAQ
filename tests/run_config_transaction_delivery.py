#!/usr/bin/env python3
"""Exercise packaged configuration CLI against real concurrent Save All processes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    parser.add_argument('package', type=Path)
    parser.add_argument('evidence', type=Path)
    parser.add_argument('--windows', action='store_true')
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='delivery-', dir=args.evidence)).resolve()
    package = args.package.resolve(strict=True)
    suffix = '.exe' if args.windows else ''
    cli = package / ('' if args.windows else 'bin') / ('getdaq-config' + suffix)
    checker = cli.with_name('getdaq-config-check' + suffix)
    probe = args.probe.resolve(strict=True)
    programs = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (probe, cli, checker)}
    environment = dict(os.environ, XDG_CACHE_HOME=str(evidence / 'cache'))
    children, runs = [], []
    native_owned = None

    def native(path):
        value = str(path)
        if not args.windows or (len(value) > 1 and value[1] == ':') or value.startswith('\\\\'):
            return value
        return subprocess.check_output(['wslpath', '-w', value], text=True).strip()

    def call(argv, expected=0):
        result = subprocess.run([str(x) for x in argv], env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
        runs.append({'arguments': [str(x) for x in argv], 'exit_code': result.returncode,
                     'output': result.stdout})
        assert result.returncode == expected, result.stdout
        return result.stdout

    def start(mode, directory, name):
        gate = evidence / (name + '.gate')
        gate.write_text('owned configuration test gate\n')
        with (evidence / (name + '.log')).open('w') as log:
            child = subprocess.Popen([str(probe), mode, native(directory), native(gate)],
                                     env=environment, stdout=log, stderr=subprocess.STDOUT)
        children.append(child)
        ready = Path(str(gate) + '.ready')
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if ready.exists():
                try:
                    return child, gate, json.loads(ready.read_text())
                except (ValueError, OSError):
                    pass
            assert child.poll() is None, (evidence / (name + '.log')).read_text()
            time.sleep(.01)
        raise AssertionError('Owned writer did not reach the first synced source')

    try:
        source_parent = evidence
        if args.windows:
            temporary = call([probe, 'temp', 'unused']).strip()
            native_owned = Path(subprocess.check_output(['wslpath', '-u', temporary], text=True).strip())
            assert native_owned.is_dir() and native_owned.name.startswith('getdaq-config-transactions-')
            source_parent = native_owned
        for label, mode, expected_exit in [('active', 'hold-write', 0), ('crashed', 'crash-write', 77)]:
            directory = source_parent / label
            directory.mkdir()
            (directory / 'root.xcfg').write_text('<Setup><A include="a.xcfg"/><B include="b.xcfg"/></Setup>')
            (directory / 'a.xcfg').write_text('<A><n type="int">1</n></A>')
            (directory / 'b.xcfg').write_text('<B><n type="int">1</n></B>')
            before = directory / 'before.json'
            call([cli, 'snapshot', native(directory / 'root.xcfg'), native(before)])
            assert json.loads(call([checker, 'snapshot', native(before)]))['valid'] is True
            child, gate, ready = start(mode, directory, label)
            assert '>2<' in (directory / 'a.xcfg').read_text() and '>1<' in (directory / 'b.xcfg').read_text()
            blocked = directory / 'must-not-exist.json'
            if mode == 'crash-write':
                assert child.wait(timeout=15) == expected_exit
            message = call([cli, 'snapshot', native(directory / 'root.xcfg'), native(blocked)], 1)
            assert ('transaction busy' if mode == 'hold-write' else 'unfinished Save All') in message
            assert not blocked.exists()
            if mode == 'hold-write':
                gate.unlink()
                assert child.wait(timeout=15) == expected_exit
            else:
                call([probe, 'recover', native(directory), ready['recovery']])
                gate.unlink()
            assert '>2<' in (directory / 'a.xcfg').read_text() and '>2<' in (directory / 'b.xcfg').read_text()
            after = directory / 'after.json'
            call([cli, 'snapshot', native(directory / 'root.xcfg'), native(after)])
            assert json.loads(call([checker, 'snapshot', native(after)]))['valid'] is True
        if native_owned is not None:
            shutil.copytree(native_owned, evidence / 'native-source-evidence')
        assert not list(source_parent.rglob('*.graw'))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest for p, digest in programs.items())
        result = {'passed': True, 'executed_from_package_paths': True, 'actual_windows_runtime': args.windows,
                  'program_sha256': programs, 'active_writer_refused': True, 'pending_crash_refused': True,
                  'explicit_recovery_then_snapshot_validated': True, 'raw_files_created': 0, 'runs': runs}
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print('PASS packaged CLI source transactions:', evidence)
    finally:
        for child in children:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=3)
        if native_owned is not None:
            shutil.rmtree(native_owned)


if __name__ == '__main__':
    main()
