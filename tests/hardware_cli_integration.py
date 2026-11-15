#!/usr/bin/env python3
"""真实进程/TCP 的硬件 CLI 测试，只启动本机模拟板端。Python 不是运行依赖。"""
import json
import pathlib
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: hardware_cli_integration.py GETDAQ_HARDWARE HARDWARE_CLIENT_TESTS')
client, fixture = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='getdaq-hardware-cli-') as directory:
    server = subprocess.Popen([fixture, '--serve'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        # Fixture itself starts synchronously and emits one proxy after successful listen.
        import select
        if not select.select([server.stdout], [], [], 10)[0]:
            raise RuntimeError('fixture did not start within 10 seconds')
        address = server.stdout.readline().strip()
        assert address, 'fixture did not provide an address'
        def call(*args, code=0):
            result = subprocess.run([client, *args], capture_output=True, text=True, timeout=10)
            assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
            return json.loads(result.stdout) if result.returncode == 0 else result.stderr

        info = call('devices', address)
        assert info['node'] == 'test CoBo 中文' and 'ctrl' in info['devices']
        info = call('describe', address, 'ctrl')
        assert info['register_bytes'] == 4
        assert info['registers'][0]['fields'][0]['width_bits'] == 4
        audit = pathlib.Path(directory) / 'write.jsonl'
        reply = call('write-register', address, 'wide', 'control', '0xffffffffffffffff', str(audit))
        assert reply == {'phase': 'acknowledged', 'readback_verified': False}
        assert call('--encoding', '1.1', 'read-register', address, 'wide', 'control')['value'] == '18446744073709551615'
        records = [json.loads(line) for line in audit.read_text().splitlines()]
        assert [r['phase'] for r in records] == ['intent', 'begin', 'acknowledged', 'complete']
        assert records[0]['steps'][0]['value'] == '18446744073709551615'
        assert 'exclusively' in call('write-register', address, 'wide', 'control', '3', str(audit), code=1)
        assert call('read-register', address, 'wide', 'control')['value'] == '18446744073709551615'
        call('write-register', 'not-a-valid-proxy', 'wide', 'control', '-1', str(audit), code=2)
        call('write-field', address, 'ctrl', 'mixed', 'status', '1', str(pathlib.Path(directory)/'ro'), code=1)
        pipeline = pathlib.Path(directory) / 'pipeline.jsonl'
        reply = call('pipeline-init', address, str(pipeline))
        assert reply['full_prepare'] is False
        records = [json.loads(line) for line in pipeline.read_text().splitlines()]
        assert len(records[0]['steps']) == 16 and records[-1]['phase'] == 'complete'
        assert not (pathlib.Path(directory)/'ro').exists()
        print('PASS hardware CLI: JSON, UTF-8, uint64 precision, audit, no replay, pipeline sequence')
    finally:
        if server.poll() is None:
            try:
                server.communicate('\n', timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.communicate()
        assert server.returncode == 0, 'fixture failed'
