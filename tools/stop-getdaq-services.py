#!/usr/bin/env python3
"""Stop only this user's services from one GetDAQ installation."""
import argparse
from pathlib import Path
import os
import signal
import time

NAMES = {'ecc': {'getdaq-ecc-server'},
         'data': {'getdaq-supervisor', 'getdaq-receiver', 'getdaq-ice-receiver'},
         'all': {'getdaq-ecc-server', 'getdaq-supervisor', 'getdaq-receiver', 'getdaq-ice-receiver'}}


def identity(pid, root, names):
    try:
        proc = Path('/proc') / str(pid)
        exe = (proc / 'exe').resolve(strict=True)
        fields = (proc / 'stat').read_text().rsplit(')', 1)[1].split()
        if proc.stat().st_uid != os.getuid() or root not in exe.parents or exe.name not in names or fields[0] == 'Z':
            return None
        return (str(exe), fields[19])
    except (OSError, ValueError, IndexError):
        return None


def stop(root, names, force=False, assume_yes=False, listing=False):
    found = {}
    for proc in Path('/proc').iterdir():
        if proc.name.isdigit():
            pid = int(proc.name)
            if pid > 1:
                current = identity(pid, root, names)
                if current:
                    found[pid] = current
    if not found:
        print('No matching services are running in ' + str(root))
        return 0
    for pid, current in found.items():
        print('PID %s  %s' % (pid, current[0]))
    if listing:
        return 0
    print('These are service processes. Finish acquisition with Run stop first; this does not stop the board or complete a Run.')
    if not assume_yes:
        question = 'Stop these processes%s? [y/N] ' % (' (force termination permitted after 5 seconds)' if force else '')
        if input(question).strip().lower() not in ('y', 'yes'):
            return 1
    for pid, current in found.items():
        if identity(pid, root, names) == current:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline and any(identity(pid, root, names) == current for pid, current in found.items()):
        time.sleep(.1)
    remaining = {pid: current for pid, current in found.items() if identity(pid, root, names) == current}
    if remaining and force:
        for pid, current in remaining.items():
            if identity(pid, root, names) == current:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline and any(identity(pid, root, names) == current for pid, current in remaining.items()):
            time.sleep(.1)
        remaining = {pid: current for pid, current in remaining.items() if identity(pid, root, names) == current}
    if remaining:
        print('Still running: %s. Inspect these processes; --force explicitly permits SIGKILL.' % ', '.join(map(str, remaining)))
        return 2
    print('Selected services exited. Existing records and data were retained.')
    return 0


if __name__ == '__main__':
    parent = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('service', choices=sorted(NAMES), nargs='?', default='all')
    parser.add_argument('--root', type=Path, default=parent.parent if parent.name == 'tools' else parent)
    parser.add_argument('--list', action='store_true', help='show processes without stopping')
    parser.add_argument('--force', action='store_true', help='permit SIGKILL if SIGTERM does not finish within five seconds')
    parser.add_argument('--yes', action='store_true', help='confirm the displayed scoped stop')
    options = parser.parse_args()
    raise SystemExit(stop(options.root.resolve(), NAMES[options.service], options.force, options.yes, options.list))
