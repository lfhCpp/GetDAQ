#!/usr/bin/env python3
"""Owned native/HTTP Describe recovery fixtures; explicit scope and raw cleanup."""
import argparse,json,signal,subprocess,tempfile,shutil,os
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

NATIVE=['description-recovery-'+kind+'-'+str(minor) for kind in ('success','denial','audit') for minor in (0,1)]
NETWORK=NATIVE+['description-recovery-qt-0','description-recovery-qt-1','normal-0','normal-1']
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('build',type=Path);ap.add_argument('receiver',type=Path);ap.add_argument('data_service',type=Path);ap.add_argument('evidence',type=Path)
    ap.add_argument('--scope',choices=('native','network'),required=True);ap.add_argument('--client-probe',type=Path);ap.add_argument('--scenarios',nargs='+');ap.add_argument('--ecc-service',type=Path)
    ap.add_argument('--handoff-tool',type=Path);ap.add_argument('--retirement-injector',type=Path);ap.add_argument('--recovery-service',type=Path);ap.add_argument('--recovery-window',type=Path)
    ap.add_argument('--sanitized',action='store_true',help='Enable ASan/UBSan/LeakSanitizer failure gates for instrumented programs')
    ap.add_argument('--handoff-reply-guard',type=Path);ap.add_argument('--asan-runtime',type=Path,help='Explicit libasan path for wrapped binaries or runtimes not discoverable through host ldd')
    a=ap.parse_args();scenarios=a.scenarios or (NATIVE if a.scope=='native' else NETWORK);runner_digest=sha(Path(__file__).resolve())
    if a.recovery_service and (a.scope!='network' or not a.handoff_tool):ap.error('Recovery service scenarios require network scope and actual handoff tool')
    if a.recovery_window and not a.recovery_service:ap.error('Recovery window requires the recovery service')
    if a.handoff_reply_guard and (not a.handoff_tool or (a.sanitized and not a.asan_runtime)):
        ap.error('Reply guard requires the handoff tool and, for ASan, --asan-runtime')
    if any(x.startswith('plan-transition-failed-node-') for x in scenarios) and not a.retirement_injector:
        ap.error('Accepted failed-node scenarios require --retirement-injector')
    if any('-qt-' in x or '-window-' in x or x.startswith(("qt-source-recovery-","qt-failure-breakup-","qt-receiver-retirement-","qt-acquisition-unprepare-","qt-acquisition-recovery-","qt-alarm-acquisition-recovery-","qt-stopped-alarm-release-")) for x in scenarios) and not a.client_probe:ap.error('Qt scenarios require --client-probe')
    a.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='description-recovery-',dir=a.evidence)).resolve();root=Path(__file__).resolve().parents[1]
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    build=a.build.resolve();programs={'fixture':build/('native-ecc-coordinator-tests' if a.scope=='native' else 'ecc-network-service-tests')}
    if a.scope=='network':programs.update(ecc=a.ecc_service.resolve() if a.ecc_service else build/'getdaq-ecc-server',receiver=a.receiver.resolve(),data_service=a.data_service.resolve())
    if a.client_probe:programs['client_probe']=a.client_probe.resolve()
    if a.handoff_tool:programs['handoff_tool']=a.handoff_tool.resolve(strict=True)
    if a.retirement_injector:programs['retirement_injector']=a.retirement_injector.resolve(strict=True)
    if a.handoff_reply_guard:
        from ecc_handoff_reply_wrapper import create
        programs['handoff_reply_guard']=a.handoff_reply_guard.resolve(strict=True)
        programs['handoff_tool_wrapper']=folder/'handoff-tool-wrapper.py'
        create(programs['handoff_tool_wrapper'],programs['handoff_tool'],programs['handoff_reply_guard'],folder/'handoff-reply-events.log',a.asan_runtime)
    if a.recovery_service:programs['recovery_service']=a.recovery_service.resolve(strict=True)
    if a.recovery_window:programs['recovery_window']=a.recovery_window.resolve(strict=True)
    before={k:sha(v) for k,v in programs.items()};process=None;rows=[]
    temp=folder/"tmp";temp.mkdir(mode=0o700);runtime=folder/"runtime";runtime.mkdir(mode=0o700);data=folder/"data";data.mkdir(mode=0o700)
    env=dict(os.environ,QT_QPA_PLATFORM="offscreen",TMPDIR=str(temp),XDG_RUNTIME_DIR=str(runtime),XDG_DATA_HOME=str(data))
    if a.handoff_tool:env['GETDAQ_TEST_ECC_HANDOFF_TOOL']=str(programs.get('handoff_tool_wrapper',programs['handoff_tool']))
    if a.retirement_injector:
        env['GETDAQ_TEST_RETIREMENT_INJECTOR']=str(programs['retirement_injector'])
        if a.sanitized:
            if a.asan_runtime:
                asan_runtime=a.asan_runtime.resolve(strict=True)
                if not asan_runtime.is_file():ap.error('--asan-runtime must be a shared-library file')
                env['GETDAQ_TEST_ASAN_RUNTIME']=str(asan_runtime)
            else:
                linked=subprocess.check_output(['ldd',str(programs['data_service'])],text=True)
                env['GETDAQ_TEST_ASAN_RUNTIME']=next(line.split('=>')[1].strip().split()[0] for line in linked.splitlines() if 'libasan.so' in line)
    if a.recovery_service:env['GETDAQ_TEST_COLD_RECOVERY_SERVICE']=str(programs['recovery_service'])
    if a.recovery_window:env['GETDAQ_TEST_COLD_RECOVERY_WINDOW']=str(programs['recovery_window'])
    if a.sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    try:
        for scenario in scenarios:
            if a.scope=='native':
                index=folder/(scenario+'.index');command=[str(programs['fixture']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(index),'--scenario',scenario]
            else:
                command=[str(programs['fixture']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(programs['receiver']),str(programs['data_service']),str(programs['ecc']),str(folder/scenario),'--scenario',scenario]
                if '-qt-' in scenario or '-window-' in scenario or scenario.startswith(('qt-source-recovery-','qt-failure-breakup-','qt-receiver-retirement-','qt-acquisition-unprepare-','qt-acquisition-recovery-','qt-alarm-acquisition-recovery-','qt-stopped-alarm-release-')):command.append(str(programs['client_probe']))
            with (folder/(scenario+'.log')).open('w') as log:
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True,env=env);code=process.wait(timeout=240)
            assert code==0,(folder/(scenario+'.log')).read_text()[-3000:];stop_owned_group(process);process=None
            if a.scope=='native':
                marker=json.loads(Path(str(index)+'.root.json').read_text());native=Path(marker['root']);parent=native.parent
                assert parent.parent==Path('/tmp') and parent.name.startswith('getdaq-native-ecc-') and native.name=='controller'
                assert not list(parent.rglob('*.graw'));shutil.copytree(native,folder/(scenario+'-native-audit'));shutil.rmtree(parent)
            else:
                if scenario.startswith('cold-handoff-service-'):
                    case=folder/scenario;cold=json.loads((case/'cold-completion-result.json').read_text())
                    assert all(cold[k] for k in ['passed','old_ecc_exited_before_completion','independent_cli_processes','original_parent_failure_preserved','both_original_children_accepted_once','native_network_audits_unchanged','lost_receipt_reexported_without_post'])
                    assert not cold['old_hardware_authority_restored'] and not cold['raw_payload_forwarded']
                    old=json.loads((case/'original-fenced-status.json').read_text());new=json.loads((case/'successor-new-cycle-status.json').read_text())
                    assert old['instance_id']!=new['instance_id'] and not new['control_needs_reconciliation']
                    old_nodes={n['agent_id']:n for n in old['data']['nodes']}
                    for node in new['data']['nodes']:
                        previous=old_nodes[node['agent_id']]
                        assert node['instance_id']==previous['instance_id'] and node['plan_sha256']==previous['plan_sha256']
                        life=node['lifecycle'];assert life['state']=='stopped' and life['session_id']!=previous['lifecycle']['session_id']
                        assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
                if scenario.startswith('plan-transition-'):
                    case=folder/scenario
                    if scenario=='plan-transition-partial-0':
                        proof=json.loads((case/'partial-plan-result.json').read_text())
                        assert proof['passed'] and proof['first_node_retired'] and proof['second_node_not_accepted'] and proof['cold_original_fenced'] and not proof['automatic_retry']
                    elif scenario.startswith('plan-transition-failed-node-'):
                        completed=json.loads((case/'partial-completion-result.json').read_text())
                        original=json.loads((case/'partial-retirement-operation.json').read_text())
                        completion=json.loads((case/'completion-operation.json').read_text())
                        assert completed['passed'] and completed['original_failure_retained'] and completed['both_original_children_accepted_once']
                        assert completed['old_controller_still_fenced'] and completed['no_completion_hardware_writes'] and not completed['operator_cli']
                        assert original['phase']=='failed' and original['request']['command']=='retire-data-plans'
                        assert completion['phase']=='complete' and completion['request']['command']=='complete-data-handoff'
                        assert completion['request']['failed_operation_id']==original['request']['operation_id']
                        exited=json.loads((case/'completion-ecc-exit.json').read_text())
                        assert exited['normal_exit'] and exited['exit_code']==0
                        post_repair=json.loads((case/'post-repair-fenced-status.json').read_text())
                        before_repair=json.loads((case/'original-fenced-status.json').read_text())
                        assert post_repair['instance_id']==before_repair['instance_id'] and post_repair['control_needs_reconciliation']
                        if scenario.startswith('plan-transition-failed-node-window-'):
                            window=json.loads((case/'gui-switch-evidence/window-proof.json').read_text())
                            repair=json.loads((case/'gui-node-repair-proof.json').read_text())
                            assert completed['operator_window'] and completed['explicit_node_repair']
                            assert window['passed'] and window['operator_node_repair_confirmed']
                            assert window['completion_cancel_default'] and window['completion_confirmed_submissions']==1
                            assert window['completion_observation_signals']==1
                            assert repair['passed'] and repair['original_child_failure_retained'] and repair['parent_was_not_repaired'] and repair['exact_reviewed_plan_restored']
                            assert (case/'gui-node-repair-request').is_file() and (case/'gui-node-repair-complete').is_file()
                            assert not (case/'gui-node-repair-failed').exists()
                    else:
                        proof=json.loads((case/'service-handoff-result.json').read_text())
                        assert proof['passed'] and proof['two_independent_ecc_processes'] and proof['cold_original_fenced']
                        assert proof['consumed_plan_restart_refused'] and proof['handoff_hardware_writes']==0
                        if scenario.startswith('plan-transition-cold-complete-'):
                            cold=json.loads((case/'cold-completion-result.json').read_text())
                            assert all(cold[k] for k in ['passed','old_ecc_exited_before_completion','independent_cli_processes','original_parent_failure_preserved','both_original_children_accepted_once','native_network_audits_unchanged','lost_receipt_reexported_without_post'])
                            assert not cold['old_hardware_authority_restored'] and not cold['raw_payload_forwarded']
                            assert json.loads((case/'partial-retirement-operation.json').read_text())['phase']=='failed'
                            exited=json.loads((case/'completion-ecc-exit.json').read_text())
                            assert exited['normal_exit'] and exited['exit_code']==0
                        if scenario.startswith('plan-transition-complete-'):
                            completed=json.loads((case/'partial-completion-result.json').read_text())
                            assert completed['passed'] and completed['original_failure_retained'] and completed['both_original_children_accepted_once']
                            assert completed['old_controller_still_fenced'] and completed['no_completion_hardware_writes']
                            assert completed['operator_cli']==scenario.endswith('-1')
                            original=json.loads((case/'partial-retirement-operation.json').read_text())
                            completion=json.loads((case/'completion-operation.json').read_text())
                            assert original['phase']=='failed' and original['request']['command']=='retire-data-plans'
                            assert completion['phase']=='complete' and completion['request']['command']=='complete-data-handoff'
                            assert completion['request']['failed_operation_id']==original['request']['operation_id']
                            exited=json.loads((case/'completion-ecc-exit.json').read_text())
                            assert exited['normal_exit'] and exited['exit_code']==0
                        old=json.loads((case/'original-fenced-status.json').read_text());new=json.loads((case/'successor-new-cycle-status.json').read_text())
                        assert old['instance_id']!=new['instance_id'] and not new['control_needs_reconciliation']
                        old_nodes={node['agent_id']:node for node in old['data']['nodes']}
                        for node in new['data']['nodes']:
                            previous_node=old_nodes[node['agent_id']]
                            assert previous_node['instance_id']!=node['instance_id'] and previous_node['plan_sha256']!=node['plan_sha256']
                            assert previous_node['lifecycle']['session_id']!=node['lifecycle']['session_id']
                            assert node['lifecycle']['state']=='stopped' and all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in node['lifecycle']['receivers'])
                if scenario.startswith('handoff-service-'):
                    case=folder/scenario;proof=json.loads((case/'service-handoff-result.json').read_text())
                    assert proof['passed'] and proof['two_independent_ecc_processes'] and proof['consumed_plan_restart_refused']
                    assert proof['cold_original_fenced'] and proof['operator_cli']==(bool(a.handoff_tool) and scenario.endswith('-1'))
                    if a.handoff_reply_guard and scenario.endswith('-1'):
                        assert (folder/'handoff-reply-events.log').read_bytes()==b'post\ndrop\n'
                        locks=json.loads((folder/'handoff-reply-events.log.locks.json').read_text())
                        assert locks['exclusive_activation_rejected'] and locks['shared_handoff_blocks_activation']
                        assert (case/'successor-inputs/tool-evidence/submission-held.json').is_file()
                    assert proof['handoff_hardware_writes']==0 and proof['encoding_minor']==int(scenario[-1])
                    plan_path=case/'successor-inputs/successor.json';plan=json.loads(plan_path.read_text())
                    marker=json.loads(Path(str(plan_path)+'.activation.json').read_text())
                    status=json.loads((case/'successor-startup-status.json').read_text())
                    old=json.loads((case/'original-fenced-status.json').read_text())
                    receipt=json.loads((case/'successor-inputs/old-handoff.json').read_text())
                    op=json.loads((case/'handoff-operation.json').read_text());request=op['request']
                    assert op['phase']=='complete' and request['command']=='handoff-data-control'
                    assert request['successor_plan_sha256']==sha(plan_path)==marker['plan_sha256']
                    assert status['instance_id']!=old['instance_id']==request['instance_id']
                    assert request['successor_scope_sha256']==receipt['successor_scope_sha256']==plan['data_controller_scope_sha256']==status['data']['controller_scope_sha256']
                    assert request['successor_epoch']==receipt['successor_epoch']==plan['receiver_controller_epoch']==status['data']['controller_epoch']
                    assert request['successor_data_binding_sha256']==plan['data_lifecycle_binding_sha256']
                    assert old['control_needs_reconciliation'] and not status['control_needs_reconciliation'] and not status['data']['needs_reconciliation']
                    assert len(status['data']['nodes'])==2
                    for node in status['data']['nodes']:
                        life=node['lifecycle'];assert life['state']=='stopped' and life['receivers']
                        assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
                if scenario.startswith('qt-source-recovery-'):
                    proof=json.loads((folder/scenario/'source-recovery-window-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==int(scenario[-1])
                    assert proof['real_main_window_and_runcontrol'] and proof['original_run_interrupted'] and proof['old_data_unsealed'] and proof['fresh_prepare_verified']
                if scenario.startswith('ice-source-recovery-'):
                    proof=json.loads((folder/scenario/'ice-source-recovery-network-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==int(scenario[-1])
                    assert proof['old_source_unknown_retained'] and proof['old_data_unsealed'] and proof['recovery_hardware_writes']==0
                    assert proof['fresh_cycle_bytes_and_seals_verified'] and proof['cold_replay_held']
                if scenario.startswith('qt-stopped-alarm-release-'):
                    proof=json.loads((folder/scenario/'stopped-alarm-window-result.json').read_text())
                    assert proof['passed'] and proof['real_main_window_and_runcontrol'] and proof['original_run_interrupted']
                    assert proof['normal_receiver_finish_verified'] and proof['fresh_prepare_verified']
                    assert proof['encoding_minor']==int(scenario[-1])
                if scenario.startswith(('qt-source-recovery-','qt-failure-breakup-','qt-receiver-retirement-','qt-acquisition-unprepare-','qt-acquisition-recovery-','qt-alarm-acquisition-recovery-')):
                    proof=json.loads((folder/scenario/'failure-breakup-window-result.json').read_text())
                    assert proof['passed'] and proof['real_main_window_and_runcontrol'] and proof['run_incomplete']
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['cancel_requests']==0 and proof['confirmed_requests']==1
                if scenario.startswith('qt-receiver-retirement-'):
                    proof=json.loads((folder/scenario/'receiver-retirement-window-result.json').read_text())
                    assert proof['passed'] and proof['parent_unknown_preserved'] and proof['run_incomplete']
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['cancel_requests']==0 and proof['confirmed_requests']==1
                if scenario.startswith(('qt-acquisition-unprepare-','qt-acquisition-recovery-','qt-alarm-acquisition-recovery-')):
                    proof=json.loads((folder/scenario/'acquisition-unprepare-window-result.json').read_text())
                    assert proof['passed'] and proof['parent_unknown_preserved'] and proof['run_incomplete']
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['cancel_requests']==0 and proof['confirmed_requests']==1
                if scenario.startswith('qt-alarm-acquisition-recovery-'):
                    proof=json.loads((folder/scenario/'alarm-acquisition-recovery-window-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==int(scenario[-1]) and proof['original_alarm_acknowledged']
                    assert proof['real_main_window_and_runcontrol'] and proof['run_incomplete'] and proof['fresh_prepare_verified']
                    assert proof['cancel_requests']==0 and proof['confirmed_requests']==1
                if scenario.startswith('qt-acquisition-recovery-'):
                    proof=json.loads((folder/scenario/'acquisition-recovery-window-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['real_main_window_and_runcontrol'] and proof['run_incomplete'] and proof['fresh_prepare_verified']
                    assert proof['cancel_requests']==0 and proof['confirmed_requests']==1
                if scenario.startswith('acquisition-recovery-network-alarm-ack-'):
                    proof=json.loads((folder/scenario/'alarm-ack-recovery-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==int(scenario[-1]) and proof['recovery_hardware_writes']==0
                    assert proof['both_policies_rearmed'] and proof['new_alarm_stopped_new_run'] and proof['cold_replay_held'] and not proof['old_run_complete']
                if scenario.startswith('acquisition-recovery-network-alarm-'):
                    proof=json.loads((folder/scenario/'acquisition-recovery-alarm-denial-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==int(scenario[-1])
                    assert proof['independent_alarm_preserved'] and proof['original_unknown_preserved'] and proof['original_data_retired']
                    assert proof['recovery_hardware_writes']==0 and proof['denied_recovery_attempts']==2
                if scenario.startswith('acquisition-recovery-network-') and '-alarm-' not in scenario:
                    proof=json.loads((folder/scenario/'acquisition-recovery-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['recovery_hardware_writes']==0 and proof['receiver_policy_rearmed']
                    assert proof['fresh_prepare_verified'] and proof['second_failure_stopped_new_run'] and not proof['original_run_complete']
                if scenario.startswith('acquisition-unprepare-network-'):
                    proof=json.loads((folder/scenario/'acquisition-unprepare-result.json').read_text())
                    assert proof['passed'] and proof['parent_unknown_preserved'] and proof['data_retired_preserved'] and proof['all_power_off_observed']
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                if scenario.startswith('receiver-retirement-network-'):
                    proof=json.loads((folder/scenario/'receiver-retirement-result.json').read_text())
                    assert proof['passed'] and proof['parent_unknown_preserved'] and proof['data_retired']
                    assert not proof['receivers_restarted'] and proof['hardware_actions']==0
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                if scenario.startswith('failure-breakup-network-'):
                    proof=json.loads((folder/scenario/'failure-breakup-result.json').read_text())
                    assert proof['passed'] and proof['unknown_preserved'] and proof['receiver_lifecycle_unchanged']
                    assert proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                if scenario.startswith(('preparation-recovery-window-','preparation-alarm-recovery-window-')):
                    proof=json.loads((folder/scenario/'preparation-window-result.json').read_text())
                    assert proof['passed'] and proof['encoding_minor']==(0 if scenario.endswith('-0') else 1)
                    assert proof['real_main_window_and_runcontrol'] and proof['no_receivers_started']
                    assert proof['accepted_cleanup_requests']==1 and proof['accepted_recovery_requests']==1
                if scenario.startswith('description-recovery-'):
                    assert json.loads((folder/scenario/'description-recovery-result.json').read_text())['passed']
                if scenario.startswith('prepare-cleanup-'):
                    proof=json.loads((folder/scenario/'failure-cleanup-result.json').read_text())
                    assert proof['passed'] and proof['unknown_preserved'] and proof['no_receivers_started']
                    assert proof['encoding_minor']==(0 if scenario.endswith("-0") else 1)
                    if scenario.startswith("prepare-cleanup-recovery-"):
                        recovery=json.loads((folder/scenario/'preparation-recovery-result.json').read_text())
                        assert recovery['passed'] and recovery['no_recovery_hardware_writes']
                    scope=json.loads((folder/scenario/'scope.json').read_text())
                    assert scope['real_ecc_network_service'] and scope['real_data_services']==2
                    assert not scope['real_receivers'] and not scope['full_ecc_workflow_tested']
            rows.append({'scenario':scenario,'exit_code':code})
    finally:
        stop_owned_group(process);deleted=[]
        for p in folder.rglob('*.graw'):
            if p.is_file() and not p.is_symlink():deleted.append({'path':str(p.relative_to(folder)),'bytes':p.stat().st_size});p.unlink()
        for name in ('credential','ecc-credential','recovery-credential'):
            for p in folder.rglob(name):
                if p.is_file() and not p.is_symlink():p.unlink()
        remaining=[str(p.relative_to(folder)) for p in folder.rglob('*.graw')];(folder/'cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':remaining},indent=2)+'\n')
    assert not remaining;assert all(sha(programs[k])==h for k,h in before.items())
    if a.sanitized:
        markers=('ERROR: AddressSanitizer','ERROR: LeakSanitizer','AddressSanitizer:DEADLYSIGNAL',
                 'SUMMARY: UndefinedBehaviorSanitizer','LeakSanitizer has encountered a fatal error','runtime error:')
        for log in folder.rglob('*.log'):
            text=log.read_text(errors='replace')
            assert not any(marker in text for marker in markers),'Sanitizer failure, including an expected-negative child: '+str(log)
    assert sha(Path(__file__).resolve())==runner_digest,'Runner source changed during validation'
    (folder/'result.json').write_text(json.dumps({'passed':True,'scope':a.scope,'runs':rows,'program_sha256':before,'real_hardware':False,'native_data_lifecycle_test_double':a.scope=='native','maximum_aggregate_test_bytes':512*1024**2 if a.scope=='network' else 0,'raw_remaining':0,'runner_sha256':runner_digest,'sanitized':a.sanitized,'leak_detection':a.sanitized},indent=2)+'\n')
    print('PASS selected ECC recovery/cleanup scenarios '+a.scope+'; own raw removed; '+str(folder))
if __name__=='__main__':main()
