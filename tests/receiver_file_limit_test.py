#!/usr/bin/env python3
"""Exercise real kernel short writes/EFBIG without filling the filesystem.

RLIMIT_FSIZE limits each private test file to 2 or 4 KiB. It is not an ENOSPC,
power-loss or physical disk failure test. All generated raw files are deleted.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import selectors
import signal
import socket
import struct
import subprocess
import tempfile


def frame(event, size=4096):
    data = bytearray([0xa5]) * size
    data[0] = 6
    data[1:4] = (size // 64).to_bytes(3, 'big')
    data[4] = 7
    struct.pack_into('>HBHHI', data, 5, 1, 4, 2, 4, (size - 128) // 4)
    struct.pack_into('>I', data, 22, event)
    data[26:28] = bytes([0, 0])
    return bytes(data)


def available_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def run_case(program, root, name, limit, payload, success, legacy=False):
    folder = root / name
    folder.mkdir(mode=0o700)
    port = available_port()

    def set_limit():
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))

    command = [str(program), '--output', str(folder), '--bind', '127.0.0.1',
               '--port', str(port), '--status-port', '0', '--queue-mib', '2',
               '--max-frame-mib', '1', '--once']
    child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, bufsize=1, preexec_fn=set_limit)
    removed = []
    try:
        with selectors.DefaultSelector() as selector:
            selector.register(child.stdout, selectors.EVENT_READ)
            assert selector.select(10), 'Receiver startup timeout'
            startup = child.stdout.readline()
        assert startup.startswith('Listening data '), startup
        with socket.create_connection(('127.0.0.1', port), timeout=10) as sender:
            try:
                sender.sendall(payload)
            except (ConnectionResetError, BrokenPipeError):
                pass  # An already reported write failure may close input early.
        out, err = child.communicate(timeout=15)
        (folder / 'receiver-output.txt').write_text(startup + out + err)
        raw = sorted(folder.glob('*.graw'))
        assert len(raw) == 1
        actual = raw[0].read_bytes()
        assert len(actual) == min(limit, len(payload))
        assert actual == payload[:len(actual)], 'Partial write changed the prefix'
        seal_path = folder / '.getdaq-receiver-seal.json'
        if legacy:
            assert child.returncode == -signal.SIGXFSZ, (child.returncode, out, err)
            assert not seal_path.exists()
            return {'case': name, 'legacy_signal_abort_confirmed': True,
                    'return_code': child.returncode, 'partial_file_bytes': len(actual)}
        assert child.returncode == (0 if success else 1), (child.returncode, out, err)
        final = json.loads(out.strip().splitlines()[-1])
        assert final['stream_sealed'] == success
        if success:
            assert not final['error'] and final['received_bytes'] == final['sealed_bytes'] == len(payload)
            seal = json.loads(seal_path.read_text())
            assert seal['complete'] and seal['durable']
        else:
            assert final['error'] and 'File too large' in final['error'], final
            assert not seal_path.exists()
        return {'case': name, 'file_limit_bytes': limit, 'sent_bytes': len(payload),
                'partial_file_bytes': len(actual), 'partial_file_sha256': hashlib.sha256(actual).hexdigest(),
                'return_code': child.returncode, 'expected_success': success, 'final_status': final}
    finally:
        if child.poll() is None: child.terminate()
        try: child.communicate(timeout=10)
        except subprocess.TimeoutExpired: child.kill();child.communicate()
        for raw in folder.glob('*.graw'):
            assert not raw.is_symlink()
            removed.append({'name': raw.name, 'bytes': raw.stat().st_size})
            raw.unlink()
        (folder / 'payload-cleanup.json').write_text(json.dumps({'files': removed,
            'deleted_bytes': sum(entry['bytes'] for entry in removed)}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--expect-legacy-abort', action='store_true',
                        help='Confirm the archived receiver aborts by SIGXFSZ; not a product pass')
    args = parser.parse_args()
    program = args.receiver.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='receiver-file-limit-', dir=args.evidence)).resolve()
    cases = [('buffered-eof', 2048, b''.join(frame(i) for i in range(1, 4)), False)]
    if not args.expect_legacy_abort:
        cases += [('buffered-flush', 4096, b''.join(frame(i) for i in range(1, 66)), False),
                  ('direct-large-frame', 4096, frame(1, 768 * 1024), False),
                  ('exact-limit-success', 4096, frame(1), True)]
    results = [run_case(program, root, *case, legacy=args.expect_legacy_abort) for case in cases]
    assert not list(root.glob('*/*.graw'))
    record = {'schema': 1, 'receiver_sha256': hashlib.sha256(program.read_bytes()).hexdigest(),
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'legacy_demonstration': args.expect_legacy_abort, 'scope': __doc__,
              'cases': results, 'generated_raw_deleted': True, 'physical_hosts': 1, 'real_hardware': False}
    (root / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    print('CONFIRMED legacy signal abort;' if args.expect_legacy_abort else 'PASS four real kernel file-limit cases;',
          'raw payload deleted; evidence', root)


if __name__ == '__main__':
    main()
