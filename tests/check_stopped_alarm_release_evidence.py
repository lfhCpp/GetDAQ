#!/usr/bin/env python3
"""Bind known-stop alarm release to actual requests, normal seals and native audits."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check(folder):
    result = read(folder / 'result.json')
    assert result['passed'] and result['raw_remaining'] == 0
    assert read(folder / 'cleanup.json')['remaining_raw'] == [] and not list(folder.rglob('*.graw'))
    count = 0
    for run in result['runs']:
        name = run['scenario']
        if not name.startswith('alarm-release-'):
            continue
        count += 1
        root = folder / name
        proof = read(root / 'stopped-alarm-release-result.json')
        assert proof['passed'] and proof['encoding_minor'] == int(name[-1])
        assert proof['normal_receiver_finish_verified'] and not proof['original_run_complete']
        loss = proof['power_off_ack_loss']
        op = read(root / 'operation-stopped-alarm-release.json')
        request = op['request']
        assert request['command'] == 'release-stopped-alarm'
        assert request['failed_operation_id'] == proof['original_stopped_operation_id']
        assert request['alarm_operation_id'] == proof['original_alarm_id']
        canonical = (root / 'controller/network/operations' / request['operation_id'] / 'request.json').read_bytes()
        assert json.loads(canonical) == request
        assert digest(b'getdaq.ecc-control-request:1\n' + canonical) == op['request_key']
        journal = rows(root / 'controller/network/operations.jsonl')
        held = [r for r in journal if r['event'] == 'alarm-held'][0]
        assert held['operation_id'] == proof['original_alarm_id']
        observation = json.loads(held['evidence'])
        assert observation['stop_requested'] and not observation['controller_outcome_unknown']
        native = rows(root / 'controller/ecc.jsonl')
        original = [r for r in native if r.get('id') == request['failed_operation_id']]
        assert sum(r['phase'] == 'complete' for r in original) == 1
        assert not any(r['phase'] in ('unknown', 'held', 'reconciled') for r in original)
        before = read(root / 'before-alarm-status.json')
        after = read(root / 'after-stopped-alarm-release.json')
        for old, now in zip(before['data']['nodes'], after['data']['nodes']):
            assert old['agent_id'] == now['agent_id'] and old['instance_id'] == now['instance_id']
            a, b = old['lifecycle'], now['lifecycle']
            assert a['session_id'] == b['session_id'] and b['state'] == 'stopped'
            assert a['receivers'][0]['pid'] == b['receivers'][0]['pid']
            assert b['receivers'][0]['reaped'] and b['receivers'][0]['seal_verified']
        breakup = 'alarm-breakup-' + digest(('getdaq.stopped-alarm-breakup:1\n' + request['snapshot_sha256'] + '\n' + request['operation_id']).encode())
        assert sum(r['phase'] == 'complete' and r.get('id') == breakup for r in native) == 1
        policy = read(root / 'after-stopped-alarm-policy.json')
        if loss:
            assert op['phase'] == 'failed' and after['controller']['outcome_unknown'] and after['control_needs_reconciliation']
            assert policy['latched']
            assert any(r['phase'] == 'unknown' and r.get('id') == request['operation_id'] for r in native)
            assert not any(r['event'] == 'complete' and r.get('operation_id') == request['operation_id'] for r in journal)
        else:
            assert op['phase'] == 'complete' and op['result']['state'] == 2
            assert not op['result']['outcome_unknown'] and not op['result']['needs_reconciliation']
            assert not policy['latched'] and policy['phase'] == 'watching'
            complete = next(r for r in journal if r['event'] == 'complete' and r['operation_id'] == request['operation_id'])
            assert complete['alarm_policy_reset'] and complete['alarm_stop_operation_id'] == request['alarm_operation_id']
            assert complete['alarm_observation_sha256'] == digest(b'getdaq.ecc-alarm-observation:1\n' + held['evidence'].encode())
            assert 'receiver_policy_reset' not in complete
            audit = rows(root / 'controller/operations' / request['operation_id'] / 'operation.jsonl')
            verified = next(r for r in audit if r['phase'] == 'stopped_alarm_release_verified')
            assert verified['read_only_final_observation'] and verified['fresh_prepare_required'] and not verified['original_run_complete']
            for entry in verified['audits']:
                path = Path(entry['path'])
                assert path.is_relative_to(root.resolve()) and digest(path.read_bytes()) == entry['sha256']
            second = read(root / 'second-stopped-alarm-policy.json')
            assert second['latched'] and second['phase'] == 'complete' and second['operation_id'] != request['alarm_operation_id']
            assert read(root / 'cold-stopped-alarm-denied.json')['phase'] == 'failed'
    assert count
    print('PASS known original result, exact request/observation hashes, normal receiver seals, verified release or retained UNKNOWN;', count, 'scenarios')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    check(parser.parse_args().folder.resolve())
