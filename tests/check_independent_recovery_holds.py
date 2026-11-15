#!/usr/bin/env python3
"""Check durable alarm denial and the separate ICE source UNKNOWN rejection path."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def alarm(folder):
    result = read(folder / 'result.json')
    assert result['passed'] and result['raw_remaining'] == 0
    assert read(folder / 'cleanup.json')['remaining_raw'] == []
    assert len(result['runs']) == 2
    for run in result['runs']:
        scenario = run['scenario']
        assert scenario in ('acquisition-recovery-network-alarm-0', 'acquisition-recovery-network-alarm-1')
        root = folder / scenario
        proof = read(root / 'acquisition-recovery-alarm-denial-result.json')
        assert proof['passed'] and proof['encoding_minor'] == int(scenario[-1])
        before, after = read(root / 'independent-alarm-held.json'), read(root / 'independent-alarm-final.json')
        assert before['latched'] and after['latched'] and before['operation_id'] == after['operation_id']
        network = rows(root / 'controller/network/operations.jsonl')
        alarms = [r for r in network if r['event'] == 'alarm-held']
        receivers = [r for r in network if r['event'] == 'receiver-held']
        assert len(alarms) == len(receivers) == 1
        assert alarms[0]['operation_id'] == after['operation_id'] != receivers[0]['operation_id']
        for held in (alarms[0], receivers[0]):
            completions = [r for r in network if r['event'] == 'complete' and r['operation_id'] == held['operation_id']]
            assert len(completions) == 1
            state = json.loads(completions[0]['result'])
            assert state['operation_id'] == 'start' and state['outcome_unknown']
        parent = rows(root / 'controller/ecc.jsonl')
        assert [r['phase'] for r in parent if r.get('id') == 'start'] == ['intent', 'complete', 'held']
        assert not any(r['phase'] == 'reconciled' for r in parent)
        for ident in ('denied-recovery', 'denied-recovery-again'):
            op = read(root / ('operation-' + ident + '.json'))
            assert op['phase'] == 'failed' and 'independent alarm/input/storage hold' in op['message']
            assert op['request']['command'] == 'reconcile-acquisition' and op['request']['failed_operation_id'] == 'start'
            assert op['result']['outcome_unknown'] and op['result']['operation_id'] == 'start'
            request = (root / 'controller/network/operations' / ident / 'request.json').read_bytes()
            assert json.loads(request) == op['request']
            assert hashlib.sha256(b'getdaq.ecc-control-request:1\n' + request).hexdigest() == op['request_key']
            complete = [r for r in network if r.get('operation_id') == ident and r['event'] == 'failed']
            assert len(complete) == 1 and json.loads(complete[0]['result']) == op['result']
            assert not (root / 'controller/operations' / ident).exists()
        for command in ('breakup-after-failure', 'abort-failed-receivers', 'retire-failed-receivers', 'unprepare-acquisition-after-failure'):
            # The fixture includes explicit denied repeat attempts; require the confirmed original command too.
            ids = [json.loads(r['request'])['operation_id'] for r in network if r['event'] == 'accepted' and json.loads(r['request'])['command'] == command]
            assert any(r['event'] == 'complete' and r['operation_id'] in ids for r in network), command
        final = read(root / 'final-status.json')
        assert final['controller']['operation_id'] == 'start' and final['controller']['outcome_unknown']
        assert final['control_needs_reconciliation'] and not final['data']['needs_reconciliation']
        assert len(final['data']['nodes']) == 2
        for node in final['data']['nodes']:
            status = node['lifecycle']
            assert status['state'] == 'retired' and all(c['reaped'] and not c['seal_verified'] for c in status['receivers'])


def source(folder):
    result = read(folder / 'result.json')
    assert result['raw_deleted'] and len(result['cases']) == 2
    assert read(folder / 'payload-cleanup.json')['remaining_raw'] == 0
    for case in result['cases']:
        minor = case['encoding']
        assert case['passed'] and minor in (0, 1)
        root = folder / str(minor)
        proof = read(root / 'source-recovery-denial-proof.json')
        assert proof['passed'] and proof['encoding_minor'] == minor and proof['denied_recovery_attempts'] == 2
        assert proof['unknown_source_disconnect_rejected'] and proof['complete_retirement_not_reached']
        assert proof['original_start_complete'] and proof['original_receivers_retained'] and proof['recovery_hardware_writes'] == 0
        observed = read(root / 'sources.json')['sources']
        assert len(observed) == 2 and all(s['state'] == 'unknown' and s['failure_stop_acknowledged'] and not s['source_end_acknowledged'] for s in observed)
        parent = rows(root / 'controller/ecc.jsonl')
        assert [r['phase'] for r in parent if r.get('id') == 'start'] == ['intent', 'complete', 'held']
        assert not any(r['phase'] == 'reconciled' for r in parent)
        assert not list(root.rglob('.getdaq-ice-receiver-seal.json'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('alarm', 'source'))
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    folder = args.folder.resolve()
    assert not list(folder.rglob('*.graw'))
    (alarm if args.kind == 'alarm' else source)(folder)
    print('PASS independent ' + args.kind + ' hold evidence; no recovery commit and raw zero')
