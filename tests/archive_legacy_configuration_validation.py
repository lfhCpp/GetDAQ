#!/usr/bin/env python3
"""Verify current inputs and small evidence for the legacy configuration stage.

This creates a local checkpoint index, not a portable or complete release archive.
It never stores transmission payloads or runs hardware commands.
"""
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-legacy-config'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    inputs = {}
    evidence = {}
    programs = {}

    def record(path, group):
        path = path.resolve()
        assert path.is_file() and path.suffix != '.graw', path
        group[str(path)] = sha(path)

    legacy = json.loads((BUILD / 'build-receipt.json').read_text())
    assert legacy['original_source_unchanged_during_build'] is True
    assert len(legacy['recipes']) == 55
    for name, digest in legacy['inputs'].items():
        path = Path(name)
        assert sha(path) == digest, ('Changed historical reference input', path)
        inputs[name] = digest
    record(BUILD / 'build-receipt.json', evidence)
    reference = Path(legacy['program'])
    assert sha(reference) == legacy['program_sha256']
    record(reference, programs)

    for folder in [BUILD / 'current-probe', BUILD / 'model', BUILD / 'gui',
                   Path('/tmp/getdaq-legacy-config-current-probe-asan'),
                   Path('/tmp/getdaq-legacy-config-model-asan')]:
        receipt = json.loads((folder / 'build-receipt.json').read_text())
        source_hashes = json.loads((folder / 'source-sha256.json').read_text())
        for name, digest in source_hashes.items():
            path = ROOT / name
            assert sha(path) == digest, ('Qt build input changed', path)
            if str(path) in inputs:
                assert inputs[str(path)] == digest
            inputs[str(path)] = digest
        program = folder / receipt['target']
        assert sha(program) == receipt['program_sha256']
        assert sha(folder / 'Makefile') == receipt['makefile_sha256']
        record(program, programs)
        for name in ('build-receipt.json', 'source-sha256.json', 'generated-sha256.json', 'results-verified-build.txt'):
            record(folder / name, evidence)

    for name in ('GetDAQ', 'getdaq-sentry', 'getdaq-simulator', 'getdaq-config', 'getdaq-config-check'):
        record(ROOT / 'bin' / name, programs)
    record(ROOT / 'build-ice-compat/getdaq-system-config', programs)
    for relative in ('GetDAQ.pro', 'apps/envoy/envoy.pro', 'apps/sentry/sentry.pro',
                     'apps/simulator/simulator.pro', 'apps/configtool/configtool.pro',
                     'apps/configcheck/configcheck.pro', 'apps/configtool/main.cpp',
                     'tests/legacy_configuration_integration.py', 'tests/archive_legacy_configuration_validation.py',
                     'tests/system_experiment_integration.py', 'tests/effective_snapshot_integration.py',
                     'third_party/validation/boost-1.55.0.json', 'third_party/validation/README.md',
                     'README.md', 'third_party/README.md', 'docs/LEGACY_CONFIGURATION_RUNTIME.md',
                     'docs/CONFIGURATION_EDITOR.md', 'docs/REFACTOR_STATUS.md', 'docs/VALIDATION.md'):
        record(ROOT / relative, inputs)
    for mode, folder, program in [('ordinary', 'runtime-ordinary-final', BUILD / 'current-probe/current-configuration-probe'),
                                  ('sanitized', 'runtime-sanitized-final', Path('/tmp/getdaq-legacy-config-current-probe-asan/current-configuration-probe'))]:
        scope = json.loads((BUILD / folder / 'scope.json').read_text())
        assert scope['cases'] == 192 and scope['processes'] == 384
        assert scope['categories'] == {'equal': 170, 'documented-extension': 6, 'strict-rejection': 8, 'both-reject': 8}
        assert scope['legacy_program_sha256'] == sha(reference)
        assert scope['current_program_sha256'] == sha(program)
        assert scope['temporary_configs_deleted'] is True and scope['raw_data_transfer'] is False
        for name in ('scope.json', 'processes.json'):
            record(BUILD / folder / name, evidence)
    logs = BUILD / 'verified-test-logs'
    logs.mkdir(exist_ok=True)
    for original, total in [(BUILD / 'results-model-final.txt', 63),
                            (Path('/tmp/getdaq-legacy-config-model-asan-results.txt'), 63),
                            (BUILD / 'results-gui-current-final.txt', 71)]:
        text = original.read_text()
        assert re.search(r'Totals: %d passed, 0 failed' % total, text), original
        destination = logs / original.name
        shutil.copyfile(original, destination)
        record(destination, evidence)
    snapshots = list((BUILD / 'cli-snapshot-final').glob('effective-snapshot-*/scope.json'))
    assert len(snapshots) == 1
    scope = json.loads(snapshots[0].read_text())
    assert scope['process_cases'] == 12 and scope['program_sha256'] == sha(ROOT / 'bin/getdaq-config')
    for p in snapshots[0].parent.glob('*.json'):
        record(p, evidence)
    system_log = BUILD / 'results-system-final.txt'
    assert '36 process cases; original source preserved' in system_log.read_text()
    record(system_log, evidence)
    for name in ('results-gui-final.txt', 'results-gui-unsandboxed-final.txt', 'results-gui-lifecycle-isolated.txt'):
        record(BUILD / name, evidence)
    assert not list(BUILD.rglob('*.graw'))
    checkpoint = {'schema': 1, 'scope': 'local file/XML original runtime comparison and current Qt configuration regression',
                  'not_a_portable_replay_archive': True, 'whole_refactor_complete': False,
                  'old_library_instrumented': False, 'system_qt_instrumented': False,
                  'platforms_actually_tested': ['Ubuntu20.04.6/GCC9.4/Qt5.12.8 local environment'],
                  'raw_data_transferred': False, 'hardware_connected': False,
                  'inputs': inputs, 'programs': programs, 'evidence': evidence,
                  'test_totals': {'legacy_comparison_each': 192, 'legacy_processes_each': 384,
                                  'model_each': 63, 'gui': 71, 'snapshot_cli': 12, 'offline_system': 36}}
    (BUILD / 'checkpoint.json').write_text(json.dumps(checkpoint, indent=2) + '\n')
    print('PASS current legacy configuration checkpoint: %d inputs; %d programs; %d evidence files; no raw payload' %
          (len(inputs), len(programs), len(evidence)))


if __name__ == '__main__':
    main()
