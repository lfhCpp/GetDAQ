#!/usr/bin/env python3
"""Produce bounded evidence for an Ubuntu 24 Envoy UI package refresh."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--test-binary', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    test_binary = args.test_binary.resolve(strict=True)
    evidence = args.evidence.absolute()
    if binary.name != 'GetDAQ' or not os.access(binary, os.X_OK):
        raise RuntimeError('--binary must be an executable named GetDAQ')
    if not os.access(test_binary, os.X_OK):
        raise RuntimeError('--test-binary must be executable')
    if evidence.exists():
        raise RuntimeError('Refusing to overwrite existing evidence: ' + str(evidence))
    evidence.parent.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    with tempfile.TemporaryDirectory(prefix='getdaq-envoy-ui-') as temporary:
        root = Path(temporary)
        runtime = root / 'runtime'
        runtime.mkdir(mode=0o700)
        for name in ('cache', 'config', 'data'):
            (root / name).mkdir()
        env.update(XDG_RUNTIME_DIR=str(runtime), XDG_CACHE_HOME=str(root / 'cache'),
                   XDG_CONFIG_HOME=str(root / 'config'), XDG_DATA_HOME=str(root / 'data'))
        result = subprocess.run([str(test_binary), 'nativeGuiMetadataAndWholeSystemControls',
                                 '-o', str(root / 'widgets.txt,txt')],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=120, check=False, env=env)
        log = result.stdout
        result_file = root / 'widgets.txt'
        if result_file.is_file():
            log += '\n' + result_file.read_text(encoding='utf-8', errors='replace')
        if result.returncode or not re.search(r'Totals: 3 passed, 0 failed', log):
            raise RuntimeError('Native GUI metadata/topology QtTest failed:\n' + log[-8000:])
        single_result = subprocess.run([str(test_binary), 'nativeSingleCoBoLifecycleUsesWholeEccTransitions',
                                        '-o', str(root / 'single-cobo.txt,txt')],
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, timeout=120, check=False, env=env)
        single_log = single_result.stdout
        single_result_file = root / 'single-cobo.txt'
        if single_result_file.is_file():
            single_log += '\n' + single_result_file.read_text(encoding='utf-8', errors='replace')
        if single_result.returncode or not re.search(r'Totals: 3 passed, 0 failed', single_log):
            raise RuntimeError('Single-CoBo lifecycle QtTest failed:\n' + single_log[-8000:])
        soap_result = subprocess.run([str(test_binary), 'legacySoapHttpRequestSendsQuotedEmptySoapAction',
                                      '-o', str(root / 'soapaction.txt,txt')],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, timeout=120, check=False, env=env)
        soap_log = soap_result.stdout
        soap_result_file = root / 'soapaction.txt'
        if soap_result_file.is_file():
            soap_log += '\n' + soap_result_file.read_text(encoding='utf-8', errors='replace')
        if soap_result.returncode or not re.search(r'Totals: 3 passed, 0 failed', soap_log):
            raise RuntimeError('Legacy SOAPAction HTTP QtTest failed:\n' + soap_log[-8000:])
        scenario = {
            'native_gui_address_and_topology_hints': True,
            'native_single_cobo_lifecycle_mapping': True,
            'legacy_soap_action_header': True,
        }
        raw_paths = list(root.rglob('*.graw'))
        if raw_paths:
            raise RuntimeError('UI test unexpectedly created raw files: ' + ', '.join(map(str, raw_paths)))

    value = {
        'schema': 'getdaq.envoy-ui-package-regression:1',
        'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'runner_sha256': sha(Path(__file__)),
        'program_sha256': {'GetDAQ': sha(binary), 'getdaq-tests': sha(test_binary)},
        'passed': True,
        'scenario': scenario,
        'owned_test_root_removed': not root.exists(),
        'synthetic_raw_bytes_generated': 0,
        'synthetic_raw_bytes_deleted': 0,
        'raw_remaining': 0,
        'real_hardware': False,
        'ubuntu24_startup': 'performed by package refresh after target-loader validation',
        'scope': 'offscreen UI and process startup only; no ECC, hardware, receiver, or DAQ payload',
    }
    temporary_file = evidence.with_name(evidence.name + '.tmp')
    try:
        with temporary_file.open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_file, evidence)
    finally:
        if temporary_file.exists():
            temporary_file.unlink()
    print(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        print('Envoy UI package regression failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
