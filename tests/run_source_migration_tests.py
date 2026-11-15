#!/usr/bin/env python3
"""Kill only owned migration fixtures after real source synchronization, then recover their exact records."""
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
    parser.add_argument('evidence', type=Path)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    assert not (args.windows and args.sanitized)
    args.evidence.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='migration-', dir=args.evidence)).resolve()
    probe = args.probe.resolve(strict=True)
    digest = hashlib.sha256(probe.read_bytes()).hexdigest()
    env = dict(os.environ, XDG_CACHE_HOME=str(evidence / 'cache'))
    if args.sanitized:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        linked = subprocess.check_output(['ldd', str(probe)], text=True)
        assert 'libasan' in linked and 'libubsan' in linked
    runs, children, scenarios = [], [], []
    native_owned = None

    def native(value):
        value = str(value)
        if not args.windows or (len(value) > 1 and value[1] == ':') or value.startswith('\\\\'):
            return value
        return subprocess.check_output(['wslpath', '-w', value], text=True).strip()

    def call(mode, *arguments, expected=0):
        # Mode/direction are words, filesystem arguments receive native spelling.
        command = [str(probe), mode] + [native(x) if isinstance(x, Path) else str(x) for x in arguments]
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
        runs.append({'mode': mode, 'arguments': command[2:], 'exit_code': result.returncode, 'output': result.stdout})
        assert result.returncode == expected, result.stdout
        return result.stdout

    def setup(name):
        directory = source_parent / name
        (directory / 'new').mkdir(parents=True)
        originals = {'root.xcfg': b'<Setup><A include="part.xcfg"/></Setup>',
                     'part.xcfg': b'<A><n type="int">2</n></A>',
                     'wrapper.xcfg': b'<Wrapper><view include="root.xcfg"/></Wrapper>'}
        for name, content in originals.items():
            (directory / name).write_bytes(content)
        return directory, originals

    def start(directory, mode, source, phase):
        with (directory / 'writer.log').open('w') as log:
            child = subprocess.Popen([str(probe), mode, native(directory), source, phase], env=env, stdout=log, stderr=subprocess.STDOUT)
        children.append(child)
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if (directory / 'progress.json').exists():
                return child, json.loads((directory / 'progress.json').read_text())
            assert child.poll() is None, (directory / 'writer.log').read_text()
            time.sleep(.01)
        raise AssertionError('Owned migration child did not reach its synchronization point')

    def verify(directory, originals, source, candidate):
        expected = dict(originals)
        if candidate:
            if source == 'root':
                del expected['root.xcfg']
                expected['new/main.xcfg'] = b'<Setup><A include="../part.xcfg"/></Setup>'
                expected['wrapper.xcfg'] = b'<Wrapper><view include="new/main.xcfg"/></Wrapper>'
            else:
                del expected['part.xcfg']
                expected['new/part.xcfg'] = originals['part.xcfg']
                expected['root.xcfg'] = b'<Setup><A include="new/part.xcfg"/></Setup>'
        for name in set(originals) | {'new/main.xcfg', 'new/part.xcfg'}:
            path = directory / name
            if name in expected:
                assert path.read_bytes() == expected[name], name
            else:
                assert not path.exists(), name
        call('read', directory / ('new/main.xcfg' if candidate and source == 'root' else 'root.xcfg'))
        call('read', directory / 'wrapper.xcfg')

    try:
        source_parent = evidence
        if args.windows:
            path = call('temp', 'unused').strip()
            native_owned = Path(subprocess.check_output(['wslpath', '-u', path], text=True).strip())
            assert native_owned.is_dir() and native_owned.name.startswith('getdaq-source-migration-')
            source_parent = native_owned
        for source in ['root', 'part']:
            for phase in ['created', 'replaced', 'removed']:
                for direction in ['original', 'candidate']:
                    name = f'{source}-{phase}-{direction}'
                    directory, originals = setup(name)
                    child, progress = start(directory, 'crash', source, phase)
                    assert child.wait(timeout=15) == 79 and progress['phase'] == phase
                    message = call('read', directory / 'root.xcfg', expected=3)
                    assert 'unfinished Save All' in message
                    call('recover', progress['recovery'], direction)
                    verify(directory, originals, source, direction == 'candidate')
                    call('recover', progress['recovery'], direction, expected=4)
                    scenarios.append({'name': name, 'crash_exit': 79, 'recovered_direction': direction})
            directory, originals = setup(source + '-active-writer')
            gate = directory / 'hold.gate'
            gate.write_text('owned gate\n')
            child, progress = start(directory, 'hold', source, 'created')
            assert child.poll() is None
            assert 'transaction busy' in call('read', directory / 'root.xcfg', expected=3)
            gate.unlink()
            assert child.wait(timeout=15) == 0
            verify(directory, originals, source, True)
        assert not list(source_parent.rglob('*.graw'))
        if native_owned is not None:
            shutil.copytree(native_owned, evidence / 'native-source-evidence')
        if args.sanitized:
            texts = [r['output'] for r in runs] + [p.read_text(errors='replace') for p in source_parent.rglob('writer.log')]
            assert not any(marker in text for text in texts for marker in ['ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'AddressSanitizer:DEADLYSIGNAL', 'runtime error:', 'LeakSanitizer has encountered a fatal error'])
        assert hashlib.sha256(probe.read_bytes()).hexdigest() == digest
        (evidence / 'result.json').write_text(json.dumps({'passed': True, 'probe_sha256': digest,
            'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'actual_windows_runtime': args.windows,
            'sanitized': args.sanitized, 'abrupt_child_lsan_verified': False, 'scenarios': scenarios,
            'active_writer_cases': 2, 'raw_files_created': 0, 'runs': runs}, indent=2) + '\n')
        print('PASS source migration processes, 12 crash/recovery and two active-writer cases:', evidence)
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
