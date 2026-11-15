#!/usr/bin/env python3
"""Archive the reviewed configuration structure editor; preserve previous evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target, current=None):
    files = json.loads((target / 'files-sha256.json').read_text())
    actual = {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
    assert actual == set(files) | {'files-sha256.json'}
    for name, digest in files.items(): assert sha(target / name) == digest, name
    inputs = json.loads((target / 'input-sha256.json').read_text())
    if current:
        for name, digest in inputs.items(): assert sha(current / name) == digest, name
    for label, sanitized, count in [('model', False, 29), ('model-sanitized', True, 29),
                                    ('gui', False, 64), ('console', False, None), ('cli', False, None)]:
        folder = target / 'builds' / label
        receipt = json.loads((folder / 'build-receipt.json').read_text())
        assert receipt['sanitized'] == sanitized and not receipt['system_qt_instrumented']
        assert sha(folder / 'program') == receipt['program_sha256']
        for name, digest in json.loads((folder / 'source-sha256.json').read_text()).items(): assert inputs[name] == digest
        for name, digest in json.loads((folder / 'generated-sha256.json').read_text()).items(): assert sha(folder / 'generated' / name) == digest
        if count:
            result = (folder / 'results.txt').read_text()
            assert f'Totals: {count} passed, 0 failed, 0 skipped' in result, label
        if sanitized:
            recipe = (folder / 'Makefile').read_text()
            assert all(flag in recipe for flag in ('-std=c++11', '-pedantic-errors', '-fsanitize=address,undefined'))
    assert not any(p.suffix == '.graw' or 'credential' in p.name.lower() for p in target.rglob('*'))
    print('PASS configuration-structure archive:', len(files), 'files;', len(inputs),
          'inputs; historical only', current is None)


def main():
    root = Path(__file__).resolve().parents[1]
    target = root / 'build-xcfg-structure/validation-structure'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create', action='store_true')
    parser.add_argument('--historical-only', action='store_true')
    args = parser.parse_args()
    if not args.create:
        verify(target, None if args.historical_only else root)
        return
    assert not target.exists() and not args.historical_only
    copies, inputs = {}, {}

    def add(path, name):
        assert path.is_file() and name not in copies and '..' not in Path(name).parts
        copies[name] = path

    builds = [('model', root / 'build-xcfg-structure', 'results-structure-current.txt'),
              ('model-sanitized', Path('/tmp/getdaq-xcfg-structure-asan'), 'results-structure-current.txt'),
              ('gui', root / 'build-suite-tests', 'results-structure-gui-current.txt'),
              ('console', root / 'build-suite/apps/envoy', None),
              ('cli', root / 'build-suite/apps/configtool', None)]
    for label, build, result in builds:
        receipt = json.loads((build / 'build-receipt.json').read_text())
        assert sha(build / receipt['target']) == receipt['program_sha256']
        for name, digest in json.loads((build / 'source-sha256.json').read_text()).items():
            assert sha(root / name) == digest, name
            assert name not in inputs or inputs[name] == digest, name
            inputs[name] = digest
        add(build / receipt['target'], 'builds/' + label + '/program')
        for name in ['build-receipt.json', 'source-sha256.json', 'generated-sha256.json', 'Makefile', 'results-verified-build.txt']:
            add(build / name, 'builds/' + label + '/' + name)
        for name, digest in json.loads((build / 'generated-sha256.json').read_text()).items():
            assert sha(build / name) == digest
            add(build / name, 'builds/' + label + '/generated/' + name)
        if result: add(build / result, 'builds/' + label + '/results.txt')
    for name in ['results-structure-snapshot-cli.txt', 'results-structure-system-cli.txt']:
        add(root / 'build-xcfg-structure' / name, 'results/' + name)
        assert 'PASS' in (root / 'build-xcfg-structure' / name).read_text()
    add(root / 'build-ice-compat/getdaq-system-config', 'existing-native/getdaq-system-config')
    for path in (root / 'build-xcfg-structure/snapshot-cli-final-evidence').rglob('*'):
        if path.is_file(): add(path, 'snapshot-cli/' + str(path.relative_to(root / 'build-xcfg-structure/snapshot-cli-final-evidence')))
    references = ['source/cconfig/CompoundConfig-release-20210208/src/CCfg/Object.cpp',
                  'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/ConfigModelAccessor.cpp',
                  'source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Model/Command/InsertConfigNode.cpp']
    for name in references:
        inputs[name] = sha(root / name)
        add(root / name, 'legacy-reference/' + name)
    for name in ['tests/archive_configuration_structure_validation.py', 'tests/effective_snapshot_integration.py',
                 'tests/system_experiment_integration.py', 'docs/CONFIGURATION_STRUCTURE_EDITOR.md',
                 'docs/CONFIGURATION_EDITOR.md', 'docs/ARCHITECTURE.md', 'docs/REFACTOR_STATUS.md', 'docs/VALIDATION.md', 'README.md']:
        inputs[name] = sha(root / name)
    for name in inputs: add(root / name, 'verified-sources/' + name)
    target.mkdir(parents=True)
    for name, source in copies.items():
        destination = target / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    (target / 'input-sha256.json').write_text(json.dumps(inputs, indent=2) + '\n')
    manifest = {str(path.relative_to(target)): sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target / 'files-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    verify(target, root)


if __name__ == '__main__': main()
