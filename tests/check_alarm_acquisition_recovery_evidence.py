#!/usr/bin/env python3
"""Check explicit alarm acknowledgement, persistent policy resets and cold rejection."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def check(folder, expected_ack=2):
    result = read(folder / 'result.json')
    assert result['passed'] and result['raw_remaining'] == 0
    assert read(folder / 'cleanup.json')['remaining_raw'] == [] and not list(folder.rglob('*.graw'))
    counts = {'ack': 0, 'prepare': 0, 'legacy': 0}
    for run in result['runs']:
        name = run['scenario']
        root = folder / name
        if name.startswith('alarm-preparation-recovery-'):
            continue  # Checked by check_preparation_alarm_recovery_evidence.py.
        if name.startswith('alarm-unknown-prepare-'):
            counts['prepare'] += 1
            op = read(root / 'alarm-preparation-recovery-denied.json')
            assert op['phase'] == 'failed' and 'independent alarm/input/storage hold' in op['message']
            assert op['request']['command'] == 'reconcile-preparation' and op['request']['failed_operation_id'] == 'prepare'
            assert op['result']['outcome_unknown'] and op['result']['operation_id'] == 'prepare'
            assert not any(r['phase'] == 'reconciled' for r in rows(root / 'controller/ecc.jsonl'))
            continue
        if not name.startswith('acquisition-recovery-network-alarm-ack-'):
            assert name in ('acquisition-recovery-network-alarm-0', 'acquisition-recovery-network-alarm-1')
            counts['legacy'] += 1
            proof = read(root / 'acquisition-recovery-alarm-denial-result.json')
            assert proof['passed'] and proof['independent_alarm_preserved'] and proof['original_unknown_preserved']
            assert not any(r['phase'] == 'reconciled' for r in rows(root / 'controller/ecc.jsonl'))
            continue
        counts['ack'] += 1
        proof = read(root / 'alarm-ack-recovery-result.json')
        assert proof['passed'] and proof['encoding_minor'] == int(name[-1])
        assert proof['both_policies_rearmed'] and proof['new_alarm_stopped_new_run'] and proof['cold_replay_held']
        assert proof['recovery_hardware_writes'] == 0 and not proof['old_run_complete']
        op = read(root / 'operation-verified-alarm-acquisition.json')
        request, target = op['request'], op['result']
        assert op['phase'] == 'complete' and request['command'] == 'reconcile-acquisition-with-alarm'
        assert request['failed_operation_id'] == 'start' and request['alarm_operation_id'] == proof['original_alarm_id']
        assert target['state'] == 2 and target['operation_id'] == request['operation_id']
        assert not target['outcome_unknown'] and not target['needs_reconciliation']
        canonical = (root / 'controller/network/operations' / request['operation_id'] / 'request.json').read_bytes()
        assert json.loads(canonical) == request
        assert hashlib.sha256(b'getdaq.ecc-control-request:1\n' + canonical).hexdigest() == op['request_key']
        events = rows(root / 'controller/network/operations.jsonl')
        alarms = [r for r in events if r['event'] == 'alarm-held']
        receivers = [r for r in events if r['event'] == 'receiver-held']
        assert len(alarms) == 2 and len(receivers) == 1
        assert alarms[0]['operation_id'] == request['alarm_operation_id']
        completed = [r for r in events if r['event'] == 'complete' and r['operation_id'] == request['operation_id']]
        assert len(completed) == 1
        completed = completed[0]
        assert completed['alarm_policy_reset'] and completed['receiver_policy_reset']
        assert completed['alarm_stop_operation_id'] == request['alarm_operation_id']
        assert completed['receiver_stop_operation_id'] == receivers[0]['operation_id']
        assert completed['alarm_observation_sha256'] == hashlib.sha256(b'getdaq.ecc-alarm-observation:1\n' + alarms[0]['evidence'].encode()).hexdigest()
        assert json.loads(completed['result']) == target
        parent = rows(root / 'controller/ecc.jsonl')
        assert [r['phase'] for r in parent if r.get('id') == 'start'] == ['intent', 'complete', 'held']
        reconciled = [r for r in parent if r['phase'] == 'reconciled']
        assert len(reconciled) == 1 and reconciled[0]['id'] == request['operation_id']
        after = read(root / 'after-ack-alarm-policy.json')
        assert after['phase'] == 'watching' and not after['latched']
        second = read(root / 'second-alarm-held.json')
        assert second['latched'] and second['phase'] == 'complete'
        assert second['operation_id'] == alarms[1]['operation_id'] != alarms[0]['operation_id']
        cold = read(root / 'cold-alarm-recovery-denial.json')
        assert cold['phase'] == 'failed' and cold['request']['instance_id'] != request['instance_id']
        assert cold['result']['needs_reconciliation']
        cold_receiver = read(root / 'cold-receiver-failure.json')
        assert cold_receiver['latched'] and cold_receiver['phase'] == 'startup-reconciliation'
        assert cold_receiver['operation_id'] == '' and cold_receiver['evidence'] is None
        cold_alarm = read(root / 'cold-alarm-policy.json')
        assert cold_alarm['latched'] and cold_alarm['phase'] == 'reopened-held'
        assert cold_alarm['operation_id'] == second['operation_id']
    assert counts['ack'] == expected_ack
    print('PASS bound alarm acknowledgement, atomic dual-policy reset, new alarm stop and cold policy gates;', counts)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--expected-ack', type=int, choices=(1, 2), default=2, help='Expected explicitly selected acknowledgement scenarios')
    args = parser.parse_args()
    check(args.folder.resolve(), args.expected_ack)
