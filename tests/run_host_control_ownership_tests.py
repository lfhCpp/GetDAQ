#!/usr/bin/env python3
"""Actual ECC startup/HTTP reservations. No hardware commands or DAQ traffic."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def port():
    with socket.socket() as server:
        server.bind(('127.0.0.1', 0))
        return server.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixed_fixture', type=Path)
    parser.add_argument('ecc_server', type=Path)
    parser.add_argument('output_parent', type=Path)
    parser.add_argument('--qt-probe', type=Path)
    parser.add_argument('--qt-readback-probe', type=Path)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[1]
    args.output_parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='host-ownership-', dir=args.output_parent.resolve()))
    with (root / 'fixed-input-test.log').open('w') as log:
        subprocess.run([str(args.fixed_fixture.resolve()), str(repository / 'tests/data/cobo-hardware.json'),
                        str(repository / 'tests/data/mutant-hardware.json'), str(root / 'inputs')],
                       stdout=log, stderr=subprocess.STDOUT, timeout=60, check=True)
    base = json.loads((root / 'inputs/service.json').read_text())
    ownership = root / 'ownership'
    ownership.mkdir(mode=0o700)
    token = Path(base['credential_file']).read_text().strip()
    configurations = []
    for index in range(2):
        config = dict(base, host_control_ownership={'directory': str(ownership)},
                      audit_root=str(root / ('controller-' + str(index))),
                      deployment_id='ownership-' + str(index), port=port())
        path = root / ('service-' + str(index) + '.json')
        path.write_text(json.dumps(config) + '\n')
        configurations.append((path, config))
    processes = []
    logs = []
    def start(index, label):
        log = (root / (label + '.log')).open('w')
        logs.append(log)
        process = subprocess.Popen([str(args.ecc_server.resolve()), '--config', str(configurations[index][0])],
                                   stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    def query(index, process, authenticate=True, path='/v1/ownership'):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('ECC service exited during startup; inspect ' + str(root))
            request = urllib.request.Request('http://127.0.0.1:' + str(configurations[index][1]['port']) + path,
                                             headers={'Authorization': 'Bearer ' + (token if authenticate else 'invalid')})
            try:
                with urllib.request.urlopen(request, timeout=1) as response:
                    return json.load(response)
            except urllib.error.HTTPError:
                raise
            except (urllib.error.URLError, TimeoutError):
                time.sleep(0.03)
        raise RuntimeError('ECC ownership query timed out')
    def stop(process):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)
    try:
        first = start(0, 'first')
        observed = query(0, first)
        status = observed['ownership']
        assert status['enabled'] and status['held_at_last_check'] and status['cached']
        assert status['scope'] == 'single-host-cooperating-ecc'
        assert not status['board_global_lock_verified'] and not status['cross_host_lock_verified']
        assert status['resources'] == ['127.0.0.1']
        if args.qt_probe:
            identity = query(0, first, path='/v1/identity')
            with (root / 'qt-probe.log').open('w') as log:
                subprocess.run([str(args.qt_probe.resolve()), '127.0.0.1', str(configurations[0][1]['port']),
                                base['credential_file'], base['service_id'], base['client_id'],
                                identity['snapshot_sha256'], identity['input_binding_sha256'], str(root / 'qt-ownership.json'),
                                'ownership-observe', '--ownership-only'],stdout=log,stderr=subprocess.STDOUT,timeout=15,check=True)
            qt_observation=json.loads((root / 'qt-ownership.json').read_text())
            assert qt_observation['instance_id']==observed['instance_id'] and qt_observation['ownership']['held_at_last_check']
            journal=Path(configurations[0][1]['audit_root']) / 'network/operations.jsonl'
            assert all(json.loads(line)['event'] not in ('accepted','complete','failed') for line in journal.read_text().splitlines())
        if args.qt_readback_probe:
            identity=query(0,first,path='/v1/identity')
            with (root/'qt-readback-probe.log').open('w') as log:
                subprocess.run([str(args.qt_readback_probe.resolve()),'127.0.0.1',str(configurations[0][1]['port']),
                                base['credential_file'],base['service_id'],base['client_id'],identity['snapshot_sha256'],
                                identity['input_binding_sha256'],str(root/'qt-readbacks.json'),'readbacks-observe','--readbacks-only'],
                               stdout=log,stderr=subprocess.STDOUT,timeout=15,check=True)
            observation=json.loads((root/'qt-readbacks.json').read_text())
            assert observation['instance_id']==observed['instance_id'] and observation['readbacks']['phase']=='not-observed'
            journal=Path(configurations[0][1]['audit_root'])/'network/operations.jsonl'
            assert all(json.loads(line)['event'] not in ('accepted','complete','failed') for line in journal.read_text().splitlines())
        try:
            query(0, first, authenticate=False)
            raise AssertionError('Ownership metadata was unauthenticated')
        except urllib.error.HTTPError as error:
            assert error.code == 401
        second = start(1, 'conflict')
        assert second.wait(timeout=10) != 0
        assert not Path(configurations[1][1]['audit_root']).exists()
        logs[-1].flush()
        assert 'already reserved' in (root / 'conflict.log').read_text()
        stop(first)
        third = start(1, 'after-release')
        resumed = query(1, third)
        assert resumed['ownership']['held_at_last_check']
        assert observed['instance_id'] != resumed['instance_id']
        evidence = {'passed': True, 'authenticated': True, 'first': observed, 'after_release': resumed,
                    'qt_readonly_client_checked': bool(args.qt_probe),
                    'qt_readback_client_checked': bool(args.qt_readback_probe),
                    'second_rejected_before_audit_root': True, 'hardware_commands_sent': False,
                    'raw_receiver_started': False, 'daq_bytes': 0,
                    'scope': 'startup and authenticated cache; not board-global ownership or hardware recovery'}
        (root / 'proof.json').write_text(json.dumps(evidence, indent=2) + '\n')
        print('PASS actual ECC startup conflict/release/authenticated ownership cache; no hardware/DAQ: ' + str(root))
    finally:
        for process in processes:
            stop(process)
        for log in logs:
            log.close()


if __name__ == '__main__':
    main()
