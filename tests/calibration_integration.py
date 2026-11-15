#!/usr/bin/env python3
"""AsAd 校准命令的跨进程/真实 TCP 测试，板端为本机模拟对象。"""
import copy
import json
import pathlib
import select
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: calibration_integration.py GETDAQ_HARDWARE HARDWARE_CLIENT_TESTS')
client, fixture = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:])

def call(*args, code=0):
    reply = subprocess.run([client, *map(str, args)], capture_output=True, text=True, timeout=10)
    assert reply.returncode == code, (args, reply.returncode, reply.stdout, reply.stderr)
    return json.loads(reply.stdout) if code == 0 else reply.stderr

with tempfile.TemporaryDirectory(prefix='getdaq-calibration-cli-') as directory:
    root = pathlib.Path(directory)
    server = subprocess.Popen([fixture, '--serve-calibration'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        assert select.select([server.stdout], [], [], 10)[0], 'Fixture start timed out'
        address = server.stdout.readline().strip()
        assert address
        description = {'schema_version': 1, 'kind': 'getdaq.hardware-description', 'node_id': 'calibration-fixture',
                       'devices': [call('describe', address, name) for name in ('ctrl', 'asad')],
                       'sources': [], 'warnings': []}
        path = root / 'description.json'
        path.write_text(json.dumps(description))
        call('validate-description', path)
        for minor in (0, 1):
            audit = root / ('calibration-1.' + str(minor) + '.jsonl')
            args = ['--encoding', '1.' + str(minor), '--description', path,
                    'asad-calibrate', address, '0x5', audit]
            reply = call(*args)
            assert reply == {'phase': 'complete', 'operation': 'asad-calibrate', 'asad_mask': 5, 'full_prepare': False}
            records = [json.loads(line) for line in audit.read_text().splitlines()]
            assert records[-1]['phase'] == 'complete'
            steps = records[0]['steps']
            assert len(steps) == 24
            reads = [step['register'] for step in steps if step['operation'] in ('read', 'expect')]
            assert reads == ['monitorID', 'AIM0', 'AIM1', 'AIM2', 'AIM3'] * 2
            assert all(step['mask'] == '255' for step in steps if step['operation'] == 'expect' and step['register'] == 'AIM0')
            assert len([record for record in records if record['phase'] == 'observed']) == 6
            assert len([record for record in records if record['phase'] == 'matched']) == 4
            assert steps[-1]['value'] == '5' and steps[-1]['register'] == 'ASAD_sc_mask'
            original = audit.read_bytes()
            assert 'exclusively' in call(*args, code=1)
            assert audit.read_bytes() == original, 'Existing audit changed'
        for mask in ('0', '16', '-1', '18446744073709551616'):
            target = root / 'invalid-mask.jsonl'
            call('--description', path, 'asad-calibrate', 'invalid-proxy', mask, target, code=2)
            assert not target.exists(), 'Invalid input created an audit'
        call('asad-calibrate', 'invalid-proxy', '1', root / 'unbound', code=2)
        assert not (root / 'unbound').exists()
        drift = copy.deepcopy(description)
        drift['devices'][1]['registers'] = drift['devices'][1]['registers'][:-1]
        drift_path = root / 'drift.json'; drift_path.write_text(json.dumps(drift))
        call('--description', drift_path, 'asad-calibrate', address, '1', root / 'drift-audit', code=1)
        assert not (root / 'drift-audit').exists(), 'Metadata mismatch reached execution'
        print('PASS AsAd calibration CLI: both encodings, sparse mask, observed identity, preflight, audit exclusivity')
    finally:
        if server.poll() is None:
            try:
                server.communicate('\n', timeout=5)
            except subprocess.TimeoutExpired:
                server.kill(); server.communicate()
        assert server.returncode == 0, 'Fixture failed'
