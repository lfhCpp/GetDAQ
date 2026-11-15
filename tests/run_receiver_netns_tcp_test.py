#!/usr/bin/env python3
"""Bounded synthetic TCP receiver test between two isolated Linux network namespaces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

RAW_LIMIT = 20 * 1024 ** 3
PAYLOAD_BYTES = 32 * 1024 ** 2
RECEIVER_IP = '198.18.147.1'
SENDER_IP = '198.18.147.2'
DATA_PORT = 46147


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def command(*arguments):
    result = subprocess.run(list(map(str, arguments)), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=20, check=False)
    if result.returncode:
        raise RuntimeError('Command failed: %s\n%s' % (' '.join(map(str, arguments)), result.stdout))
    return result.stdout.strip()


def sender(source, address):
    source = Path(source)
    deadline = time.monotonic() + 15
    connection = None
    while time.monotonic() < deadline:
        try:
            connection = socket.create_connection((address, DATA_PORT), timeout=2)
            break
        except OSError:
            time.sleep(0.05)
    if connection is None:
        raise RuntimeError('Could not connect to the receiver namespace')
    with connection, source.open('rb') as stream:
        while True:
            block = stream.read(256 * 1024)
            if not block:
                break
            connection.sendall(block)
        connection.shutdown(socket.SHUT_WR)
    return {'bytes': source.stat().st_size, 'sha256': sha256(source)}


def wait_namespace(process, current_namespace):
    deadline = time.monotonic() + 5
    namespace = Path('/proc/%d/ns/net' % process.pid)
    while time.monotonic() < deadline:
        try:
            value = os.readlink(namespace)
            if value != current_namespace:
                return
        except FileNotFoundError:
            pass
        if process.poll() is not None:
            raise RuntimeError('Network namespace helper exited early')
        time.sleep(0.02)
    raise RuntimeError('Network namespace helper did not enter a new network namespace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receiver', type=Path)
    parser.add_argument('--result', type=Path)
    parser.add_argument('--send-source', type=Path)
    parser.add_argument('--send-address')
    args = parser.parse_args()
    if args.send_source:
        print(json.dumps(sender(args.send_source, args.send_address), sort_keys=True))
        return 0
    if not args.receiver or not args.result:
        parser.error('--receiver and --result are required')
    if os.geteuid() != 0:
        raise RuntimeError('Run inside an unprivileged user/network namespace: unshare -Urn python3 ...')
    if PAYLOAD_BYTES * 2 > RAW_LIMIT:
        raise RuntimeError('Synthetic source plus receiver raw exceeds the 20 GiB budget')
    receiver = args.receiver.resolve(strict=True)
    result_path = args.result.absolute()
    if result_path.exists() or result_path.is_symlink():
        raise FileExistsError('Refusing to overwrite result: ' + str(result_path))
    result_path.parent.mkdir(parents=True, exist_ok=True)

    evidence = {'schema': 'getdaq.receiver-netns-tcp:1', 'status': 'failed',
                'payload_bytes_per_host': PAYLOAD_BYTES,
                'aggregate_synthetic_raw_limit_bytes': RAW_LIMIT,
                'network': 'two isolated Linux network namespaces connected by a private veth pair',
                'authentication': 'none; ordinary TCP/IP', 'real_hardware': False,
                'receiver_sha256': sha256(receiver),
                'runner_sha256': sha256(Path(__file__))}
    temporary = Path(tempfile.mkdtemp(prefix='getdaq-netns-tcp-'))
    receiver_namespace = sender_namespace = receiver_process = None
    link_created = False
    raw_before = []
    try:
        source_root = temporary / 'source'
        receive_root = temporary / 'receive'
        source_root.mkdir(mode=0o700)
        receive_root.mkdir(mode=0o700)
        source = source_root / 'synthetic.graw'
        block = bytes((index * 73 + 19) % 251 for index in range(64 * 1024))
        with source.open('xb') as stream:
            for unused in range(PAYLOAD_BYTES // len(block)):
                stream.write(block)
            stream.flush()
            os.fsync(stream.fileno())
        source_digest = sha256(source)
        incoming = receive_root / 'incoming'
        incoming.mkdir(mode=0o700)

        receiver_namespace = subprocess.Popen(['unshare', '-n', 'sleep', '180'])
        sender_namespace = subprocess.Popen(['unshare', '-n', 'sleep', '180'])
        current_namespace = os.readlink('/proc/self/ns/net')
        wait_namespace(receiver_namespace, current_namespace)
        wait_namespace(sender_namespace, current_namespace)
        suffix = str(os.getpid())[-5:]
        receiver_link, sender_link = 'gdqra' + suffix, 'gdqsb' + suffix
        command('ip', 'link', 'add', receiver_link, 'type', 'veth', 'peer', 'name', sender_link)
        link_created = True
        command('ip', 'link', 'set', receiver_link, 'netns', receiver_namespace.pid)
        command('ip', 'link', 'set', sender_link, 'netns', sender_namespace.pid)
        for process, link, address in ((receiver_namespace, receiver_link, RECEIVER_IP),
                                       (sender_namespace, sender_link, SENDER_IP)):
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'link', 'set', 'lo', 'up')
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'addr', 'add',
                    address + '/30', 'dev', link)
            command('nsenter', '-t', process.pid, '-n', '--', 'ip', 'link', 'set', link, 'up')

        receiver_log = temporary / 'receiver.log'
        with receiver_log.open('w') as output:
            receiver_process = subprocess.Popen([
                'nsenter', '-t', str(receiver_namespace.pid), '-n', '--', str(receiver),
                '--output', str(incoming), '--bind', RECEIVER_IP,
                '--port', str(DATA_PORT), '--status-bind', '127.0.0.1',
                '--status-port', '46148', '--storage-mode', 'byte-storage',
                '--rotate-mib', '64', '--once'], stdout=output, stderr=subprocess.STDOUT)
            sender_output = command('nsenter', '-t', sender_namespace.pid, '-n', '--',
                                    sys.executable, __file__, '--send-source', source,
                                    '--send-address', RECEIVER_IP)
            sender_result = json.loads(sender_output)
            exit_code = receiver_process.wait(timeout=30)
            receiver_process = None
        receiver_text = receiver_log.read_text(encoding='utf-8', errors='replace')
        if exit_code != 0:
            raise RuntimeError('Receiver exited %d: %s' % (exit_code, receiver_text))
        raw_files = sorted(incoming.glob('*.graw'))
        if len(raw_files) != 1:
            raise RuntimeError('Expected one byte-storage output file, found %d' % len(raw_files))
        raw = raw_files[0]
        destination_digest = sha256(raw)
        seal = json.loads((incoming / '.getdaq-receiver-seal.json').read_text(encoding='utf-8'))
        if (sender_result['bytes'] != PAYLOAD_BYTES or sender_result['sha256'] != source_digest or
                raw.stat().st_size != PAYLOAD_BYTES or destination_digest != source_digest or
                seal.get('complete') is not True or seal.get('sender_eof_confirmed') is not True):
            raise RuntimeError('Cross-namespace payload, hash, or EOF seal verification failed')
        raw_before = [{'path': str(path.relative_to(temporary)), 'bytes': path.stat().st_size,
                       'sha256': sha256(path)} for path in temporary.rglob('*.graw')]
        if len(raw_before) != 2 or sum(item['bytes'] for item in raw_before) != 2 * PAYLOAD_BYTES:
            raise RuntimeError('Synthetic raw accounting differs from the declared 64 MiB aggregate')
        evidence.update(status='passed', source_sha256=source_digest,
                        destination_sha256=destination_digest,
                        sender_bytes=sender_result['bytes'],
                        receiver_file_bytes=raw.stat().st_size,
                        synthetic_raw_generated_bytes=sum(item['bytes'] for item in raw_before),
                        eof_sealed=True, receiver_exit_code=exit_code,
                        namespace_count=2, raw_cleanup_pending=True)
    finally:
        if receiver_process is not None and receiver_process.poll() is None:
            receiver_process.terminate()
            try:
                receiver_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                receiver_process.kill()
                receiver_process.wait(timeout=3)
        for process in (receiver_namespace, sender_namespace):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
        remaining = list(temporary.rglob('*.graw')) if temporary.exists() else []
        removed_bytes = sum(path.stat().st_size for path in remaining)
        removed_count = len(remaining)
        shutil.rmtree(temporary)
        raw_after = list(temporary.rglob('*.graw')) if temporary.exists() else []
        evidence['synthetic_raw_deleted_bytes'] = removed_bytes
        evidence['raw_remaining'] = len(raw_after)
        evidence['owned_temporary_root_removed'] = not temporary.exists()
        evidence['raw_files_deleted'] = removed_count
        evidence['raw_cleanup_pending'] = False
        if not evidence['owned_temporary_root_removed'] or raw_after:
            evidence['status'] = 'failed'
        with result_path.open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(evidence, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write('\n')
    if evidence['status'] != 'passed':
        raise RuntimeError('Isolated TCP receiver regression failed; see ' + str(result_path))
    print('PASS isolated two-namespace TCP receiver; 32 MiB delivered and hash/EOF verified; 64 MiB raw deleted')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError, KeyError) as error:
        print('Receiver network-namespace test failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
