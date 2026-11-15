#!/usr/bin/env python3
"""Offline XML compilation -> native loader -> loopback board CREATE and policy check."""
import copy
import json
import pathlib
import select
import subprocess
import sys
import tempfile

if len(sys.argv) != 4:
    raise SystemExit('usage: description_integration.py GETDAQ_CONFIG GETDAQ_HARDWARE HARDWARE_CLIENT_TESTS')
compiler, client, fixture = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:])
repository = pathlib.Path(__file__).resolve().parents[1]
configs = repository / 'source/get/GetBench-software-release-20210208/data/config'

def run(binary, *args, code=0):
    result = subprocess.run([binary, *map(str, args)], capture_output=True, text=True, timeout=30)
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    return result

with tempfile.TemporaryDirectory(prefix='getdaq-description-integration-') as directory:
    root = pathlib.Path(directory)
    artifacts = {}
    for name, count in [('fullCoBoStandAlone', 11), ('Mutant', 5), ('Zedboard', 4)]:
        target = root / (name + '.json')
        run(compiler, 'compile-hardware', configs / ('hardwareDescription_' + name + '.xcfg'), target)
        content = target.read_bytes()
        run(compiler, 'compile-hardware', configs / ('hardwareDescription_' + name + '.xcfg'), target, code=1)
        assert target.read_bytes() == content, 'Compiler replaced an existing artifact'
        reply = json.loads(run(client, 'validate-description', target).stdout)
        assert reply['valid'] and reply['devices'] == count
        artifacts[name] = target
    data = json.loads(artifacts['fullCoBoStandAlone'].read_text())
    for field, value in [('schema_version', 2), ('unexpected', True)]:
        bad = copy.deepcopy(data); bad[field] = value
        path = root / (field + '.json'); path.write_text(json.dumps(bad))
        run(client, 'validate-description', path, code=1)
    duplicate = root / 'duplicate.json'
    duplicate.write_text(artifacts['fullCoBoStandAlone'].read_text().replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1'))
    run(client, 'validate-description', duplicate, code=1)
    overflow = copy.deepcopy(data)
    overflow['devices'][0]['base_address'] = '18446744073709551616'
    bad = root / 'overflow.json'; bad.write_text(json.dumps(overflow))
    run(client, 'validate-description', bad, code=1)

    server = subprocess.Popen([fixture, '--serve-empty'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        assert select.select([server.stdout], [], [], 10)[0], 'Fixture start timed out'
        address = server.stdout.readline().strip(); assert address
        audit = root / 'create.jsonl'
        reply = json.loads(run(client, 'create-description', address, artifacts['fullCoBoStandAlone'], audit).stdout)
        assert reply['metadata_verified'] and not reply['register_values_read'] and not reply['board_permissions_verified']
        records = [json.loads(line) for line in audit.read_text().splitlines()]
        assert [r['phase'] for r in records] == ['intent', 'acknowledged', 'complete']
        inventory = json.loads(run(client, 'devices', address).stdout)
        assert len(inventory['devices']) == 11 and inventory['node'] == data['node_id']
        run(client, 'create-description', address, artifacts['fullCoBoStandAlone'], root / 'second-create', code=1)
        assert not (root / 'second-create').exists()
        readonly = [(device['name'], reg['name']) for device in data['devices']
                    for reg in device['registers'] if reg['read_only']]
        assert readonly, 'Repository sample must exercise register-level readOnly'
        device, reg = readonly[0]
        run(client, '--description', artifacts['fullCoBoStandAlone'], 'write-register', address,
            device, reg, '1', root / 'blocked-write', code=1)
        assert not (root / 'blocked-write').exists()
        view = json.loads(run(client, '--description', artifacts['fullCoBoStandAlone'], 'describe', address, device).stdout)
        assert next(r for r in view['registers'] if r['name'] == reg)['read_only']
        drift = copy.deepcopy(data)
        matching = next(d for d in drift['devices'] if d['name'] == device)
        matching['base_address'] = str(int(matching['base_address']) + 1)
        bad = root / 'drift.json'; bad.write_text(json.dumps(drift))
        result = run(client, '--description', bad, 'read-register', address, device, reg, code=1)
        assert 'differs' in result.stderr
        print('PASS description pipeline: all 3 repository samples, strict loader, 11-device CREATE, readback, no overwrite, local policy and drift rejection')
    finally:
        if server.poll() is None:
            try:
                server.communicate('\n', timeout=5)
            except subprocess.TimeoutExpired:
                server.kill(); server.communicate()
        assert server.returncode == 0, 'Fixture failed'
