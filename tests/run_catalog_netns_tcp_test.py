#!/usr/bin/env python3
"""Verify the packaged catalog sender/listener over two isolated network stacks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import time

RAW_LIMIT = 20 * 1024 ** 3
RECEIVER_IP = '198.18.148.1'
SENDER_IP = '198.18.148.2'
FIXTURE_BYTES = 4 * 1024 * 1024


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def command(*args, timeout=30):
    result = subprocess.run(list(map(str, args)), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True,
                            timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError('Command failed (%d): %s\n%s' %
                           (result.returncode, ' '.join(map(str, args)), result.stdout))
    return result.stdout.strip()


def json_record(output, expected_kind):
    for line in reversed(output.splitlines()):
        try:
            value = json.loads(line)
        except ValueError:
            continue
        if isinstance(value, dict) and value.get('kind') == expected_kind:
            return value
    raise RuntimeError('Command omitted JSON record kind ' + expected_kind + ': ' + output[-1000:])


def wait_namespace(process, current):
    deadline = time.monotonic() + 5
    path = Path('/proc/%d/ns/net' % process.pid)
    while time.monotonic() < deadline:
        try:
            if os.readlink(path) != current:
                return
        except FileNotFoundError:
            pass
        if process.poll() is not None:
            raise RuntimeError('Network namespace helper exited early')
        time.sleep(0.02)
    raise RuntimeError('Network namespace helper did not enter a new network namespace')


def run(args):
    if os.geteuid() != 0:
        raise RuntimeError('Run inside an unprivileged user/network namespace: unshare -Urn ...')
    catalog = Path(args.catalog).resolve(strict=True)
    fixture_tool = Path(args.fixture_tool).resolve(strict=True)
    result_path = Path(args.result).absolute()
    if result_path.exists() or result_path.is_symlink():
        raise FileExistsError('Refusing to overwrite evidence: ' + str(result_path))
    result_path.parent.mkdir(parents=True, exist_ok=True)
    evidence = {
        'schema': 'getdaq.catalog-netns-tcp:1', 'status': 'failed',
        'network': 'two isolated Linux network namespaces connected by a private veth pair',
        'authentication': 'none; ordinary TCP/IP', 'real_hardware': False,
        'catalog_binary_sha256': sha256(catalog), 'fixture_tool_sha256': sha256(fixture_tool),
        'runner_sha256': sha256(Path(__file__)),
        'maximum_aggregate_synthetic_raw_bytes': RAW_LIMIT,
    }
    temporary = Path(tempfile.mkdtemp(prefix='getdaq-catalog-netns-'))
    receiver_ns = sender_ns = listener = None
    raw_paths = []
    try:
        fixture_root = temporary / 'fixture-work'
        fixture_root.mkdir(mode=0o700)
        os.environ['GETDAQ_CATALOG_TEST_ROOT'] = str(fixture_root)
        source_root = temporary / 'source'
        source_root.mkdir(mode=0o700)
        source_catalog = Path(command(fixture_tool, '--prepare-netns-catalog',
                                     source_root, FIXTURE_BYTES, timeout=60))
        source_catalog = source_catalog.resolve(strict=True)
        source_inspection = command(catalog, '--inspect-catalog', source_catalog, timeout=60)
        source_info = json_record(source_inspection, 'getdaq.receiver-catalog-complete')

        receiver_ns = subprocess.Popen(['unshare', '-n', 'sleep', '180'])
        sender_ns = subprocess.Popen(['unshare', '-n', 'sleep', '180'])
        current = os.readlink('/proc/self/ns/net')
        wait_namespace(receiver_ns, current)
        wait_namespace(sender_ns, current)
        suffix = str(os.getpid())[-5:]
        recv_link, send_link = 'gdqrc' + suffix, 'gdqsd' + suffix
        command('ip', 'link', 'add', recv_link, 'type', 'veth', 'peer', 'name', send_link)
        command('ip', 'link', 'set', recv_link, 'netns', receiver_ns.pid)
        command('ip', 'link', 'set', send_link, 'netns', sender_ns.pid)
        for process, link, address in ((receiver_ns, recv_link, RECEIVER_IP),
                                       (sender_ns, send_link, SENDER_IP)):
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'link', 'set', 'lo', 'up')
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'addr', 'add',
                    address + '/30', 'dev', link)
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'link', 'set', link, 'up')

        target_root = temporary / 'archive-target'
        target_root.mkdir(mode=0o700)
        listener = subprocess.Popen([
            'nsenter', '-t', str(receiver_ns.pid), '-n', '--', str(catalog),
            '--listen-catalog-tcp', RECEIVER_IP, '0', str(target_root), '--once'],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        ready, _, _ = select.select([listener.stdout], [], [], 10)
        if not ready:
            raise RuntimeError('Catalog TCP listener did not announce readiness')
        ready_line = listener.stdout.readline()
        ready_message = json.loads(ready_line)
        port = ready_message.get('port')
        if ready_message.get('phase') != 'listening' or not isinstance(port, int) or not 0 < port <= 65535:
            raise RuntimeError('Malformed catalog TCP listener readiness: ' + ready_line)

        send_output = command('nsenter', '-t', sender_ns.pid, '-n', '--', catalog,
                              '--send-catalog-tcp', source_catalog, RECEIVER_IP, port,
                              timeout=120)
        send_receipt = json_record(send_output, 'getdaq.catalog-transfer-complete')
        listener_status = listener.wait(timeout=120)
        listener_output = ready_line + listener.stdout.read()
        listener = None
        if listener_status != 0:
            raise RuntimeError('Catalog TCP listener failed: ' + listener_output)
        targets = list(target_root.glob('getdaq-transfer-*'))
        if len(targets) != 1 or not targets[0].is_dir():
            raise RuntimeError('Expected exactly one published archive catalog')
        target_catalog = targets[0]
        verify_output = command(catalog, '--verify-transfer', target_catalog, timeout=120)
        if json_record(verify_output, 'getdaq.catalog-transfer-complete') != send_receipt:
            raise RuntimeError('Sender receipt differs from independent target verification')
        if not source_info or send_receipt.get('source_request_key') != source_info.get('request_key'):
            raise RuntimeError('Transfer receipt is not bound to the inspected source catalog')

        raw_paths = sorted(temporary.rglob('*.graw'))
        generated = sum(path.stat().st_size for path in raw_paths)
        if not raw_paths or generated > RAW_LIMIT:
            raise RuntimeError('Aggregate source/target raw exceeded the declared bound')
        evidence.update({
            'status': 'passed', 'catalog_send_over_veth': True,
            'source_inspect_passed': True, 'target_full_content_verify_passed': True,
            'target_catalog_count': len(targets),
            'source_catalog': str(source_catalog.relative_to(temporary)),
            'target_catalog': str(target_catalog.relative_to(temporary)),
            'source_request_key': source_info.get('request_key'),
            'target_source_request_key': send_receipt.get('source_request_key'),
            'synthetic_raw_generated_bytes': generated,
            'synthetic_raw_files_generated': len(raw_paths),
            'send_receipt_sha256': hashlib.sha256(send_output.encode()).hexdigest(),
            'listener_output_sha256': hashlib.sha256(listener_output.encode()).hexdigest(),
        })
    finally:
        if listener is not None and listener.poll() is None:
            listener.terminate()
            try:
                listener.wait(timeout=3)
            except subprocess.TimeoutExpired:
                listener.kill(); listener.wait(timeout=3)
        for process in (receiver_ns, sender_ns):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=3)
        raw_paths = list(temporary.rglob('*.graw')) if temporary.exists() else []
        evidence['synthetic_raw_deleted_bytes'] = sum(path.stat().st_size for path in raw_paths)
        evidence['synthetic_raw_deleted_files'] = len(raw_paths)
        shutil.rmtree(temporary)
        evidence['raw_remaining'] = len(list(temporary.rglob('*.graw'))) if temporary.exists() else 0
        evidence['owned_temporary_root_removed'] = not temporary.exists()
        if evidence['raw_remaining'] or not evidence['owned_temporary_root_removed']:
            evidence['status'] = 'failed'
        with result_path.open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(evidence, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write('\n')
    if evidence['status'] != 'passed':
        raise RuntimeError('Catalog cross-namespace TCP regression failed; see ' + str(result_path))
    print('PASS catalog sender/listener across two network namespaces; full verification passed; '
          '%d raw bytes deleted' % evidence['synthetic_raw_deleted_bytes'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', required=True, help='Exact getdaq-catalog executable under test')
    parser.add_argument('--fixture-tool', required=True, help='C++ test helper that creates a valid bounded catalog')
    parser.add_argument('--result', required=True)
    run(parser.parse_args())


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError, KeyError) as error:
        print('Catalog network-namespace TCP test failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
