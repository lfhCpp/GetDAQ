#!/usr/bin/env python3
"""Build an isolated gSOAP server from the old ECC interface and test the Qt client."""
import argparse
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import selectors
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'third_party/archives/gsoap-2.8.124.orig.tar.gz'
ARCHIVE_SHA256 = '1f6727ccf0e84a5dc7c86a554d0b0e2adb12f66b6e601b21d081de9041443800'


def checked_run(command, cwd=None, env=None):
    subprocess.run([str(part) for part in command], cwd=cwd, env=env, check=True)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def safe_extract(archive, destination):
    with tarfile.open(archive, 'r:gz') as package:
        for member in package.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or any(part in ('', '.', '..') for part in name.parts):
                raise ValueError('Unsafe gSOAP archive path: ' + member.name)
            if not (member.isfile() or member.isdir()):
                raise ValueError('Unexpected link or special file in gSOAP archive: ' + member.name)
        if sys.version_info >= (3, 12):
            package.extractall(destination, filter='data')
        else:
            package.extractall(destination)


def find_qmake(requested):
    candidate = requested or shutil.which('qmake') or shutil.which('qmake-qt5')
    if not candidate:
        raise RuntimeError('Qt 5 qmake was not found; pass --qmake explicitly')
    result = subprocess.run([str(candidate), '-query', 'QT_VERSION'], check=True,
                            stdout=subprocess.PIPE, text=True).stdout.strip()
    if not result.startswith('5.'):
        raise RuntimeError('This compatibility probe requires the project Qt 5 test build; found ' + result)
    return candidate


