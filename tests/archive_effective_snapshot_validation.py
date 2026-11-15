#!/usr/bin/env python3
"""保存有效配置快照阶段的同版本机证据；不覆盖既有归档。"""
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    target = root / 'build-xcfg-shared/validation-effective-snapshot'
    assert not target.exists(), 'Keep existing evidence immutable'
    builds = [root / 'build-xcfg-shared', Path('/tmp/getdaq-xcfg-shared-asan'), root / 'build-suite-tests']
    inputs = {}
    copies = {}

    def add(source, destination):
        assert source.is_file(), source
        assert destination not in copies, destination
        copies[destination] = source

    for index, build in enumerate(builds):
        receipt = json.loads((build / 'build-receipt.json').read_text())
        assert sha(build / receipt['target']) == receipt['program_sha256'], build
        assert receipt['sanitized'] == (index == 1) and not receipt['system_qt_instrumented']
        for name, digest in json.loads((build / 'source-sha256.json').read_text()).items():
            assert sha(root / name) == digest, name
            assert name not in inputs or inputs[name] == digest, name
            inputs[name] = digest
        for name, digest in json.loads((build / 'generated-sha256.json').read_text()).items():
            assert sha(build / name) == digest, name
            add(build / name, f'qt-build/{index}/{name}')
        expected = 58 if index == 2 else 18
        result = build / 'results-effective-snapshot-tests-current.txt'
        assert f'Totals: {expected} passed, 0 failed, 0 skipped' in result.read_text(), result
        for name in ['build-receipt.json', 'source-sha256.json', 'generated-sha256.json', 'Makefile', 'results-verified-build.txt', result.name, receipt['target']]:
            add(build / name, f'qt-build/{index}/{name}')

    documents = ['README.md', 'docs/ARCHITECTURE.md', 'docs/CONFIGURATION_EDITOR.md', 'docs/CONFIGURATION_SNAPSHOTS.md',
                 'docs/EFFECTIVE_CONFIGURATION_SNAPSHOTS.md', 'docs/REFACTOR_STATUS.md', 'docs/VALIDATION.md']
    extras = documents + ['tests/effective_snapshot_integration.py', 'tests/archive_effective_snapshot_validation.py',
                         'tests/system_experiment_integration.py', 'tests/aget_compiler_integration.py',
                         'tests/cobo_readout_integration.py', 'tests/mutant_compiler_integration.py']
    for name in extras:
        inputs[name] = sha(root / name)
    for name in sorted(inputs):
        add(root / name, 'verified-sources/' + name)
    for name in ['getdaq-config', 'GetDAQ', 'getdaq-sentry', 'getdaq-simulator']:
        program = root / 'build-suite/bin' / name
        add(program, 'programs/' + name)
    add(root / 'build-ice-compat/getdaq-system-config', 'programs/getdaq-system-config')
    add(root / 'build-experiment/system-experiment-tests', 'programs/system-experiment-tests')
    for folder, names in {
        'build-suite-tests': ['results-effective-snapshot-cli-current.txt', 'results-effective-snapshot-system-cli-current.txt',
                              'results-effective-snapshot-aget-cli-current.txt', 'results-effective-snapshot-mutant-cli-current.txt',
                              'results-effective-snapshot-readout-cli-current.txt'],
        'build-suite': ['results-effective-snapshot-unified-build-final.txt'],
        'build-experiment': ['results-effective-snapshot-parser-build-current.txt', 'results-effective-snapshot-parser-tests-current.txt'],
        'build-ice-compat': ['results-effective-snapshot-system-loader-build-current.txt'],
    }.items():
        for name in names:
            add(root / folder / name, 'results/' + name)
    evidence = root / 'build-suite-tests/effective-snapshot-cli-current-evidence'
    scopes = list(evidence.glob('effective-snapshot-*/scope.json'))
    assert len(scopes) == 1, scopes
    scope = json.loads(scopes[0].read_text())
    assert scope['offline_only'] and not scope['hardware_fixture'] and not scope['raw_data_transfer'] and scope['process_cases'] == 12
    assert scope['program_sha256'] == sha(root / 'build-suite/bin/getdaq-config')
    for source in sorted(scopes[0].parent.iterdir()):
        add(source, 'cli-evidence/' + source.name)
    assert '1/1 Test #1: system-experiment' in (root / 'build-experiment/results-effective-snapshot-parser-tests-current.txt').read_text()
    for name, needle in [('system', '36 process cases'), ('aget', 'PASS:'), ('mutant', '50 contracts'), ('readout', 'PASS:')]:
        assert needle in (root / f'build-suite-tests/results-effective-snapshot-{name}-cli-current.txt').read_text()
    for destination, source in copies.items():
        output = target / destination
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, output)
    (target / 'input-sha256.json').write_text(json.dumps(inputs, indent=2) + '\n')
    manifest = {str(path.relative_to(target)): sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target / 'files-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for name, digest in manifest.items():
        assert sha(target / name) == digest, name
    for name, digest in inputs.items():
        assert sha(root / name) == digest, name
    print(f'PASS {len(manifest)} effective-snapshot evidence files; {len(inputs)} current input hashes; three Qt programs/results, offline CLI proof and archive manifest verified')


if __name__ == '__main__':
    main()
