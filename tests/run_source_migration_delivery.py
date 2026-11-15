#!/usr/bin/env python3
"""Validate actual packaged migration/snapshot CLIs; tiny owned XML only."""
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
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('probe', type=Path)
    ap.add_argument('package', type=Path)
    ap.add_argument('evidence', type=Path)
    ap.add_argument('--windows', action='store_true')
    args = ap.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='delivery-', dir=args.evidence)).resolve()
    package = args.package.resolve(strict=True)
    suffix = '.exe' if args.windows else ''
    cli = package / ('' if args.windows else 'bin') / ('getdaq-config' + suffix)
    checker = cli.with_name('getdaq-config-check' + suffix)
    probe = args.probe.resolve(strict=True)
    programs = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (probe, cli, checker)}
    env = dict(os.environ, XDG_CACHE_HOME=str(evidence / 'cache'))
    runs, children = [], []
    native_owned = None

    def native(value):
        value = str(value)
        if not args.windows or (len(value) > 1 and value[1] == ':') or value.startswith('\\\\'):
            return value
        return subprocess.check_output(['wslpath', '-w', value], text=True).strip()

    def host_path(value):
        """Normalize paths reported by Qt back to the runner's path namespace."""
        if not args.windows or os.name == 'nt':
            return str(Path(value).resolve())
        windows_value = value.replace('/', '\\')
        host_value = subprocess.check_output(['wslpath', '-u', windows_value], text=True).strip()
        return str(Path(host_value).resolve())

    def call(executable, *arguments, expected=0):
        command = [str(executable)] + [native(x) if isinstance(x, Path) else str(x) for x in arguments]
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
        runs.append({'command': command, 'exit_code': result.returncode, 'output': result.stdout})
        assert result.returncode == expected, result.stdout
        return result.stdout

    def setup(name):
        directory = source_parent / name
        (directory / 'new').mkdir(parents=True)
        files = {'root.xcfg': b'<Setup><A include="part.xcfg"/></Setup>',
                 'part.xcfg': b'<A><n type="int">2</n></A>',
                 'wrapper.xcfg': b'<Wrapper><view include="root.xcfg"/></Wrapper>'}
        for name, data in files.items():
            (directory / name).write_bytes(data)
        return directory, files

    def snapshot(path, output):
        call(cli, 'snapshot', path, output)
        assert json.loads(call(checker, 'snapshot', output))['valid']

    def crash(directory):
        with (directory / 'writer.log').open('w') as log:
            child = subprocess.Popen([str(probe), 'crash', native(directory), 'root', 'removed'], env=env, stdout=log, stderr=subprocess.STDOUT)
        children.append(child)
        assert child.wait(timeout=20) == 79
        progress = json.loads((directory / 'progress.json').read_text())
        assert progress['phase'] == 'removed'
        return progress['recovery']

    try:
        source_parent = evidence
        if args.windows:
            temporary = call(probe, 'temp', 'unused').strip()
            native_owned = Path(subprocess.check_output(['wslpath', '-u', temporary], text=True).strip())
            assert native_owned.is_dir() and native_owned.name.startswith('getdaq-source-migration-')
            source_parent = native_owned
        directory, files = setup('normal')
        old, target, root, wrapper = directory / 'part.xcfg', directory / 'new/part.xcfg', directory / 'root.xcfg', directory / 'wrapper.xcfg'
        discovery = json.loads(call(cli, 'discover-project-referrers', directory, old))
        assert discovery['complete'] and discovery['read_only'] and not discovery['disk_written']
        assert [host_path(path) for path in discovery['referrers']] == [str(root.resolve()), str(wrapper.resolve())]
        assert [host_path(path) for path in discovery['candidate_roots']] == [str(wrapper.resolve())]
        review = json.loads(call(cli, 'preview-path-migration', old, target, root, wrapper))
        assert review['disk_written'] is False and not target.exists()
        assert all((directory / name).read_bytes() == data for name, data in files.items())
        result = json.loads(call(cli, 'migrate-path', old, target, root, wrapper))
        assert result['complete'] and not old.exists() and target.read_bytes() == files['part.xcfg']
        assert root.read_bytes() == b'<Setup><A include="new/part.xcfg"/></Setup>'
        snapshot(root, directory / 'after.json')
        snapshot(wrapper, directory / 'wrapper-after.json')
        for direction in ['candidate', 'original']:
            directory, files = setup('recover-' + direction)
            record = crash(directory)
            blocked = directory / 'must-not-exist.json'
            assert 'unfinished Save All' in call(cli, 'snapshot', directory / 'root.xcfg', blocked, expected=1)
            assert not blocked.exists()
            review = json.loads(call(cli, 'preview-path-recovery', record, direction))
            assert review['disk_written'] is False
            result = json.loads(call(cli, 'recover-path', record, direction))
            assert result['disk_written'] is True
            if direction == 'original':
                assert all((directory / name).read_bytes() == data for name, data in files.items())
                assert not (directory / 'new/main.xcfg').exists()
                root = directory / 'root.xcfg'
            else:
                assert not (directory / 'root.xcfg').exists()
                root = directory / 'new/main.xcfg'
                assert root.read_bytes() == b'<Setup><A include="../part.xcfg"/></Setup>'
                assert (directory / 'wrapper.xcfg').read_bytes() == b'<Wrapper><view include="new/main.xcfg"/></Wrapper>'
            snapshot(root, directory / 'recovered.json')
            snapshot(directory / 'wrapper.xcfg', directory / 'wrapper-recovered.json')
            assert 'already completed' in call(cli, 'recover-path', record, direction, expected=1)
        incomplete = evidence / 'incomplete-scan'
        incomplete.mkdir();(incomplete/'root.xcfg').write_text('<Setup><Other include="/outside/unknown.xcfg"/></Setup>')
        scan_source=source_parent/'recover-original/part.xcfg'
        out = subprocess.run([str(cli),'discover-project-referrers',native(incomplete),native(scan_source)],
            env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
        runs.append({'command':[str(cli),'discover-project-referrers',native(incomplete),native(scan_source)],
            'exit_code':out.returncode,'output':out.stdout})
        assert out.returncode==3 and not json.loads(out.stdout)['complete']
        assert not list(source_parent.rglob('*.graw'))
        if native_owned is not None:
            shutil.copytree(native_owned, evidence / 'native-source-evidence')
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in programs.items())
        (evidence / 'result.json').write_text(json.dumps({'passed': True, 'actual_windows_runtime': args.windows,
            'executed_from_package_paths': True, 'program_sha256': programs, 'preview_did_not_write_sources': True,
            'actual_migration_cli_checked': True, 'both_recovery_directions_checked': True,
            'snapshot_checked_by_packaged_native_tool': True, 'raw_files_created': 0, 'runs': runs}, indent=2) + '\n')
        print('PASS packaged source migration, both recovery directions and checked snapshots:', evidence)
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
