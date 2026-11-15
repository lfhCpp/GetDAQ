#!/usr/bin/env python3
"""板端替身控制走Ice TCP，原始帧直接送native receiver；Python仅为测试依赖。"""
import json
import pathlib
import select
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

FIXTURE = pathlib.Path(sys.argv[1]).resolve()
RECEIVER = pathlib.Path(sys.argv[2]).resolve()


def frame(event):
    data = bytearray([event & 255]) * 4096
    data[0:12] = bytes([6, 0, 0, 64, 7, 0, 1, 4, 0, 2, 0, 4])
    data[12:16] = ((4096 - 128) // 4).to_bytes(4, 'big')
    data[22:26] = event.to_bytes(4, 'big')
    data[26:28] = b'\x00\x00'
    return bytes(data)


def line(process):
    readable, _, _ = select.select([process.stdout], [], [], 10)
    assert readable, 'bridge response timeout'
    text = process.stdout.readline()
    assert text, ('bridge exited', process.poll())
    return json.loads(text)


def command(process, text):
    process.stdin.write(text + '\n')
    process.stdin.flush()
    return line(process)


def status(port):
    with urllib.request.urlopen('http://127.0.0.1:%d/status' % port, timeout=1) as reply:
        return json.load(reply)


def wait_status(process, port, predicate):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        assert process.poll() is None, ('receiver exited early', process.communicate())
        try:
            value = status(port)
            if predicate(value):
                return value
        except OSError:
            pass
        time.sleep(.02)
    raise AssertionError('receiver status timeout')


def start_receiver(directory):
    # 同时保留两个不同端口到进程启动前；只在确证EADDRINUSE且未开始传数据时重新启动。
    for _ in range(3):
        sockets = [socket.socket(), socket.socket()]
        for item in sockets:
            item.bind(('127.0.0.1', 0))
        data_port, status_port = [item.getsockname()[1] for item in sockets]
        for item in sockets:
            item.close()
        process = subprocess.Popen([str(RECEIVER), '--output', str(directory), '--port', str(data_port),
                                    '--status-port', str(status_port), '--queue-mib', '1', '--once'],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_status(process, status_port, lambda s: s['service'] == 'getdaq-receiver')
            return process, data_port, status_port
        except AssertionError:
            if process.poll() is None:
                process.kill()
                process.communicate(timeout=5)
                raise
            _, error = process.communicate(timeout=5)
            if 'Address already in use' not in error:
                raise
    raise AssertionError('no receiver ports after confirmed bind collisions')


def run_case(minor, mode):
    with tempfile.TemporaryDirectory(prefix='getdaq-sender-receiver-') as folder:
        root = pathlib.Path(folder)
        data, audit = root / 'incoming', root / 'audits'
        data.mkdir()
        audit.mkdir()
        receiver, data_port, status_port = start_receiver(data)
        bridge = None
        try:
            bridge = subprocess.Popen([str(FIXTURE), '--bridge', '127.0.0.1:%d' % data_port, str(minor),
                                       str(audit), 'truncated' if mode == 'truncated' else 'normal'],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      text=True, bufsize=1)
            assert line(bridge)['state'] == 0
            assert command(bridge, 'connect')['state'] == 2
            wait_status(receiver, status_port, lambda s: s['connected'] and s['connections'] == 1)
            assert command(bridge, 'start')['state'] == 4
            stopped = command(bridge, 'stop')
            assert stopped['state'] == 6 and not stopped['unknown']
            if mode != 'truncated':
                paused = wait_status(receiver, status_port, lambda s: s['frames'] == 2)
                assert paused['connected'] and not paused['stream_sealed']
                assert receiver.poll() is None
                assert not (data / '.getdaq-receiver-seal.json').exists(), 'Stop was mistaken for EOF/seal'
                assert command(bridge, 'start')['state'] == 4
                assert command(bridge, 'stop')['state'] == 6
                resumed = wait_status(receiver, status_port, lambda s: s['frames'] == 4)
                assert resumed['connections'] == 1 and resumed['connected'] and not resumed['stream_sealed']
            if mode == 'reply-loss':
                command(bridge, 'fail-disconnect')
            disconnected = command(bridge, 'disconnect')
            if mode == 'reply-loss':
                assert 'UNKNOWN' in disconnected['error']
                snapshot = disconnected['snapshot']
                assert snapshot['state'] == 9 and snapshot['last_confirmed'] == 6
                assert snapshot['issued'] == 8 and snapshot['acknowledged'] == 7
                refused = command(bridge, 'disconnect')
                assert 'transition rejected' in refused['error']
                assert len(refused['snapshot']['calls']) == 8, 'idempotent disconnect was replayed'
            else:
                assert disconnected['state'] == 8 and not disconnected['unknown']
            out, error = receiver.communicate(timeout=15)
            result = json.loads(out.strip().splitlines()[-1])
            raw = b''.join(item.read_bytes() for item in sorted(data.glob('*.graw')))
            if mode == 'truncated':
                assert receiver.returncode == 1 and result['error'], (out, error)
                assert not result['stream_sealed']
                assert raw == frame(1), 'truncated tail lost preceding complete frame'
                assert not (data / '.getdaq-receiver-seal.json').exists(), 'ACKs falsely proved complete data'
            else:
                payload = b''.join(frame(event) for event in [1, 101, 2, 102])
                assert receiver.returncode == 0 and not result['error'], (out, error)
                assert result['stream_sealed'] and result['frames'] == 4
                assert result['received_bytes'] == result['sealed_bytes'] == len(payload)
                assert raw == payload, 'pause/resume/flush bytes differ'
                seal = json.loads((data / '.getdaq-receiver-seal.json').read_text())
                assert seal['complete'] and seal['durable'] and int(seal['received_bytes']) == len(payload)
                assert sum(int(item['bytes']) for item in seal['files']) == len(payload)
                # 正常退出释放目录内核锁；控制断开应答丢失不取消独立的数据封存证据。
                import fcntl
                with (data / '.getdaq-receiver.lock').open('rb') as lock:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            rows = [json.loads(row) for path in sorted(audit.glob('*.jsonl')) for row in path.read_text().splitlines()]
            assert all(not row.get('receiver_sealed', False) for row in rows)
            assert sum(row.get('call') == 'disconnect' and row['phase'] == 'call_begin' for row in rows) == 1
            if mode == 'reply-loss':
                assert any(row['phase'] == 'unknown' for row in rows)
            bridge.stdin.write('quit\n')
            bridge.stdin.flush()
            bridge.communicate(timeout=10)
            assert bridge.returncode == 0
        finally:
            for process in [bridge, receiver]:
                if process is not None:
                    if process.poll() is None:
                        process.kill()
                    process.communicate(timeout=5)
    print('PASS control TCP 1.%d -> native raw TCP receiver: %s' % (minor, mode), flush=True)


for encoding in [0, 1]:
    for scenario in ['normal', 'truncated', 'reply-loss']:
        run_case(encoding, scenario)
print('PASS all 6 cross-process sender/receiver lifecycle cases', flush=True)
