#!/usr/bin/env python3
"""Freeze final source readout/ECC integration, source/recipes/program identities and cleaned payload evidence."""
import argparse,json,shutil
from pathlib import Path
from run_receiver_observation_tests import sha
ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build-source-readout'
TARGET=STAGE/'validation-source-readout-ecc'


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    values=list(parent.glob(pattern));assert len(values)==1,(parent,pattern,len(values));return values[0]


def verify(current):
    manifest=read(TARGET/'files-sha256.json');inputs=read(TARGET/'input-sha256.json')
    assert {str(path.relative_to(TARGET)) for path in TARGET.rglob('*') if path.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        p=TARGET/name;assert p.suffix!='.graw' and not p.is_symlink() and sha(p)==digest,name
        assert name.startswith('verified-sources/') or not any(word in p.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(TARGET/'verified-sources'/name)==digest,name
        if current:assert sha(ROOT/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=TARGET/label;core=read(folder/'core-build/build-receipt.json')
        assert core['compile_count']==8 and core['sanitized']==(label=='sanitized') and not core['old_vendor_libraries_instrumented']
        for name,program in core['programs'].items():assert sha(folder/'core-programs'/name)==program['sha256'],name
        assert 'PASS 13' in (folder/'core-build/reader-result.txt').read_text() and 'PASS 9' in (folder/'core-build/readout-result.txt').read_text()
        remote=read(folder/'remote-evidence/result.json');local=read(folder/'local-evidence/result.json')
        assert len(remote['results'])==3 and all(row['exit_code']==0 for row in remote['results']) and len(local['cases'])==2 and all(row['passed'] for row in local['cases'])
        for kind,result,count,total in [('remote',remote,8,131072),('local',local,4,65536)]:
            assert result['raw_deleted'] and not result['real_hardware'] and result['maximum_aggregate_test_bytes']<=8*1024**2<result['aggregate_limit_bytes']==20*1024**3
            cleanup=read(folder/(kind+'-evidence')/'payload-cleanup.json');assert cleanup['remaining_raw']==0 and len(cleanup['deleted'])==count and sum(row['bytes'] for row in cleanup['deleted'])==total
        for name,digest in remote['program_sha256'].items():assert sha(folder/'programs'/name)==digest,(label,name)
        assert local['fixture_sha256']==sha(folder/'programs/local_fixture') and local['receiver_sha256']==sha(folder/'programs/receiver')
        regression=read(folder/'ring-regression/result.json');assert len(regression['cases'])==4 and all(row['passed'] for row in regression['cases']) and regression['raw_deleted']
        assert regression['fixture_sha256']==core['programs']['ring-ice-source-fixture']['sha256'] and regression['receiver_sha256']==sha(folder/'programs/receiver')
        if label=='sanitized':
            overlay=read(folder/'ecc-build/build-receipt.json');assert overlay['compile_count']==5 and overlay['sanitized'] and not overlay['old_vendor_libraries_instrumented']
            assert overlay['program_sha256']['ecc-network-service-tests']==remote['program_sha256']['fixture'] and overlay['program_sha256']['getdaq-ecc-server']==remote['program_sha256']['ecc_service']
    data=read(TARGET/'data-only/data-only-receipt.json');assert data['repo_source_count']==18 and data['get_interface_count']==3 and data['qt_configure_disabled'] and not data['qt_linked']
    print('PASS source readout/ECC archive:',len(manifest),'files;',len(inputs),'inputs; complete-message mux, actual local/remote cycles, cleaned raw; historical only',not current)


def create():
    assert not TARGET.exists(),'Preserve existing immutable validation';TARGET.mkdir();inputs={}
    def copy(path,target):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    def copy_metadata(build,target,source_record):
        for name,digest in read(build/source_record).items():assert sha(ROOT/name)==digest,name;inputs[name]=digest
        for path in build.iterdir():
            if path.is_file() and path.suffix in ['.json','.txt'] and 'credential' not in path.name:copy(path,target/path.name)
    overlay=Path('/tmp/getdaq-source-readout-ecc-overlay-asan')
    for label,core,native,receiver in [('ordinary',STAGE/'strict-ordinary-final',ROOT/'build-ice-compat',ROOT/'build-controlled-ice/ice-ordinary/getdaq-ice-receiver'),
                                      ('sanitized',Path('/tmp/getdaq-source-readout-core-final-asan'),overlay,Path('/tmp/getdaq-controlled-ice-ingress-asan/getdaq-ice-receiver'))]:
        copy_metadata(core,TARGET/label/'core-build','source-sha256.json')
        for name in read(core/'build-receipt.json')['programs']:copy(core/name,TARGET/label/'core-programs'/name)
        if label=='sanitized':copy_metadata(native,TARGET/label/'ecc-build','source-sha256.json')
        else:copy(native/'compile_commands.json',TARGET/label/'ecc-build/compile_commands.json')
        paths={'fixture':native/'ecc-network-service-tests','ecc_service':native/'getdaq-ecc-server','local_fixture':native/'native-ecc-ice-receiver-tests','receiver':receiver,
               'data_service':ROOT/'build-managed-ice/supervisor/getdaq-supervisor'}
        for name,path in paths.items():copy(path,TARGET/label/'programs'/name)
        for kind,pattern in [('remote','managed-ice-ecc-*'),('local','local-ice-ecc-*')]:
            evidence=one(STAGE/(label+('-ecc-final' if kind=='remote' else '-local-final')),pattern)
            for path in evidence.rglob('*'):
                if path.is_file() and not any(word in path.name.lower() for word in ['credential','bearer','token']):copy(path,TARGET/label/(kind+'-evidence')/path.relative_to(evidence))
        evidence=one(STAGE/(label+'-ring-regression'),'ring-ice-*')
        for path in evidence.rglob('*'):
            if path.is_file():copy(path,TARGET/label/'ring-regression'/path.relative_to(evidence))
    build=ROOT/'build-source-control/data-only'
    for name,digest in read(build/'data-only-source-sha256.json').items():assert sha(ROOT/name)==digest;inputs[name]=digest
    for name in ['data-only-source-sha256.json','data-only-receipt.json','data-only-build.txt','data-only-configure.txt']:copy(build/name,TARGET/'data-only'/name)
    for name in ['tests/archive_source_readout_ecc_validation.py','tests/build_source_readout_ecc_validation.py','tests/build_source_ring_validation.py','tests/build_native_ecc_sanitizers.py',
                 'tests/run_managed_ice_ecc_tests.py','tests/run_local_ice_ecc_tests.py','tests/run_ecc_network_service_tests.py','tests/ice_ecc_source_fixture.h',
                 'apps/ecc/native/CMakeLists.txt','README.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','docs/SOURCE_RING_READOUT.md','docs/SOURCE_READOUT_LIFECYCLE.md','docs/ICE_SOURCE_CONTROL.md','docs/CONTROLLED_ICE_SOURCE.md']:
        inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (TARGET/'README.md').write_text('源读出生命周期、完整帧边界切换和实际本机/远程ECC验证；普通/新代码插桩两编码多周期/空源/结束和raw清理。模拟设备内存，非真实板端DMA/中断/吞吐；旧vendor与系统Qt未整体插桩，数据服务为明确普通程序。\n')
    manifest={str(path.relative_to(TARGET)):sha(path) for path in sorted(TARGET.rglob('*')) if path.is_file()};(TARGET/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(not args.historical_only)
