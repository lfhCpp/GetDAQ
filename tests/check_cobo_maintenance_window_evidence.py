#!/usr/bin/env python3
"""Check actual GUI intent against HTTP and native maintenance, then sealed receivers."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from check_cobo_maintenance_evidence import maintenance


def read(path):return json.loads(path.read_text())
def rows(path):return [json.loads(line) for line in path.read_text().splitlines()]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def digest(value):return hashlib.sha256(value.encode()).hexdigest()


def check(folder):
    clean=read(folder/'cleanup.json')
    if (folder/'result.json').exists():
        record=read(folder/'result.json')
        assert record['passed'] and record['scope']=='network'
        assert not record['real_hardware'] and not record['native_data_lifecycle_test_double']
        assert record['maximum_aggregate_test_bytes']<=20*1024**3
        assert record['raw_remaining']==0
        assert not clean['remaining_raw'] and clean['deleted']
        assert sum(entry['bytes'] for entry in clean['deleted'])<=record['maximum_aggregate_test_bytes']
    else:
        # The native runner writes results.json/cleanup.json directly. Accept that
        # authoritative layout too; do not require an archive wrapper to re-check it.
        runs=read(folder/'results.json')
        assert runs and all(run['exit_code']==0 for run in runs)
        assert not clean['remaining_raw_files'] and not clean['cleanup_errors']
        assert not clean['limit_exceeded'] and clean['generated_raw_bytes']<=clean['limit_bytes']<=20*1024**3
        record={'passed':True,'scope':'network','real_hardware':False,'native_data_lifecycle_test_double':False,
                'maximum_aggregate_test_bytes':clean['limit_bytes'],'raw_remaining':0,'runs':runs,'source_evidence_root':str(folder)}
    assert not list(folder.rglob('*.graw'))
    summaries=[]
    for run in record['runs']:
        assert run['exit_code']==0 and run['scenario'] in ('cobo-maintenance-window-0','cobo-maintenance-window-1')
        case=folder/run['scenario'];proof=read(case/'gui-maintenance-evidence/window-proof.json')
        source_root=Path(run.get('source_evidence_root',record.get('source_evidence_root',str(folder))))
        source_case=source_root/run['scenario']
        assert proof['passed'] and proof['actual_main_window_and_runcontrol'] and proof['cancel_default']
        assert proof['cancel_submissions']==proof['cancel_intents']==0 and proof['confirmed_submissions']==1
        assert proof['confirmed_led_submissions']==1 and (proof['led_asad_id'],proof['led_type'],proof['led_state'])==(3,2,3)
        assert proof['confirmed_pulser_submissions']==1 and proof['pulser_voltage_mv']==321
        assert proof['confirmed_blink_submissions']==1 and proof['confirmed_flip_submissions']==1
        assert proof['confirmed_pulse_submissions']==1 and (proof['pulse_led_id'],proof['pulse_period_ms'])==(10,725)
        assert proof['flip_led_id']==10
        assert proof['busy_control_buttons_disabled'] and proof['complete_known_prepared']
        assert not proof['raw_transferred_by_gui'] and proof['raw_read_guard_selfcheck']
        assert (case/'gui-raw-reads.log').read_bytes()==b'raw-read-denied\n'
        programs=read(case/'gui-programs.json')
        for name in ('probe','guard'):assert sha(Path(programs[name]))==programs[name+'_sha256']
        request_file=case/'gui-maintenance-evidence/request.json';request=read(request_file)
        assert request['command']=='reprepare-cobo' and request['cobo_id']=='C0'
        led_request_file=case/'gui-maintenance-evidence/led-request.json';led_request=read(led_request_file)
        assert led_request['command']=='set-cobo-led' and led_request['cobo_id']=='C0'
        assert (led_request['asad_id'],led_request['led_type'],led_request['led_state'])==(3,2,3)
        assert led_request['instance_id']==request['instance_id'] and led_request['snapshot_sha256']==request['snapshot_sha256']
        blink_request_file=case/'gui-maintenance-evidence/blink-request.json';blink_request=read(blink_request_file)
        assert blink_request['command']=='blink-cobo-leds' and blink_request['cobo_id']=='C0'
        assert blink_request['instance_id']==request['instance_id'] and blink_request['snapshot_sha256']==request['snapshot_sha256']
        flip_request_file=case/'gui-maintenance-evidence/flip-request.json';flip_request=read(flip_request_file)
        assert flip_request['command']=='flip-cobo-led' and flip_request['cobo_id']=='C0' and flip_request['led_id']==10
        assert flip_request['instance_id']==request['instance_id'] and flip_request['snapshot_sha256']==request['snapshot_sha256']
        pulse_request_file=case/'gui-maintenance-evidence/pulse-request.json';pulse_request=read(pulse_request_file)
        assert pulse_request['command']=='pulse-cobo-led' and pulse_request['cobo_id']=='C0'
        assert (pulse_request['led_id'],pulse_request['period_ms'])==(10,725)
        assert pulse_request['instance_id']==request['instance_id'] and pulse_request['snapshot_sha256']==request['snapshot_sha256']
        pulser_request_file=case/'gui-maintenance-evidence/pulser-request.json';pulser_request=read(pulser_request_file)
        assert pulser_request['command']=='trigger-asad-pulser' and pulser_request['cobo_id']=='C0' and pulser_request['voltage_mv']==321
        assert pulser_request['instance_id']==request['instance_id'] and pulser_request['snapshot_sha256']==request['snapshot_sha256']
        requests=list((case/'gui-client-records/requests').glob('*.json'));assert len(requests)==6
        persisted={json.loads(path.read_text())['command']:path.read_bytes() for path in requests}
        assert persisted['reprepare-cobo']==request_file.read_bytes() and persisted['set-cobo-led']==led_request_file.read_bytes()
        assert persisted['blink-cobo-leds']==blink_request_file.read_bytes() and persisted['flip-cobo-led']==flip_request_file.read_bytes()
        assert persisted['trigger-asad-pulser']==pulser_request_file.read_bytes()
        assert persisted['pulse-cobo-led']==pulse_request_file.read_bytes()
        operation=read(case/'maintenance-operation.json');assert operation['phase']=='complete' and operation['request']==request
        key=digest('getdaq.ecc-control-request:1\n'+request_file.read_text());assert key==operation['request_key']
        result=operation['result'];assert result['state']==3 and result['transition']==result['error']==0
        assert not result['outcome_unknown'] and not result['needs_reconciliation']
        led_operation=read(case/'maintenance-led-operation.json');assert led_operation['phase']=='complete' and led_operation['request']==led_request
        led_key=digest('getdaq.ecc-control-request:1\n'+led_request_file.read_text());assert led_key==led_operation['request_key']
        led_result=led_operation['result'];assert led_result['state']==3 and led_result['transition']==led_result['error']==0
        assert not led_result['outcome_unknown'] and not led_result['needs_reconciliation']
        pulser_operation=read(case/'maintenance-pulser-operation.json')
        assert pulser_operation['phase']=='complete' and pulser_operation['request']==pulser_request
        pulser_key=digest('getdaq.ecc-control-request:1\n'+pulser_request_file.read_text())
        assert pulser_key==pulser_operation['request_key']
        pulser_result=pulser_operation['result'];assert pulser_result['state']==3 and pulser_result['transition']==0 and pulser_result['error']==0
        assert not pulser_result['outcome_unknown'] and not pulser_result['needs_reconciliation']
        blink_operation=read(case/'maintenance-blink-operation.json')
        assert blink_operation['phase']=='complete' and blink_operation['request']==blink_request
        blink_key=digest('getdaq.ecc-control-request:1\n'+blink_request_file.read_text())
        assert blink_key==blink_operation['request_key'] and blink_operation['result']['state']==3
        flip_operation=read(case/'maintenance-flip-operation.json')
        assert flip_operation['phase']=='complete' and flip_operation['request']==flip_request
        flip_key=digest('getdaq.ecc-control-request:1\n'+flip_request_file.read_text())
        assert flip_key==flip_operation['request_key']
        flip_result=flip_operation['result'];assert flip_result['state']==3 and flip_result['transition']==flip_result['error']==0
        assert not flip_result['outcome_unknown'] and not flip_result['needs_reconciliation']
        pulse_operation=read(case/'maintenance-led-pulse-operation.json')
        assert pulse_operation['phase']=='complete' and pulse_operation['request']==pulse_request
        pulse_key=digest('getdaq.ecc-control-request:1\n'+pulse_request_file.read_text())
        assert pulse_operation['request_key']==pulse_key
        pulse_result=pulse_operation['result'];assert pulse_result['state']==3 and pulse_result['transition']==pulse_result['error']==0
        assert not pulse_result['outcome_unknown'] and not pulse_result['needs_reconciliation']
        targets=read(case/'maintenance-targets.json')
        assert targets['instance_id']==request['instance_id'] and targets['snapshot_sha256']==request['snapshot_sha256']
        assert {item['id'] for item in targets['cobos']}=={'C0','C1'}
        network=rows(case/'controller/network/operations.jsonl')
        accepted=[r for r in network if r['event']=='accepted' and r['operation_id']==request['operation_id']]
        completed=[r for r in network if r['event']=='complete' and r['operation_id']==request['operation_id']]
        assert len(accepted)==len(completed)==1 and accepted[0]['request_key']==completed[0]['request_key']==key
        assert json.loads(accepted[0]['request'])==request and json.loads(completed[0]['result'])==result
        led_accepted=[r for r in network if r['event']=='accepted' and r['operation_id']==led_request['operation_id']]
        led_completed=[r for r in network if r['event']=='complete' and r['operation_id']==led_request['operation_id']]
        assert len(led_accepted)==len(led_completed)==1 and led_accepted[0]['request_key']==led_completed[0]['request_key']==led_key
        assert json.loads(led_accepted[0]['request'])==led_request and json.loads(led_completed[0]['result'])==led_result
        pulser_accepted=[r for r in network if r['event']=='accepted' and r['operation_id']==pulser_request['operation_id']]
        pulser_completed=[r for r in network if r['event']=='complete' and r['operation_id']==pulser_request['operation_id']]
        assert len(pulser_accepted)==len(pulser_completed)==1 and pulser_accepted[0]['request_key']==pulser_completed[0]['request_key']==pulser_key
        assert json.loads(pulser_accepted[0]['request'])==pulser_request and json.loads(pulser_completed[0]['result'])==pulser_result
        for command,req,key,result in (('blink-cobo-leds',blink_request,blink_key,blink_operation['result']),
                                       ('flip-cobo-led',flip_request,flip_key,flip_result),
                                       ('pulse-cobo-led',pulse_request,pulse_key,pulse_result)):
            accepted=[r for r in network if r['event']=='accepted' and r['operation_id']==req['operation_id']]
            completed=[r for r in network if r['event']=='complete' and r['operation_id']==req['operation_id']]
            assert len(accepted)==len(completed)==1 and accepted[0]['request_key']==completed[0]['request_key']==key
            assert json.loads(accepted[0]['request'])==req and json.loads(completed[0]['result'])==result
        native=rows(case/'controller/ecc.jsonl')
        intents=[r for r in native if r['phase']=='intent'];selected=[r for r in intents if r['id']==request['operation_id']]
        assert len(selected)==1 and selected[0]['command']==11 and selected[0]['from']==selected[0]['to']==3
        opdir=case/'controller/operations'/request['operation_id'];binding=rows(opdir/'operation.jsonl')[0]
        original=PurePosixPath(str(source_case/'controller'))
        original_opdir=source_case/'controller'/'operations'/request['operation_id']
        base=digest('getdaq.native-ecc-operation:1\n'+binding['snapshot_sha256']+'\n'+request['operation_id']+'\n11\n'+str(original_opdir))
        assert binding['cobo_id']=='C0' and binding['request_key']==selected[0]['request_key']==digest('getdaq.native-ecc-maintenance:1\n'+base+'\nC0')
        intent,done=maintenance(opdir/'maintenance/maintenance.jsonl',case/'controller',original)
        assert done and intent['cobo_id']=='C0'
        selected_led=[r for r in intents if r['id']==led_request['operation_id']]
        assert len(selected_led)==1 and selected_led[0]['command']==12 and selected_led[0]['from']==selected_led[0]['to']==3
        led_dir=case/'controller/operations'/led_request['operation_id'];led_binding=rows(led_dir/'operation.jsonl')[0]
        original_led_dir=source_case/'controller'/'operations'/led_request['operation_id']
        led_base=digest('getdaq.native-ecc-operation:1\n'+led_binding['snapshot_sha256']+'\n'+led_request['operation_id']+'\n12\n'+str(original_led_dir))
        assert led_binding['request_key']==selected_led[0]['request_key']==digest('getdaq.native-ecc-led:1\n'+led_base+'\nC0\n3\n2\n3')
        led_audit=rows(led_dir/'maintenance/led.jsonl')
        assert len(led_audit)==2 and led_audit[0]['phase']=='intent' and led_audit[1]['phase']=='complete'
        assert led_audit[0]['operation']=='set-cobo-led' and (led_audit[0]['cobo_id'],led_audit[0]['asad_id'],led_audit[0]['led_type'],led_audit[0]['led_state'])==('C0',3,2,3)
        assert not led_audit[0]['physical_lamp_state_verified'] and not led_audit[0]['automatic_retry']
        assert led_audit[1]['hardware_acknowledged'] and not led_audit[1]['physical_lamp_state_verified']
        selected_flip=[r for r in intents if r['id']==flip_request['operation_id']]
        assert len(selected_flip)==1 and selected_flip[0]['command']==15 and selected_flip[0]['from']==selected_flip[0]['to']==3
        flip_dir=case/'controller/operations'/flip_request['operation_id'];flip_binding=rows(flip_dir/'operation.jsonl')[0]
        original_flip_dir=source_case/'controller'/'operations'/flip_request['operation_id']
        flip_base=digest('getdaq.native-ecc-operation:1\n'+flip_binding['snapshot_sha256']+'\n'+flip_request['operation_id']+'\n15\n'+str(original_flip_dir))
        assert flip_binding['cobo_id']=='C0' and flip_binding['led_id']==10
        assert flip_binding['request_key']==selected_flip[0]['request_key']==digest('getdaq.native-ecc-led-flip:1\n'+flip_base+'\nC0\n10')
        flip_audit=rows(flip_dir/'maintenance/led-flip.jsonl')
        assert len(flip_audit)==2 and flip_audit[0]['phase']=='intent' and flip_audit[1]['phase']=='complete'
        assert flip_audit[0]['operation']=='flip-cobo-led' and flip_audit[0]['cobo_id']=='C0' and flip_audit[0]['led_id']==10
        assert not flip_audit[0]['physical_lamp_state_verified'] and not flip_audit[0]['automatic_retry']
        assert flip_audit[1]['hardware_acknowledged'] and not flip_audit[1]['physical_lamp_state_verified']
        selected_pulse=[r for r in intents if r['id']==pulse_request['operation_id']]
        assert len(selected_pulse)==1 and selected_pulse[0]['command']==16 and selected_pulse[0]['from']==selected_pulse[0]['to']==3
        pulse_dir=case/'controller/operations'/pulse_request['operation_id'];pulse_binding=rows(pulse_dir/'operation.jsonl')[0]
        original_pulse_dir=source_case/'controller'/'operations'/pulse_request['operation_id']
        pulse_base=digest('getdaq.native-ecc-operation:1\n'+pulse_binding['snapshot_sha256']+'\n'+pulse_request['operation_id']+'\n16\n'+str(original_pulse_dir))
        assert pulse_binding['cobo_id']=='C0' and (pulse_binding['led_id'],pulse_binding['period_ms'])==(10,725)
        assert pulse_binding['request_key']==selected_pulse[0]['request_key']==digest('getdaq.native-ecc-led-pulse:1\n'+pulse_base+'\nC0\n10\n725')
        pulse_audit=rows(pulse_dir/'maintenance/led-pulse.jsonl')
        assert len(pulse_audit)==2 and pulse_audit[0]['phase']=='intent' and pulse_audit[1]['phase']=='complete'
        assert pulse_audit[0]['operation']=='pulse-cobo-led' and pulse_audit[0]['cobo_id']=='C0'
        assert pulse_audit[0]['led_id']==10 and pulse_audit[0]['period_ms']==725 and pulse_audit[0]['period_semantics']=='half-period-ms'
        assert pulse_audit[0]['zero_stops_selected_pulser'] and not pulse_audit[0]['physical_lamp_state_verified'] and not pulse_audit[0]['automatic_retry']
        assert pulse_audit[1]['hardware_acknowledged'] and not pulse_audit[1]['physical_lamp_state_verified']
        selected_pulser=[r for r in intents if r['id']==pulser_request['operation_id']]
        assert len(selected_pulser)==1 and selected_pulser[0]['command']==13 and selected_pulser[0]['from']==selected_pulser[0]['to']==3
        pulser_dir=case/'controller/operations'/pulser_request['operation_id'];pulser_binding=rows(pulser_dir/'operation.jsonl')[0]
        original_pulser_dir=source_case/'controller'/'operations'/pulser_request['operation_id']
        pulser_base=digest('getdaq.native-ecc-operation:1\n'+pulser_binding['snapshot_sha256']+'\n'+pulser_request['operation_id']+'\n13\n'+str(original_pulser_dir))
        assert pulser_binding['cobo_id']=='C0' and pulser_binding['pulser_voltage_mv']==321
        assert pulser_binding['request_key']==selected_pulser[0]['request_key']==digest('getdaq.native-ecc-pulser:1\n'+pulser_base+'\nC0\n321')
        pulse_records=rows(pulser_dir/'maintenance/pulser.jsonl')
        assert len(pulse_records)==2 and pulse_records[0]['phase']=='intent' and pulse_records[1]['phase']=='complete'
        assert pulse_records[0]['operation']=='trigger-asad-pulser' and pulse_records[0]['cobo_id']=='C0' and pulse_records[0]['voltage_mv']==321
        assert not pulse_records[0]['physical_pulse_verified'] and not pulse_records[0]['automatic_retry']
        hardware_pulse=rows(pulser_dir/'maintenance/hardware-pulser.jsonl')
        assert len(hardware_pulse)==4 and hardware_pulse[0]['phase']=='intent' and hardware_pulse[1]['phase']=='begin'
        assert hardware_pulse[2]['phase']=='acknowledged' and hardware_pulse[3]['phase']=='complete'
        assert not hardware_pulse[2]['readback_verified']
        assert pulse_records[1]['hardware_acknowledged'] and not pulse_records[1]['physical_pulse_verified']
        following=[r['command'] for r in intents if int(r['seq'])>int(selected[0]['seq'])]
        assert all(command in following for command in (3,4,5,8))
        held=read(case/'maintenance-result.json')
        assert all(held[k] for k in ('passed','actual_main_window','other_board_unchanged','other_subscription_unchanged','system_alignment_not_repeated'))
        assert held['configured_board_repeat_rejected'] and held['duplicate_configure_no_mutation']
        assert held['reprepare_operation_id']==request['operation_id'] and held['led_operation_id']==led_request['operation_id']
        assert held['pulser_operation_id']==pulser_request['operation_id'] and held['pulser_voltage_mv']==321 and held['selected_pulser_command_count']==1
        assert held['flip_operation_id']==flip_request['operation_id'] and held['flip_led_id']==10
        assert held['selected_flip_command_count']==1 and held['changed_flip_bit_rejected']
        assert held['led_pulse_operation_id']==pulse_request['operation_id'] and (held['led_pulse_id'],held['led_pulse_period_ms'])==(10,725)
        assert held['selected_led_pulse_count']==1 and held['changed_led_pulse_period_rejected']
        assert held['selected_led_command_count']==1 and (held['led_type'],held['led_asad_id'],held['led_state'])==(2,3,3)
        assert not held['physical_lamp_state_verified'] and not held['physical_pulse_verified']
        scope=read(case/'scope.json')
        exit_proof=read(case/'maintenance-ecc-exit.json')
        assert exit_proof['normal_exit'] and exit_proof['exit_code']==0
        assert scope['real_ecc_network_service'] and scope['real_data_services']==2
        assert scope['real_qt_runcontrol'] and scope['real_receivers'] and scope['full_ecc_workflow_tested']
        assert not scope['real_hardware'] and not scope['throughput_verified']
        final=read(case/'final-status.json');assert not final['operation_active']
        assert not final['control_needs_reconciliation'] and not final['data']['needs_reconciliation']
        for node in final['data']['nodes']:
            receivers=node['lifecycle']['receivers'];assert receivers
            assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in receivers)
        summaries.append(dict(scenario=run['scenario'],gui_intents=6,maintenance_target='C0',led_target='C0/AsAd3/U/FastPulse',led_flip_target='C0/GPIO10',led_pulse_target='C0/GPIO10/725ms',pulser_target='C0/321mV',sealed_receivers=True))
    return summaries


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('evidence',type=Path)
    print('PASS selected CoBo GUI evidence:',json.dumps(check(parser.parse_args().evidence.resolve()),sort_keys=True))
