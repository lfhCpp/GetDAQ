#!/usr/bin/env python3
"""仅供开发验证：独立生成 MFM 字节，经真实 TCP 送入不含 Qt 的程序。"""
import hashlib
import json
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request
import urllib.error

BINARY = pathlib.Path(sys.argv[1]).resolve()
SENTRY = pathlib.Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None


def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def ports(count):
    # Keep each reservation open until all distinct ports have been selected.
    sockets = [socket.socket() for _ in range(count)]
    try:
        for reserved in sockets:
            reserved.bind(('127.0.0.1', 0))
        return [reserved.getsockname()[1] for reserved in sockets]
    finally:
        for reserved in sockets:
            reserved.close()


def frame(event, size=4096, little=False, block_size=64, header_blocks=2, revision=4, frame_type=1):
    # GET CoboFormats-Rev-4: block 64, header 128, event@22, cobo@26, asad@27.
    endian = 'little' if little else 'big'
    b = bytearray([event & 255]) * size
    b[0] = (block_size.bit_length() - 1) | (128 if little else 0)
    b[1:4] = (size // block_size).to_bytes(3, endian)
    b[4] = 7
    b[5:7] = frame_type.to_bytes(2, endian)
    b[7] = revision
    b[8:10] = header_blocks.to_bytes(2, endian)
    b[10:12] = (4).to_bytes(2, endian)
    b[12:16] = ((size - block_size * header_blocks) // 4).to_bytes(4, endian)
    b[22:26] = event.to_bytes(4, endian)
    b[26:28] = bytes([0, 0])
    return bytes(b)


class Run:
    def __init__(self, folder, *args):
        self.folder = pathlib.Path(folder)
        for _ in range(3):
            self.data, self.status, self.export = ports(3)
            self.p = subprocess.Popen([str(BINARY), '--output', str(folder), '--port', str(self.data),
                                       '--status-port', str(self.status), '--export-port', str(self.export),
                                       '--queue-mib', '2', '--max-frame-mib', '2', '--export-queue-mib', '1', *args],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                self.wait(lambda s: s['service'] == 'getdaq-receiver')
                return
            except AssertionError as startup_error:
                if self.p.poll() is None:
                    self.p.kill()
                    self.p.communicate(timeout=5)
                    raise
                if 'Address already in use' not in str(startup_error):
                    raise
        raise AssertionError('no receiver ports after three confirmed bind collisions')

    def get(self):
        with urllib.request.urlopen('http://127.0.0.1:%d/status' % self.status, timeout=2) as r:
            return json.load(r)

    def wait(self, predicate):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if self.p.poll() is not None:
                raise AssertionError('receiver exited: ' + repr(self.p.communicate()))
            try:
                s = self.get()
                if predicate(s):
                    return s
            except (OSError, ValueError):
                pass
            time.sleep(.02)
        raise AssertionError('status timeout')

    def stop(self, expected=0):
        if self.p.poll() is None:
            self.p.terminate()
        out, err = self.p.communicate(timeout=15)
        assert self.p.returncode == expected, (self.p.returncode, out, err)
        return json.loads(out.strip().splitlines()[-1])

    def close(self):
        if self.p.poll() is None:
            self.p.kill()
        self.p.communicate()


def exact(n, sock):
    out = bytearray()
    while len(out) < n:
        b = sock.recv(n-len(out))
        assert b, 'early EOF in export'
        out.extend(b)
    return bytes(out)


def archive_integration():
    if SENTRY is None:
        print('SKIP: cross-process sentry archive (supply sentry binary as second argument)')
        return
    process_check_integration()
    with tempfile.TemporaryDirectory(prefix='getdaq-archive-') as folder:
        endpoint = port()
        config = pathlib.Path(folder) / 'sentry.json'
        config.write_text(json.dumps({'data_dir': folder, 'check_process': False,
                                      'bind': '127.0.0.1', 'port': endpoint, 'settle_ms': 0}))
        agent = subprocess.Popen([str(SENTRY), '--config', str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        r = None
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    with urllib.request.urlopen('http://127.0.0.1:%d/status' % endpoint, timeout=.5):
                        break
                except OSError:
                    assert time.monotonic() < deadline, 'sentry did not start'
                    time.sleep(.02)

            def catalog():
                req = urllib.request.Request('http://127.0.0.1:%d/catalog' % endpoint,
                      data=b'{"experiment":"Exp","run_number":1}', headers={'Content-Type': 'application/json'})
                try:
                    with urllib.request.urlopen(req, timeout=3) as reply:
                        return reply.status, json.load(reply)
                except urllib.error.HTTPError as error:
                    return error.code, json.load(error)

            r = Run(folder)
            payload = frame(1) + frame(2)
            with socket.create_connection(('127.0.0.1', r.data), timeout=5) as sender:
                sender.sendall(payload)
            r.wait(lambda s: s['stream_sealed'] and s['frames'] == 2)
            assert catalog()[0] == 409, 'archive accepted while receiver owns directory'
            assert not (pathlib.Path(folder) / 'Exp').exists()
            r.stop()
            inspector = BINARY.parent / 'getdaq-inspect'
            files = list(pathlib.Path(folder).glob('*.graw'))
            check = subprocess.run([str(inspector), str(files[0])], capture_output=True, text=True)
            assert check.returncode == 0 and 'frames=2' in check.stdout, check
            with files[0].open('ab') as changed:
                changed.write(b'!')
            assert catalog()[0] == 409, 'archive ignored changed file size'
            with files[0].open('r+b') as restored:
                restored.truncate(len(payload))
            code, status = catalog()
            assert code == 200 and status['data_path_files'] == 0, (code, status)
            archived = pathlib.Path(folder) / 'Exp' / 'run_0001'
            assert next(archived.glob('*.graw')).read_bytes() == payload
            assert (archived / 'receiver-seal.json').exists()
            assert catalog()[0] == 409, 'duplicate archive accepted'
        finally:
            if r is not None:
                r.close()
            agent.terminate()
            try:
                agent.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                agent.kill(); agent.communicate()
    print('PASS: live receiver archive refused; offline inspector; changed file refused; sealed data archived exactly once')


def process_check_integration():
    """Exercise Linux ps/comm detection against the actual receiver process."""
    with tempfile.TemporaryDirectory(prefix='getdaq-sentry-process-') as folder:
        endpoint = port()
        config = pathlib.Path(folder) / 'sentry.json'
        config.write_text(json.dumps({'data_dir': folder, 'process_name': BINARY.name,
                                      'check_process': True, 'bind': '127.0.0.1',
                                      'port': endpoint, 'settle_ms': 0}))
        receiver = Run(folder)
        agent = subprocess.Popen([str(SENTRY), '--config', str(config)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    with urllib.request.urlopen('http://127.0.0.1:%d/status' % endpoint,
                                                timeout=.5) as reply:
                        status = json.load(reply)
                        assert reply.status == 200 and status['process'] == BINARY.name, status
                        break
                except OSError:
                    assert time.monotonic() < deadline, 'sentry process check did not become ready'
                    time.sleep(.02)

            receiver.stop()
            try:
                urllib.request.urlopen('http://127.0.0.1:%d/status' % endpoint, timeout=2)
                raise AssertionError('sentry accepted status after the configured process exited')
            except urllib.error.HTTPError as error:
                body = json.load(error)
                assert error.code == 503 and BINARY.name in body['error'], (error.code, body)
            assert not list(pathlib.Path(folder).glob('*.graw')), 'process check generated DAQ payload'
        finally:
            receiver.close()
            agent.terminate()
            try:
                agent.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                agent.kill(); agent.communicate()
    print('PASS: sentry reports the actual getdaq-receiver process while live and 503 after exit; no raw payload')


def run_tests():
    with tempfile.TemporaryDirectory(prefix='getdaq-tcp-') as folder:
        r = Run(folder)
        try:
            # Two consumers, one frame split into single bytes, then coalesced frames.
            watchers = [socket.create_connection(('127.0.0.1', r.export), timeout=5) for _ in range(2)]
            r.wait(lambda s: s['export_clients'] == 2)
            # Conduit currently accepts meta_type == 8: GET Rev5 big-endian.
            # Rev4 (0x06) and little-endian Rev5 (0x88) are valid receiver inputs,
            # but do not pass that downstream parser's exact meta-type check.
            conduit_frame = frame(4, block_size=256, header_blocks=1, revision=5)
            little_rev5 = frame(5, little=True, block_size=256, header_blocks=1, revision=5)
            assert conduit_frame[0] == 8
            assert little_rev5[0] == 0x88
            payloads = [frame(1, frame_type=1), frame(2, little=True, frame_type=2, revision=5),
                        frame(3, frame_type=3), conduit_frame, little_rev5]
            with socket.create_connection(('127.0.0.1', r.data), timeout=5) as sender:
                for b in payloads[0]:
                    sender.sendall(bytes([b]))
                sender.sendall(b''.join(payloads[1:]))
            s = r.wait(lambda s: s['stream_sealed'] and s['frames'] == len(payloads))
            assert s['received_bytes'] == s['sealed_bytes'] == sum(map(len, payloads))
            assert s['acquisition_event_count_available'] and s['acquisition_events'] == 4
            for watcher in watchers:
                for payload in payloads:
                    length = struct.unpack('<Q', exact(8, watcher))[0]
                    assert exact(length, watcher) == payload
                watcher.close()
            files = list(pathlib.Path(folder).glob('*.graw'))
            assert len(files) == 2, [path.name for path in files]
            by_source = {path.name.split('_')[0]: path.read_bytes() for path in files}
            assert by_source['CoBo0'] == payloads[0] + payloads[1] + payloads[3] + payloads[4]
            assert by_source['MFM3'] == payloads[2]
            # A second receiver cannot take ownership of the same data directory.
            p = subprocess.run([str(BINARY), '--output', folder, '--port', str(port()), '--status-port', '0'], capture_output=True)
            assert p.returncode != 0 and b'already used' in p.stderr
            r.stop()
            seal = json.loads((pathlib.Path(folder) / '.getdaq-receiver-seal.json').read_text())
            assert seal['complete'] and seal['durable'] and int(seal['received_bytes']) == sum(map(len, payloads))
        finally:
            r.close()
    print('PASS: TCP fragmentation, endian variants, multicast export, exact files, seal, directory ownership')

    with tempfile.TemporaryDirectory(prefix='getdaq-slow-') as folder:
        r = Run(folder)
        try:
            slow = socket.create_connection(('127.0.0.1', r.export), timeout=5)
            slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            r.wait(lambda s: s['export_clients'] == 1)
            checksum = hashlib.sha256()
            start = time.monotonic()
            count = 8192  # 32 MiB, deliberately larger than the export queue and receive pool.
            with socket.create_connection(('127.0.0.1', r.data), timeout=15) as sender:
                for event in range(1, count+1):
                    b = frame(event); checksum.update(b); sender.sendall(b)
            s = r.wait(lambda s: s['stream_sealed'] and s['frames'] == count)
            assert s['export_disconnections'] >= 1
            actual = hashlib.sha256()
            for p in sorted(pathlib.Path(folder).glob('*.graw')):
                with p.open('rb') as f:
                    for chunk in iter(lambda: f.read(1024*1024), b''):
                        actual.update(chunk)
            assert actual.digest() == checksum.digest(), 'slow exporter changed raw recording'
            assert s['sealed_bytes'] == count * 4096
            print('PASS: slow exporter disconnected; 32 MiB file hash exact; elapsed %.3fs (not a hardware throughput claim)' % (time.monotonic()-start))
            slow.close(); r.stop()
        finally:
            r.close()

    for description, payload in [('truncated', frame(1) + frame(2)[:100]), ('invalid length', frame(1) + b'\x06\xff\xff\xff\x07\x00\x01\x04')]:
        with tempfile.TemporaryDirectory(prefix='getdaq-bad-') as folder:
            r = Run(folder, '--once')
            try:
                with socket.create_connection(('127.0.0.1', r.data), timeout=5) as sender:
                    sender.sendall(payload)
                r.p.wait(timeout=10)
                s = r.stop(expected=1)
                assert s['error'] and not s['stream_sealed']
                assert not (pathlib.Path(folder) / '.getdaq-receiver-seal.json').exists()
                files = list(pathlib.Path(folder).glob('*.graw'))
                assert len(files) == 1 and files[0].read_bytes() == frame(1), 'lost preceding complete frame'
            finally:
                r.close()
        print('PASS:', description, 'is a visible failure, not a successful run')


if __name__ == '__main__':
    run_tests()
    archive_integration()
