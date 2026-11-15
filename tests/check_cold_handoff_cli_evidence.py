#!/usr/bin/env python3
"""Bind independent cold CLI runs to old parent history and new sealed cycles."""
import argparse
from pathlib import Path
from check_partial_handoff_completion_evidence import completion, read, rows, sha, digest
from check_cold_handoff_evidence import context


def check(folder, tool, guard, delivery_receipt=None):
    result = read(folder / 'result.json')
    assert result['passed'] and result['scope'] == 'network' and not result['real_hardware']
    assert not result['native_data_lifecycle_test_double'] and result['raw_remaining'] == 0
    assert not list(folder.rglob('*.graw'))
    cleanup = read(folder / 'cleanup.json')
    assert cleanup['deleted'] and sum(x['bytes'] for x in cleanup['deleted']) <= result['maximum_aggregate_test_bytes'] <= 20 * 1024**3
    if delivery_receipt:
        receipt = read(delivery_receipt)
        assert sha(tool) == receipt['programs']['getdaq-ecc-handoff']['sha256']
        for role, name in [('ecc', 'getdaq-ecc-server'), ('receiver', 'getdaq-receiver'), ('data_service', 'getdaq-supervisor')]:
            assert result['program_sha256'][role] == receipt['programs'][name]['sha256']
    prefixes=('plan-transition-cold-complete-','cold-handoff-service-')
    cold = [r for r in result['runs'] if r['scenario'].startswith(prefixes)]
    assert cold
    for prefix in prefixes:
        selected={r['scenario'] for r in cold if r['scenario'].startswith(prefix)}
        assert not selected or selected=={prefix+'0',prefix+'1'}
    for run in cold:
        case = folder / run['scenario']
        inputs = case / 'successor-inputs'
        programs = read(inputs / 'cold-cli-programs.json')
        assert programs['program_sha256'] == sha(tool) and programs['guard_sha256'] == sha(guard)
        assert programs['wrapper_sha256'] == result['program_sha256']['handoff_tool']
        network = inputs / 'cold-inspect-network.log'
        assert not network.exists() or network.stat().st_size == 0
        proof = read(case / 'cold-completion-result.json')
        assert all(proof[k] for k in ['passed', 'old_ecc_exited_before_completion', 'independent_cli_processes',
                                     'original_parent_failure_preserved', 'both_original_children_accepted_once',
                                     'native_network_audits_unchanged', 'lost_receipt_reexported_without_post'])
        assert not proof['old_hardware_authority_restored'] and not proof['raw_payload_forwarded']
        for name in ['cold-original-ecc-exit.json', 'completion-ecc-exit.json']:
            exited = read(case / name)
            assert exited['normal_exit'] and exited['exit_code'] == 0
        original = read(case / 'partial-retirement-operation.json')
        retirement=run['scenario'].startswith(prefixes[0])
        assert original['phase'] == 'failed' and original['request']['command'] == ('retire-data-plans' if retirement else 'handoff-data-control')
        assert read(case / 'handoff-operation.json') == original
        request = original['request']
        original_evidence = inputs / 'cold-original-evidence'
        external = inputs / 'cold-completion-evidence'
        for name in ['receipt.json']:
            assert (external / name).read_bytes() == (inputs / 'old-handoff.json').read_bytes()
        assert (external / 'original-request.json').read_bytes() == (original_evidence / 'request.json').read_bytes()
        assert (external / 'original-status.json').read_bytes() == (original_evidence / 'original-status.json').read_bytes()
        assert read(external / 'original-request.json') == request
        intent = read(external / 'cold-request.json')
        assert intent['schema'] == 1 and intent['kind'] == 'getdaq.cold-data-handoff-completion'
        assert intent['operation_id'] == 'cold-data-completion'
        assert intent['original_request_sha256'] == sha(original_evidence / 'request.json')
        assert intent['original_evidence_directory'] == str(original_evidence)
        assert intent['successor_plan_sha256'] == request['successor_plan_sha256']
        original_path = case / 'controller/operations' / request['operation_id'] / 'data-handoff'
        assert intent['original_audit_directory'] == str(original_path)
        assert intent['original_intent_sha256'] == sha(original_path / 'intent.json')
        completion(case, external / 'receipt.json', case / 'controller/operations/cold-data-completion/data-handoff',
                   case/'controller/operations/cold-uncertain/data-handoff')
        context(case, external / 'receipt.json')
        held = read(inputs / 'cold-missing-evidence/completion-held.json')
        assert held['completion_invoked_once'] and not held['automatic_retry'] and held['original_control_remains_fenced']
        failed = case / 'controller/operations' / ('cold-missing-plan' if retirement else 'cold-uncertain') / 'data-handoff'
        assert read(failed / 'intent.json')['requests'] == read(original_path / 'intent.json')['requests']
        failed_rows = rows(failed / 'remote.jsonl')
        assert failed_rows[-1]['phase'] == 'unknown'
        assert [r['agent_id'] for r in failed_rows if r['phase'] == 'node_explicit_submit'] == (['node-1'] if retirement else [])
        rejected=[r['http_status'] for r in failed_rows if r['phase'] == 'explicit_rejection']
        if retirement:assert rejected in ([409],[412])
        else:assert not rejected
        assert not (case / 'controller/operations/cold-while-live').exists()
        native_before = (case / 'cold-original-native.jsonl').read_bytes()
        network_before = (case / 'cold-original-network.jsonl').read_bytes()
        for file, before, field in [(case / 'controller/ecc.jsonl', native_before, 'phase'),
                                     (case / 'controller/network/operations.jsonl', network_before, 'event')]:
            current = file.read_bytes()
            assert current.startswith(before)
            # The fixture explicitly reopens the old ECC after the new cycle.
            # Cold submit/inspect themselves append neither native nor network opened.
            suffix = current[len(before):].decode()
            import json
            extra = [json.loads(line) for line in suffix.splitlines() if line]
            assert len(extra) == 1 and extra[0][field] == 'opened'
        parent_history = rows(case / 'controller/network/operations.jsonl')
        assert len([r for r in parent_history if r.get('operation_id') == request['operation_id'] and r['event'] == 'failed']) == 1
        assert not any(r.get('operation_id') == request['operation_id'] and r['event'] == 'complete' for r in parent_history)
        plan = read(inputs / 'successor.json')
        assert sha(inputs / 'successor.json') == request['successor_plan_sha256']
        next_root = Path(plan['audit_root'])
        next_root.resolve().relative_to(case.resolve())
        assert (next_root / 'controller-handoff.json').read_bytes() == (external / 'receipt.json').read_bytes()
        activation = read(Path(str(inputs / 'successor.json') + '.activation.json'))
        assert activation['plan_sha256'] == sha(inputs / 'successor.json')
        old = read(case / 'original-fenced-status.json')
        new = read(case / 'successor-new-cycle-status.json')
        assert old['control_needs_reconciliation'] and old['instance_id'] != new['instance_id']
        assert not new['control_needs_reconciliation'] and not new['data']['needs_reconciliation']
        assert new['data']['controller_scope_sha256'] == plan['data_controller_scope_sha256']
        assert new['data']['controller_epoch'] == plan['receiver_controller_epoch']
        old_nodes = {n['agent_id']: n for n in old['data']['nodes']}
        assert len(new['data']['nodes']) == len(old_nodes) == 2
        for node in new['data']['nodes']:
            previous = old_nodes[node['agent_id']]
            assert (node['instance_id'] != previous['instance_id']) == retirement
            assert (node['plan_sha256'] != previous['plan_sha256']) == retirement
            life = node['lifecycle']
            assert life['state'] == 'stopped' and not life['needs_reconciliation']
            assert life['session_id'] != previous['lifecycle']['session_id'] and life['receivers']
            assert all(r['reaped'] and r['seal_verified'] and r['exit_code'] == 0 for r in life['receivers'])
    markers = ('ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'AddressSanitizer:DEADLYSIGNAL',
               'LeakSanitizer has encountered a fatal error', 'SUMMARY: UndefinedBehaviorSanitizer',
               'runtime error:', 'ASan runtime does not come first', 'cannot be preloaded')
    for log in folder.rglob('*.log'):
        text = log.read_text(errors='replace')
        assert not any(marker in text for marker in markers), log
    return len(cold)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence', type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    parser.add_argument('--guard', required=True, type=Path)
    parser.add_argument('--delivery-receipt', type=Path)
    args = parser.parse_args()
    print('PASS cold CLI parent/child/new-cycle evidence:',
          check(args.evidence.resolve(), args.tool.resolve(), args.guard.resolve(), args.delivery_receipt),
          'scenarios; old parent remains failed and fenced; hardware recovery not claimed')
