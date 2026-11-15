#!/usr/bin/env python3
"""Small process-level regression for legacy DataRouter ByteStorage behavior."""
import json
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def wait_for_listener(process, port):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError('receiver exited before listening: ' + process.stderr.read().decode(errors='replace'))
        ready, _, _ = select.select([process.stdout], [], [], 0.1)
        if ready:
            line = process.stdout.readline().decode(errors='replace')
            if line.startswith('Listening data 127.0.0.1:' + str(port) + ';'):
                return
    raise RuntimeError('receiver did not listen within 10 seconds')


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: byte_storage_integration.py GETDAQ_RECEIVER')
    receiver = Path(sys.argv[1]).resolve()
    port = free_port()
    payload = bytes((i * 73 + 0xA5) & 0xFF for i in range(384 * 1024 + 137))
    with tempfile.TemporaryDirectory(prefix='getdaq-byte-storage-') as temporary:
        output = Path(temporary) / 'incoming'
        output.mkdir()
        process = subprocess.Popen(
            [str(receiver), '--output', str(output), '--bind', '127.0.0.1', '--port', str(port),
             '--status-port', '0', '--once', '--storage-mode', 'byte-storage', '--rotate-mib', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            wait_for_listener(process, port)
            with socket.create_connection(('127.0.0.1', port), timeout=2) as sender:
                for offset in range(0, len(payload), 8191):
                    sender.sendall(payload[offset:offset + 8191])
            stdout, stderr = process.communicate(timeout=10)
            if process.returncode != 0:
                raise RuntimeError('receiver failed: ' + stderr.decode(errors='replace') + stdout.decode(errors='replace'))
            status = json.loads(stdout.decode().strip().splitlines()[-1])
            files = sorted(output.glob('*.graw'))
            stored = b''.join(path.read_bytes() for path in files)
            if stored != payload:
                raise RuntimeError('ByteStorage changed or reordered received bytes')
            if (status.get('storage_mode') != 'byte-storage' or status.get('frames') != 0 or
                    status.get('acquisition_event_count_available') is not False):
                raise RuntimeError('ByteStorage status mislabeled the raw stream')
            if status.get('received_bytes') != len(payload) or status.get('accepted_file_bytes') != len(payload):
                raise RuntimeError('ByteStorage byte counts differ from received payload')
            if status.get('files') != len(files) or not status.get('stream_sealed'):
                raise RuntimeError('ByteStorage did not report a complete seal')
            if sum(path.stat().st_size for path in files) != len(payload):
                raise RuntimeError('ByteStorage output lengths differ from received payload')
            # Validation must reject FDT before opening a listener because raw-byte
            # mode cannot safely distinguish FDT control words from arbitrary bytes.
            invalid = subprocess.run(
                [str(receiver), '--output', str(output), '--input-protocol', 'FDT',
                 '--storage-mode', 'byte-storage'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
            if invalid.returncode != 2 or b'ByteStorage' not in invalid.stderr:
                raise RuntimeError('ByteStorage accepted FDT or rejected it for an unexpected reason')
            removed_bytes = sum(path.stat().st_size for path in output.iterdir() if path.is_file())
            raw_bytes_deleted = sum(path.stat().st_size for path in files)
            removed_files = 0
            for path in output.iterdir():
                if path.is_file():
                    path.unlink()
                    removed_files += 1
            raw_remaining = sum(1 for _ in output.glob('*.graw'))
            if raw_remaining:
                raise RuntimeError('ByteStorage test left raw files behind')
            print(json.dumps({'status': 'passed', 'payload_bytes': len(payload), 'files': len(files),
                              'sha256': __import__('hashlib').sha256(stored).hexdigest(),
                              'synthetic_raw_bytes_deleted': raw_bytes_deleted,
                              'cleanup_files': removed_files, 'cleanup_bytes': removed_bytes,
                              'raw_remaining': raw_remaining}, sort_keys=True))
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == '__main__':
    main()