def reserve_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def wait_ready(process, port, timeout=10):
    deadline = time.monotonic() + timeout
    with selectors.DefaultSelector() as ready:
        ready.register(process.stdout, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('gSOAP fixture exited before readiness: ' + process.stderr.read())
            wait = min(0.1, max(0.0, deadline - time.monotonic()))
            if not ready.select(timeout=wait):
                continue
            line = process.stdout.readline()
            if line.strip() == 'READY ' + str(port):
                return
            raise RuntimeError('Unexpected gSOAP fixture startup output: ' + line.strip())
    raise RuntimeError('Timed out waiting for the gSOAP fixture')


def remove_owned_raw_files(root):
    raw_files = list(root.rglob('*.graw'))
    regular_files = [path for path in raw_files if path.is_file() and not path.is_symlink()]
    total_bytes = sum(path.stat().st_size for path in regular_files)
    if len(regular_files) != len(raw_files):
        raise RuntimeError('Unexpected non-regular raw fixture in owned test tree')
    for path in raw_files:
        path.unlink()
    if list(root.rglob('*.graw')):
        raise RuntimeError('Raw test fixture cleanup left .graw files')
    if total_bytes > 20 * 1024 * 1024 * 1024:
        raise RuntimeError('Synthetic raw files exceeded the 20 GiB test safety limit; owned files were removed')
    return len(raw_files), total_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qmake', help='Qt 5 qmake used to build tests/tests.pro')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--work-dir', help='Use an empty directory and keep generated test files there')
    parser.add_argument('--evidence', type=Path,
                        help='Write a compact JSON receipt and QtTest log under build-receiver/')
    parser.add_argument('--full-qt-suite', action='store_true',
                        help='Run all Qt tests; the gSOAP process test is included in that run')
    args = parser.parse_args()
    if args.jobs < 1 or args.jobs > 8:
        parser.error('--jobs must be between 1 and 8')
    evidence_path = None
    test_log_path = None
    if args.evidence:
        evidence_path = args.evidence.resolve()
        evidence_root = (ROOT / 'build-receiver').resolve()
        if evidence_root not in evidence_path.parents or evidence_path.exists() or evidence_path.is_symlink():
            parser.error('--evidence must name a new file under build-receiver/')
        test_log_path = evidence_path.with_suffix('.log')
        if test_log_path.exists() or test_log_path.is_symlink():
            parser.error('The evidence log path already exists')
    if sha256(ARCHIVE) != ARCHIVE_SHA256:
        raise RuntimeError('Pinned gSOAP source archive SHA-256 mismatch')
    qmake = find_qmake(args.qmake)
    c_compiler = os.environ.get('CC', 'cc')
    cxx_compiler = os.environ.get('CXX', 'c++')

    if args.work_dir:
        work_dir = Path(args.work_dir).resolve()
        work_dir.mkdir(parents=True, exist_ok=True)
        if any(work_dir.iterdir()):
            raise RuntimeError('--work-dir must be empty: ' + str(work_dir))
        temporary_scope = nullcontext(str(work_dir))
    else:
        temporary_scope = tempfile.TemporaryDirectory(prefix='getdaq-legacy-gsoap-')
    with temporary_scope as temp_name:
        temporary = Path(temp_name)
        source_root = temporary / 'source'
        source_root.mkdir()
        safe_extract(ARCHIVE, source_root)
        upstream = source_root / 'gsoap-2.8.124'
        gsoap = upstream / 'gsoap'
        gsoap_src = gsoap / 'src'
        visual = gsoap / 'VisualStudio2005/soapcpp2/soapcpp2'
        binding = temporary / 'bindings'
        binding.mkdir()
        shutil.copy2(ROOT / 'source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh', binding)

        generator = temporary / 'soapcpp2'
        generator_sources = [
            gsoap_src / 'soapcpp2_yacc.tab.c',
            visual / 'lex.soapcpp2_lex.c',
            gsoap_src / 'symbol2.c', gsoap_src / 'error2.c',
            gsoap_src / 'init2.c', gsoap_src / 'soapcpp2.c',
        ]
        checked_run([c_compiler, '-O2', '-DWITH_YACC', '-DLINUX',
                     '-I' + str(gsoap_src), '-I' + str(gsoap),
                     *generator_sources, '-o', generator])
        checked_run([generator, '-z1', '-L', '-x', '-pecc', '-n',
                     '-d' + str(binding), 'ecc.hh'], cwd=binding)
        if not (binding / 'eccServer.cpp').is_file() or not (binding / 'ecceccObject.h').is_file():
            raise RuntimeError('soapcpp2 did not generate the expected historical ECC server bindings')

        fixture = temporary / 'gsoap-ecc-fixture'
        checked_run([cxx_compiler, '-std=c++11', '-O2', '-I' + str(gsoap),
                     '-I' + str(gsoap_src), '-I' + str(binding),
                     binding / 'eccC.cpp', binding / 'eccServer.cpp',
                     ROOT / 'tests/legacy-gsoap/gsoap_ecc_fixture.cpp',
                     gsoap / 'stdsoap2.cpp', '-o', fixture])

        test_build = temporary / 'qt-test-build'
        test_build.mkdir()
        checked_run([qmake, ROOT / 'tests/tests.pro', 'DEFINES+=GETDAQ_GSOAP_INTEROP'], cwd=test_build)
        checked_run(['make', '-s', '-j' + str(args.jobs)], cwd=test_build)

        port = reserve_port()
        request_log = temporary / 'soap-requests.log'
        server = subprocess.Popen([str(fixture), str(port), str(request_log)],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_ready(server, port)
            test_env = os.environ.copy()
            test_env['GETDAQ_GSOAP_FIXTURE_PORT'] = str(port)
            test_env.setdefault('QT_QPA_PLATFORM', 'offscreen')
            test_home = temporary / 'test-home'
            test_tmp = temporary / 'test-temp'
            test_home.mkdir()
            test_tmp.mkdir()
            test_env['HOME'] = str(test_home)
            test_env['TMPDIR'] = str(test_tmp)
            test_env['XDG_CACHE_HOME'] = str(test_home / '.cache')
            test_env['XDG_CONFIG_HOME'] = str(test_home / '.config')
            test_env['XDG_DATA_HOME'] = str(test_home / '.local/share')
            test_command = [test_build / 'getdaq-tests']
            if not args.full_qt_suite:
                test_command.append('legacyGsoapProcessInterop')
            test_run = subprocess.run(test_command, env=test_env, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, text=True, check=False)
            print(test_run.stdout, end='')
            if test_run.returncode:
                raise subprocess.CalledProcessError(test_run.returncode, test_command)
            summary = re.search(r'Totals: (\d+) passed, (\d+) failed, (\d+) skipped, (\d+) blacklisted',
                                test_run.stdout)
            if not summary:
                raise RuntimeError('QtTest output did not contain its final Totals line')
            captured = request_log.read_text(encoding='utf-8')
            operations = ('GetState', 'Describe', 'Prepare', 'Configure', 'Start',
                          'Stop', 'Pause', 'Resume', 'Breakup', 'Undo')
            for expected in ('OPERATION ' + operation for operation in operations):
                if expected not in captured:
                    raise RuntimeError('The gSOAP server did not decode expected operation: ' + expected)
            text_operations = operations[1:]
            if captured.count('CONFIG_BEGIN\n') != len(text_operations) or \
                    captured.count('TABLE_BEGIN\n') != len(text_operations) or \
                    captured.count('fixture-describe-id') != len(text_operations) or \
                    captured.count('DataLinkSet') != 2 * len(text_operations) or \
                    captured.count('46005') != len(text_operations):
                raise RuntimeError('The gSOAP server did not decode all text operation parameters: ' +
                                   'config=' + str(captured.count('CONFIG_BEGIN\n')) +
                                   ', table=' + str(captured.count('TABLE_BEGIN\n')) +
                                   ', id=' + str(captured.count('fixture-describe-id')) +
                                   ', links=' + str(captured.count('DataLinkSet')) +
                                   ', port=' + str(captured.count('46005')))
        finally:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
            raw_files, raw_bytes = remove_owned_raw_files(temporary)
        result = {
            'schema': 'getdaq.legacy-gsoap-interop:1',
            'passed': True,
            'gsoap_version': '2.8.124',
            'source_interface': 'source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh',
            'operations': ['GetState', 'Describe', 'Prepare', 'Configure', 'Start',
                           'Stop', 'Pause', 'Resume', 'Breakup', 'Undo'],
            'soap_namespace': 'urn:http://localhost:8083/',
            'soap_action': '""',
            'generated_fixture_isolated_from_product': True,
            'gsoap_raw_payload_bytes': 0,
            'test_raw_files_remaining': raw_files,
            'test_raw_bytes_remaining': raw_bytes,
            'temporary_tree_removed': not args.work_dir,
            'hardware_accessed': False,
            'qt_test': {
                'passed': int(summary.group(1)), 'failed': int(summary.group(2)),
                'skipped': int(summary.group(3)), 'blacklisted': int(summary.group(4)),
                'output_sha256': hashlib.sha256(test_run.stdout.encode('utf-8')).hexdigest(),
            },
        }
        if evidence_path:
            test_log_path.write_text(test_run.stdout, encoding='utf-8')
            result['qt_test']['log'] = str(test_log_path.relative_to(ROOT))
            result['qt_test']['log_sha256'] = sha256(test_log_path)
            evidence_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                                     encoding='utf-8')
            result['evidence'] = str(evidence_path.relative_to(ROOT))
        print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError, tarfile.TarError) as error:
        print('Legacy gSOAP compatibility probe failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
