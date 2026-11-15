#!/usr/bin/env python3
"""Check cold DATA completion; do not infer an ECC or hardware recovery."""
import argparse
from pathlib import Path
from check_partial_handoff_completion_evidence import completion, read, rows, sha, digest
from check_cold_handoff_evidence import context


def check(folder):
    result = read(folder / 'result.json')
    assert result['passed'] and result['raw_remaining'] == 0 and not result['real_hardware']
    assert {c['case'] for c in result['cases']} == {'handoff-cold-retire-0', 'handoff-cold-retire-1', 'handoff-cold-control-0', 'handoff-cold-control-1'}
    assert not list(folder.rglob('*.graw'))
    cleanup = read(folder / 'cleanup.json')
    assert cleanup['deleted'] and sum(r['bytes'] for r in cleanup['deleted']) <= result['maximum_aggregate_test_bytes'] <= 20 * 1024**3
    for entry in result['cases']:
        case = folder / entry['case']
        proof = read(case / 'handoff-proof.json')
        assert all(proof[k] for k in ['passed', 'cold_data_completion', 'original_objects_destroyed',
                                     'native_journal_unchanged', 'original_manager_fenced', 'old_raw_retained'])
        assert not proof['raw_forwarded'] and not proof['production_cli_integrated']
        receipt_file = case / 'control-handoff-result.json'
        receipt = read(receipt_file)
        completion(case, receipt_file, case / 'controller/operations/cold-complete/data-handoff',
                   case / 'controller/operations/cold-ambiguous-404/data-handoff',proof['cold_missing_agent'])
        document, nodes = context(case, receipt_file)
        assert len(nodes) == 2 and document['original_epoch'] == receipt['original_epoch']
        native = rows(case / 'controller/ecc.jsonl')
        # These are direct data-adapter cycles: no native hardware transitions.
        assert len(native) == 1 and native[0]['phase'] == 'header' and native[0]['state'] == 1
        assert native[0]['scope'] == 'getdaq.native-ecc:1:' + sha(case / 'controller/snapshot.json')
        assert receipt['original_scope_sha256'] == digest('getdaq.data-controller:1\n' + sha(case / 'controller/snapshot.json') + '\n' + str((case / 'controller').resolve()))
        assert not (case / 'controller/operations/cold-while-live').exists()
        assert not (case / 'controller/operations/cold-empty-boundary').exists()
        assert not (case / 'controller/operations/cold-artifact-drift').exists()
        retirement = receipt['kind'] == 'getdaq.remote-plan-retirement'
        failed = case / 'controller/operations' / ('cold-missing-plan' if retirement else 'cold-ambiguous-404') / 'data-handoff'
        assert read(failed / 'intent.json')['requests'] == read(Path(receipt['audit_directory']) / 'intent.json')['requests']
        history = rows(failed / 'remote.jsonl')
        assert history[-1]['phase'] == 'unknown'
        assert [r['agent_id'] for r in history if r['phase'] == 'node_explicit_submit'] == ([proof['cold_missing_agent']] if retirement else [])
        rejected = [r for r in history if r['phase'] == 'explicit_rejection']
        if retirement:
            assert len(rejected) == 1 and rejected[0]['http_status'] in (409, 412)
            node1 = next(n for n in receipt['nodes'] if n['agent_id'] == proof['cold_missing_agent'])
            assert rejected[0]['operation_id'] == node1['operation_id']
            assert rejected[0]['request_key'] == node1['request_key']
            changed=rows(case/'controller/operations/cold-current-cycle/data-handoff/remote.jsonl')
            assert changed[-1]['phase']=='unknown' and not any(r['phase']=='node_explicit_submit' for r in changed)
        else:
            assert not rejected
    return len(result['cases'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    print('PASS cold data completion:', check(args.evidence.resolve()),
          'scenarios; no production CLI, native hardware recovery or real board acceptance')
