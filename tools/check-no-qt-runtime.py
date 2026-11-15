#!/usr/bin/env python3
"""Fail when a native DAQ data-path executable dynamically depends on Qt."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def runtime_dependencies(binary):
    if sys.platform.startswith('linux'):
        command = ['ldd', str(binary)]
    elif sys.platform == 'darwin':
        command = ['otool', '-L', str(binary)]
    else:
        raise RuntimeError('Unsupported platform for Qt runtime check: ' + sys.platform)
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, check=False)
    if result.returncode:
        raise RuntimeError('%s failed for %s:\n%s' %
                           (command[0], binary, result.stdout))
    dependencies = []
    for line in result.stdout.splitlines()[1:]:
        item = line.strip().split(' (', 1)[0].split(' => ', 1)[0].strip()
        if not item:
            continue
        dependencies.append(item)
    return dependencies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', action='append', required=True,
                        help='native data-path executable to inspect; repeat as needed')
    args = parser.parse_args()
    has_qt = re.compile(r'^(?:lib)?Qt[0-9A-Za-z_]*$', re.IGNORECASE)
    checked = []
    violations = []
    for value in args.binary:
        binary = Path(value).resolve(strict=True)
        deps = runtime_dependencies(binary)
        checked.append(str(binary))
        for dependency in deps:
            name = os.path.basename(dependency.rstrip('/'))
            name = name.split('.so', 1)[0].split('.dylib', 1)[0]
            if has_qt.match(name):
                violations.append((binary, dependency))
    if violations:
        for binary, dependency in violations:
            print('FAIL: Qt runtime dependency: %s -> %s' % (binary, dependency), file=sys.stderr)
        return 1
    for binary in checked:
        print('PASS: no Qt runtime dependency: ' + binary)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit('no-Qt runtime check failed: ' + str(error))
