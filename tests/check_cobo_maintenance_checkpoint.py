#!/usr/bin/env python3
"""Verify native selected-board maintenance evidence without a GUI/hardware claim."""
import argparse
import json
from pathlib import Path
from check_ecc_plan_transition_checkpoint import read,sha,relative,hashes,verify_map
from check_cobo_maintenance_evidence import check

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-cobo-maintenance'


def create(args):
    assert not (PHASE/'checkpoint.json').exists(),'Keep the original checkpoint'
    results=[PHASE/'ordinary-evidence/result.json',args.sanitized_preparation.resolve(),args.sanitized_coordinator.resolve()]
    files=[];inputs={};summaries=[];builds={}
    for name in ['sanitized-preparation','sanitized-coordinator']:
        file=PHASE/name/'build-receipt.json';value=read(file);assert value['sanitized']
        for field in ['dependencies','tracked_inputs','reused_objects','compiled_objects']:verify_map(value[field])
        if value['reuse_overlay']:verify_map({value['reuse_overlay']['file']:value['reuse_overlay']['sha256']})
        for command in value['commands']:
            if '-c' in command:assert all(flag in command for flag in ['-std=c++11','-pedantic-errors','-fsanitize=address,undefined'])
        inputs.update(value['tracked_inputs']);files.append(file)
        builds[name]=dict(compiled=value['compile_count'],reused=len(value['reused_objects']))
    assert any(name.endswith('/apps/hardware/systempreparation.cpp') for name in inputs)
    for result_file in results:
        value=read(result_file);summaries.append(dict(result=relative(result_file),checked=check(result_file)))
        if value['scope']=='existing-completed-ordinary-runs':
            for run in value['runs']:
                path=Path(run['program']);assert sha(path)==run['program_sha256'];files.append(path)
        else:
            assert value['sanitized'] and value['leak_detection'] and value['runner_sha256']==sha(ROOT/'tests/run_cobo_maintenance_tests.py')
            path=Path(value['program']);assert sha(path)==value['program_sha256'];files.append(path)
        files.extend(p for p in result_file.parent.rglob('*') if p.is_file())
    for name in ['state-tests.log','journal-tests.log','codec-tests.log']:
        file=PHASE/name;assert file.read_text().startswith('PASS ');files.append(file)
    files.extend(ROOT/name for name in ['build-ice-compat/ecc/ecc-state-tests','build-ice-compat/ecc-journal-tests','build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests','build-ice-compat/compile_commands.json'])
    for name in ['build.log','coordinator-build.log','native-tests.log','coordinator-0.log','coordinator-1.log','coordinator-0.index.root.json','coordinator-1.index.root.json','sanitized-preparation-build.log','sanitized-coordinator-build.log','sanitized-preparation-run.log','sanitized-coordinator-run.log','documentation-links.log']:
        files.append(PHASE/name)
    for name in ['tests/check_cobo_maintenance_checkpoint.py','tests/check_cobo_maintenance_evidence.py','tests/check_ecc_plan_transition_checkpoint.py','tests/check_ecc_plan_transition_evidence.py','tests/check_ecc_service_handoff_checkpoint.py','tests/check_controller_handoff_evidence.py','tests/run_cobo_maintenance_tests.py','tests/build_controller_handoff_sanitizers.py','tests/cobo_maintenance_cases.h','tests/hardware_system_preparation_test.cpp','tests/native_ecc_coordinator_test.cpp','tests/ecc_state_test.cpp','tests/ecc_protocol_test.cpp','docs/COBO_MAINTENANCE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']:
        files.append(ROOT/name)
    for p in files:
        if ROOT in p.resolve().parents and p.suffix in ('.cpp','.h','.py'):inputs[str(p.resolve())]=sha(p)
    value=dict(schema=1,phase='70.58-native',goal_complete=False,real_hardware=False,real_receiver_executed=False,
        production_http_or_gui_integrated=False,whole_suite_rebuilt=False,new_runtime_package=False,raw_remaining=0,
        library_cases=12,native_coordinator_cases=4,source_inputs=inputs,artifacts=hashes(files),runs=summaries,sanitizer_builds=builds)
    verify_map(inputs);verify_map(value['artifacts']);assert not list(PHASE.rglob('*.graw'))
    (PHASE/'checkpoint.json').write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--index',action='store_true')
    parser.add_argument('--sanitized-preparation',type=Path);parser.add_argument('--sanitized-coordinator',type=Path);args=parser.parse_args()
    if args.index:
        assert args.sanitized_preparation and args.sanitized_coordinator;create(args)
    value=read(PHASE/'checkpoint.json');assert not value['goal_complete'] and not value['production_http_or_gui_integrated'] and not value['real_receiver_executed']
    count=verify_map(value['source_inputs'])+verify_map(value['artifacts'])
    for run in value['runs']:assert check(ROOT/run['result'])==run['checked']
    assert not list(PHASE.rglob('*.graw'))
    print('PASS native CoBo maintenance checkpoint:',count,'digests; 12 library cases, 4 ECC cases; HTTP/GUI and whole goal incomplete')


if __name__=='__main__':main()
