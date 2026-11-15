#!/usr/bin/env python3
"""Independently check durable acquisition recovery and a second receiver stop."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def check(folder):
    result = read(folder / 'result.json')
    assert result['passed'] and result['scope'] == 'network' and result['raw_remaining'] == 0
    assert read(folder / 'cleanup.json')['remaining_raw'] == []
    assert not list(folder.rglob('*.graw'))
    for run in result['runs']:
        scenario = run['scenario']
        assert scenario in ('acquisition-recovery-network-0', 'acquisition-recovery-network-1')
        root = folder / scenario
        proof = read(root / 'acquisition-recovery-result.json')
        assert proof['passed'] and proof['encoding_minor'] == int(scenario[-1])
        assert proof['recovery_hardware_writes'] == 0 and proof['second_failure_stopped_new_run']
        assert not proof['original_run_complete']
        op = read(root / 'operation-verified-acquisition.json')
        request, state = op['request'], op['result']
        assert op['phase'] == 'complete' and request['command'] == 'reconcile-acquisition'
        assert request['failed_operation_id'] == 'start'
        assert state['state'] == 2 and state['operation_id'] == request['operation_id']
        assert not state['outcome_unknown'] and not state['needs_reconciliation']
        rows = [json.loads(line) for line in (root / 'controller/ecc.jsonl').read_text().splitlines()]
        start = [r for r in rows if r.get('id') == 'start']
        assert [r['phase'] for r in start] == ['intent', 'complete', 'held']
        assert any(r['phase'] == 'reconciled' and r['id'] == request['operation_id'] and r['to'] == 2 for r in rows)
        network = [json.loads(line) for line in (root / 'controller/network/operations.jsonl').read_text().splitlines()]
        recovered = [r for r in network if r['event'] == 'complete' and r['operation_id'] == request['operation_id']]
        assert len(recovered) == 1 and recovered[0]['receiver_policy_reset']
        assert json.loads(recovered[0]['result']) == state
        canonical = (root / 'controller/network/operations/verified-acquisition/request.json').read_bytes()
        assert json.loads(canonical) == request
        assert canonical == (json.dumps(request, ensure_ascii=False, separators=(',', ':')) + '\n').encode()
        assert op['request_key'] == hashlib.sha256(b'getdaq.ecc-control-request:1\n' + canonical).hexdigest()
        held = [r for r in network if r['event'] == 'receiver-held']
        assert len(held) == 2 and held[0]['operation_id'] == recovered[0]['receiver_stop_operation_id']
        assert held[0]['operation_id'] != held[1]['operation_id']
        for index, original in enumerate(('start', 'fresh-start')):
            bound = json.loads(json.loads(held[index]['evidence'])['bound_status'])
            assert bound['controller']['operation_id'] == original
            stops = [r for r in network if r['event'] == 'complete' and r['operation_id'] == held[index]['operation_id']]
            assert len(stops) == 1
            stop = json.loads(stops[0]['result'])
            assert stop['outcome_unknown'] and stop['operation_id'] == original
        final = read(root / 'second-receiver-failure.json')
        assert final['latched'] and final['phase'] == 'complete' and final['operation_id'] == held[1]['operation_id']
    print('PASS durable original Start/held/reconciled history, atomic policy reset, two bound stops and raw cleanup')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    check(parser.parse_args().folder.resolve())
