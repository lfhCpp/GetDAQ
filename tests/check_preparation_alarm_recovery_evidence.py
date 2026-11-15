#!/usr/bin/env python3
"""Independently bind explicit failed-Prepare alarm acknowledgement to durable evidence."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def check(folder):
    result = read(folder / 'result.json')
    assert result['passed'] and result['raw_remaining'] == 0
    assert read(folder / 'cleanup.json')['remaining_raw'] == []
    assert not list(folder.rglob('*.graw'))
    count = 0
    for run in result['runs']:
        name = run['scenario']
        if not name.startswith('alarm-preparation-recovery-'):
            continue
        count += 1
        root = folder / name
        proof = read(root / 'preparation-alarm-recovery-result.json')
        assert proof['passed'] and proof['encoding_minor'] == int(name[-1])
        assert proof['recovery_hardware_writes'] == 0 and proof['receiver_starts'] == 0
        assert proof['fresh_prepare_verified'] and proof['cold_replay_held']
        alarm_id = proof['original_alarm_id']
        ordinary = read(root / 'alarm-preparation-recovery-denied.json')
        assert ordinary['phase'] == 'failed' and ordinary['request']['command'] == 'reconcile-preparation'
        assert ordinary['result']['outcome_unknown'] and ordinary['result']['operation_id'] == 'prepare'
        journal = rows(root / 'controller/network/operations.jsonl')
        accepted = {row['operation_id']: row for row in journal if row['event'] == 'accepted'}
        requests = {key: json.loads(row['request']) for key, row in accepted.items()}
        for key, row in accepted.items():
            request_file = root / 'controller/network/operations' / key / 'request.json'
            assert request_file.read_text() == row['request']
            digest = hashlib.sha256(('getdaq.ecc-control-request:1\n' + row['request']).encode()).hexdigest()
            assert digest == row['request_key']
        request = requests['verified-preparation-alarm']
        assert request['command'] == 'reconcile-preparation-with-alarm'
        assert request['failed_operation_id'] == 'prepare' and request['alarm_operation_id'] == alarm_id
        complete = next(row for row in journal if row['event'] == 'complete' and row['operation_id'] == request['operation_id'])
        assert complete['alarm_policy_reset'] and complete['alarm_stop_operation_id'] == alarm_id
        assert 'receiver_policy_reset' not in complete and 'receiver_stop_operation_id' not in complete
        held = next(row for row in journal if row['event'] == 'alarm-held' and row['operation_id'] == alarm_id)
        assert complete['alarm_observation_sha256'] == hashlib.sha256(('getdaq.ecc-alarm-observation:1\n' + held['evidence']).encode()).hexdigest()
        state = json.loads(complete['result'])
        assert state['state'] == 2 and state['transition'] == 0 and state['operation_id'] == request['operation_id']
        assert not state['outcome_unknown'] and not state['needs_reconciliation']
        wrong = next(row for row in journal if row['event'] == 'failed' and row['operation_id'] == 'wrong-preparation-alarm')
        assert json.loads(wrong['result'])['outcome_unknown']
        native = rows(root / 'controller/ecc.jsonl')
        original = [row for row in native if row.get('id') == 'prepare']
        assert any(row['phase'] == 'unknown' for row in original)
        assert not any(row['phase'] == 'complete' for row in original)
        assert sum(row['phase'] == 'reconciled' for row in native) == 1
        rearmed = read(root / 'after-preparation-alarm-ack.json')
        assert not rearmed['latched'] and rearmed['phase'] == 'watching' and not rearmed['operation_id']
        cold = read(root / 'cold-preparation-alarm-denied.json')
        assert cold['phase'] == 'failed' and cold['request']['alarm_operation_id'] == alarm_id
        assert cold['request']['instance_id'] != request['instance_id']
    assert count
    print('PASS original failed Prepare, exact request hashes, alarm-only atomic reset, fresh Prepare and cold rejection;', count, 'scenarios')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    check(parser.parse_args().folder)
