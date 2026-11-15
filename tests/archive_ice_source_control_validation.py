#!/usr/bin/env python3
"""Freeze typed source control, actual ECC/session workflows and latched fault evidence."""
import argparse,json,shutil
from pathlib import Path
from run_receiver_observation_tests import sha
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'build-source-control';TARGET=STAGE/'validation-ice-source-control'

def read(path):return json.loads(path.read_text())
def one(parent,pattern):
    found=list(parent.glob(pattern));assert len(found)==1,(parent,pattern,len(found));return found[0]
def verify(current):
    files=read(TARGET/'files-sha256.json');inputs=read(TARGET/'input-sha256.json')
    assert {str(p.relative_to(TARGET)) for p in TARGET.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    for name,digest in files.items():
        p=TARGET/name;assert not p.is_symlink() and p.suffix!='.graw' and sha(p)==digest,name
        assert name.startswith('verified-sources/') or not any(word in p.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(TARGET/'verified-sources'/name)==digest,name
        if current:assert sha(ROOT/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=TARGET/label
        for group,count in [('direct',11),('ecc',3),('local',2)]:
            evidence=folder/(group+'-evidence');result=read(evidence/'result.json');rows=result.get('cases',result.get('results'));assert len(rows)==count
            assert all(row.get('passed',row.get('exit_code')==0) for row in rows) and result['raw_deleted'] and not result['real_hardware']
            assert result['maximum_aggregate_test_bytes']<result['aggregate_limit_bytes']==20*1024**3 and read(evidence/'payload-cleanup.json')['remaining_raw']==0
            if group=='direct':
                assert sha(folder/'programs/ice-source-control-tests')==result['fixture_sha256'] and sha(folder/'programs/getdaq-ice-receiver')==result['receiver_sha256']
                lost=next(row for row in rows if row['case']=='end-ack-loss-0');assert lost['control']['unknown'] and not lost['control']['source_end_acknowledged'] and lost['receiver_end_confirmed']
            elif group=='local':assert sha(folder/'programs/native-ecc-ice-receiver-tests')==result['fixture_sha256']
            else:
                for key,name in [('fixture','ecc-network-service-tests'),('ecc_service','getdaq-ecc-server'),('receiver','getdaq-ice-receiver'),('data_service','getdaq-supervisor')]:assert sha(folder/'programs'/name)==result['program_sha256'][key]
                assert sum(row['bytes'] for row in read(evidence/'payload-cleanup.json')['deleted'])==131072
        assert 'ACK cannot clear UNKNOWN' in (folder/'fault-probe.txt').read_text()
        assert (folder/'programs/source-fault-commit-tests').is_file()
    commands=read(TARGET/'sanitized/native-build/commands.json');compiled=[r['command'] for r in commands if '-c' in r['command']]
    assert len(compiled)==81 and all('-std=c++11' in r and '-pedantic-errors' in r and '-fsanitize=address,undefined' in r for r in compiled)
    data=read(TARGET/'data-only/build/data-only-receipt.json');assert data['qt_configure_disabled'] and not data['qt_linked'] and data['get_interface_count']==3 and data['repo_source_count']==13
    for name,digest in data['program_sha256'].items():assert sha(TARGET/'data-only/programs'/name)==digest
    result=read(TARGET/'data-only/evidence/result.json');assert len(result['cases'])==12 and result['raw_deleted']
    assert result['receiver_sha256']==data['program_sha256']['getdaq-ice-receiver'] and result['fixture_sha256']==data['program_sha256']['controlled-ice-source-fixture']
    assert 'ACK cannot clear UNKNOWN' in (TARGET/'data-only/fault-probe.txt').read_text()
    regression=read(TARGET/'default-regression/result.json');assert len(regression['results'])==4 and all(r['exit_code']==0 for r in regression['results']) and regression['generated_raw_deleted']
    assert '100% tests passed' in (TARGET/'default-sender-tests.txt').read_text()
    print('PASS ICE source control archive:',len(files),'files;',len(inputs),'inputs; actual pinned control/local/remote ECC; UNKNOWN latch; no raw/credentials; historical only',not current)
def create():
    assert not TARGET.exists(),'preserve existing immutable archive';TARGET.mkdir();inputs={}
    def copy(path,target):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    def tree(folder,target):
        for path in folder.rglob('*'):
            if path.is_file() and not path.is_symlink() and path.suffix!='.graw' and not any(word in path.name.lower() for word in ['credential','bearer','token']):copy(path,target/path.relative_to(folder))
    def merge(path):
        for name,digest in read(path).items():assert sha(ROOT/name)==digest,name;inputs[name]=digest
    ordinary=ROOT/'build-ice-compat';san=Path('/tmp/getdaq-source-control-native-asan');local=Path('/tmp/getdaq-source-control-local-asan')
    merge(STAGE/'ordinary-build/source-sha256.json');merge(san/'source-sha256.json');merge(local/'source-sha256.json')
    for label,build,receiver,local_program,service in [('ordinary',ordinary,ROOT/'build-controlled-ice/ice-ordinary/getdaq-ice-receiver',ordinary/'native-ecc-ice-receiver-tests',ROOT/'build-managed-ice/supervisor/getdaq-supervisor'),
        ('sanitized',san,Path('/tmp/getdaq-controlled-ice-ingress-asan/getdaq-ice-receiver'),local/'native-ecc-ice-receiver-tests',ROOT/'build-managed-ice/supervisor/getdaq-supervisor')]:
        for name,path in [('ice-source-control-tests',build/'ice-source-control-tests'),('ecc-network-service-tests',build/'ecc-network-service-tests'),('getdaq-ecc-server',build/'getdaq-ecc-server'),('getdaq-supervisor',service),('getdaq-ice-receiver',receiver),('native-ecc-ice-receiver-tests',local_program)]:copy(path,TARGET/label/'programs'/name)
        fault=ordinary/'source-fault-commit-tests' if label=='ordinary' else Path('/tmp/getdaq-source-control-fault-asan/source-fault-commit-tests')
        copy(fault,TARGET/label/'programs/source-fault-commit-tests')
        for group,pattern in [('direct','source-control-*'),('ecc','managed-ice-ecc-*'),('local','local-ice-ecc-*')]:tree(one(STAGE/(label+'-'+group+'-final'),pattern),TARGET/label/(group+'-evidence'))
        copy(Path('/tmp/getdaq-source-control-fault-'+('asan-' if label=='sanitized' else '')+'tests.txt'),TARGET/label/'fault-probe.txt')
    for path in san.glob('*'):
        if path.is_file() and path.suffix in ['.json','.txt'] and not any(word in path.name.lower() for word in ['credential','bearer','token']):copy(path,TARGET/'sanitized/native-build'/path.name)
    copy(Path('/tmp/getdaq-source-control-fault-asan/build-receipt.json'),TARGET/'sanitized/fault-build/build-receipt.json')
    copy(Path('/tmp/getdaq-source-control-fault-asan/commands.json'),TARGET/'sanitized/fault-build/commands.json')
    for path in local.glob('*'):
        if path.is_file() and path.suffix in ['.json','.txt','.log']:copy(path,TARGET/'sanitized/local-build'/path.name)
    for name in ['source-control-protocol/generated/SourceControl.h','source-control-protocol/generated/SourceControl.cpp','ice-ingress/generated/IceIngress.h','ice-ingress/generated/IceIngress.cpp']:
        copy(ordinary/name,TARGET/'generated'/name)
    for path in (STAGE/'ordinary-build').glob('*.json'):copy(path,TARGET/'ordinary/build'/path.name)
    data=STAGE/'data-only';merge(data/'data-only-source-sha256.json')
    for name in ['data-only-receipt.json','data-only-source-sha256.json','data-only-configure.txt','data-only-build.txt','compile_commands.json']:copy(data/name,TARGET/'data-only/build'/name)
    for name in read(data/'data-only-receipt.json')['program_sha256']:copy(data/name,TARGET/'data-only/programs'/name)
    copy(data/'source-fault-commit-tests',TARGET/'data-only/programs/source-fault-commit-tests')
    tree(one(STAGE/'data-only-lifecycle-final','controlled-ice-*'),TARGET/'data-only/evidence');copy(Path('/tmp/getdaq-source-control-fault-data-only-tests.txt'),TARGET/'data-only/fault-probe.txt')
    tree(one(STAGE/'ordinary-default-regression','managed-fdt-*'),TARGET/'default-regression');copy(Path('/tmp/getdaq-source-control-default-sender-tests.txt'),TARGET/'default-sender-tests.txt')
    for name in ['tests/archive_ice_source_control_validation.py','tests/run_ice_source_control_tests.py','tests/run_managed_ice_ecc_tests.py','tests/run_local_ice_ecc_tests.py','tests/build_native_ice_local_validation.py',
        'tests/run_ecc_network_service_tests.py','tests/run_managed_fdt_tests.py','tests/build_ice_data_only_validation.py','tests/build_controlled_ice_source_validation.py','tests/source_fault_commit_test.cpp',
        'apps/icecontrol/CMakeLists.txt','apps/icecontrol/protocol/CMakeLists.txt','apps/icecontrol/SourceControl.ice','apps/hardware/CMakeLists.txt','apps/ecc/native/CMakeLists.txt','third_party/ice-compat/CMakeLists.txt',
        'README.md','docs/ARCHITECTURE.md','docs/ICE_SOURCE_CONTROL.md','docs/CONTROLLED_ICE_SOURCE.md','docs/MANAGED_ICE.md','docs/ICE_DATA_SOURCE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']:inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n');(TARGET/'README.md').write_text('实际本机源控制、固定实例/会话、完整ECC与UNKNOWN证据；旧库/系统Qt未整体插桩，非硬件/平台/持续吞吐/断电验收。全部raw清理。\n')
    files={str(p.relative_to(TARGET)):sha(p) for p in sorted(TARGET.rglob('*')) if p.is_file()};(TARGET/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(not args.historical_only)
if __name__=='__main__':main()
