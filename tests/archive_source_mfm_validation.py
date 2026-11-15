#!/usr/bin/env python3
"""Freeze checked raw MFM prefixes, partial-tail refusal and receiver/ECC regression evidence."""
import argparse,json,shutil
from pathlib import Path
from run_receiver_observation_tests import sha
ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build-source-mfm'
TARGET=STAGE/'validation-source-mfm'


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    rows=list(parent.glob(pattern));assert len(rows)==1,(parent,pattern,len(rows));return rows[0]


def verify(current):
    files=read(TARGET/'files-sha256.json');inputs=read(TARGET/'input-sha256.json')
    assert {str(p.relative_to(TARGET)) for p in TARGET.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    for name,digest in files.items():
        p=TARGET/name;assert p.suffix!='.graw' and not p.is_symlink() and sha(p)==digest,name
        assert name.startswith('verified-sources/') or not any(word in p.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(TARGET/'verified-sources'/name)==digest,name
        if current:assert sha(ROOT/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=TARGET/label;core=read(folder/'source-build/build-receipt.json');assert core['compile_count']==10 and core['sanitized']==(label=='sanitized')
        for name,program in core['programs'].items():assert sha(folder/'source-programs'/name)==program['sha256']
        for name,count in [('reader',13),('readout',9),('mfm',15)]:assert 'PASS '+str(count) in (folder/'source-build'/(name+'-result.txt')).read_text()
        comparison=read(folder/'header-comparison/result.json');assert comparison['case_count']==20006 and comparison['sanitized']==(label=='sanitized') and not comparison['full_original_library_executed']
        assert sha(TARGET/'verified-sources/apps/datareceiver/mfmheader.h')==comparison['shared_header_sha256']
        ring=read(folder/'ring-evidence/result.json');cases={r['case']:r for r in ring['cases']};assert len(cases)==6 and all(r['passed'] for r in cases.values()) and ring['raw_deleted']
        assert ring['fixture_sha256']==core['programs']['ring-ice-source-fixture']['sha256'] and ring['receiver_sha256']==sha(folder/'programs/receiver')
        for mode,tail in [('partial-header-0',7),('partial-body-1',2048)]:
            s=cases[mode]['source'];assert s['raw_unknown'] and s['ring_unknown'] and s['source_unknown'] and not s['end_acknowledged']
            assert s['complete_write_total']==s['published_total']==8192 and s['raw_write_total']==8192+tail
        clean=read(folder/'ring-evidence/payload-cleanup.json');assert clean['remaining_raw']==0 and sum(r['bytes'] for r in clean['deleted'])==90112
        remote=read(folder/'remote-evidence/result.json');local=read(folder/'local-evidence/result.json');assert len(remote['results'])==3 and all(r['exit_code']==0 for r in remote['results']) and len(local['cases'])==2 and all(r['passed'] for r in local['cases'])
        for name,digest in remote['program_sha256'].items():assert sha(folder/'programs'/name)==digest
        assert local['fixture_sha256']==sha(folder/'programs/local_fixture') and remote['raw_deleted'] and local['raw_deleted']
        native=read(folder/'native-evidence/result.json');assert len(native['cases'])==12 and native['generated_raw_deleted'] and native['aggregate_wire_bytes']==49739
        assert native['receiver_sha256']==sha(folder/'native-programs/getdaq-receiver')
        ingress=read(folder/'ingress-evidence/result.json');assert len(ingress['cases'])==14 and ingress['generated_raw_deleted']
        assert ingress['receiver_sha256']==sha(folder/'programs/receiver') and ingress['fixture_sha256']==sha(folder/'programs/ingress_fixture')
        if label=='sanitized':assert read(folder/'ecc-build/build-receipt.json')['compile_count']==6
    data=read(TARGET/'data-only/data-only-receipt.json');assert data['repo_source_count']==20 and data['get_interface_count']==3 and data['qt_configure_disabled'] and not data['qt_linked']
    print('PASS source MFM archive:',len(files),'files;',len(inputs),'inputs; raw partial tails refuse End, shared header compatibility, actual ECC/receiver; historical only',not current)


def create():
    assert not TARGET.exists(),'Preserve existing immutable evidence';TARGET.mkdir();inputs={}
    def copy(path,target):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    def metadata(build,target,record):
        for name,digest in read(build/record).items():assert sha(ROOT/name)==digest,name;inputs[name]=digest
        for path in build.iterdir():
            if path.is_file() and path.suffix in ['.json','.txt'] and 'credential' not in path.name:copy(path,target/path.name)
    for label,core,native,ecc,receiver,ingress,comparison in [
        ('ordinary',STAGE/'source-ordinary',STAGE/'receiver',ROOT/'build-ice-compat',ROOT/'build-source-control/data-only/getdaq-ice-receiver',ROOT/'build-source-control/data-only/ice-ingress-fixture',STAGE/'header-comparison-final'),
        ('sanitized',Path('/tmp/getdaq-source-mfm-source-asan'),Path('/tmp/getdaq-source-mfm-native-receiver-asan'),Path('/tmp/getdaq-source-mfm-ecc-asan'),Path('/tmp/getdaq-source-mfm-ice-asan/getdaq-ice-receiver'),Path('/tmp/getdaq-source-mfm-ice-asan/ice-ingress-fixture'),Path('/tmp/getdaq-source-mfm-header-comparison-asan'))]:
        folder=TARGET/label;metadata(core,folder/'source-build','source-sha256.json');metadata(native,folder/'native-build','source-sha256.json')
        if label=='sanitized':metadata(ecc,folder/'ecc-build','source-sha256.json');metadata(Path('/tmp/getdaq-source-mfm-ice-asan'),folder/'ice-build','source-sha256.json')
        else:copy(ecc/'compile_commands.json',folder/'ecc-build/compile_commands.json')
        for name in read(core/'build-receipt.json')['programs']:copy(core/name,folder/'source-programs'/name)
        for name in read(native/'build-receipt.json')['program_sha256']:copy(native/name,folder/'native-programs'/name)
        for path in comparison.iterdir():
            if path.is_file():copy(path,folder/'header-comparison'/path.name)
        programs={'fixture':ecc/'ecc-network-service-tests','ecc_service':ecc/'getdaq-ecc-server','local_fixture':ecc/'native-ecc-ice-receiver-tests','receiver':receiver,
                  'data_service':ROOT/'build-managed-ice/supervisor/getdaq-supervisor','ingress_fixture':ingress}
        for name,path in programs.items():copy(path,folder/'programs'/name)
        evidence=[('ring',STAGE/(label+'-ring-final'),'ring-ice-*'),('remote',STAGE/(label+'-ecc-final'),'managed-ice-ecc-*'),('local',STAGE/(label+'-local-final'),'local-ice-ecc-*'),
                  ('native',STAGE/(label+'-native-transports'),'receiver-transports-*'),('ingress',STAGE/(label+'-ice-ingress-regression'+('-final' if label=='ordinary' else '')),'ice-ingress-*')]
        for kind,parent,pattern in evidence:
            selected=one(parent,pattern)
            for path in selected.rglob('*'):
                if path.is_file() and not any(word in path.name.lower() for word in ['credential','bearer','token']):copy(path,folder/(kind+'-evidence')/path.relative_to(selected))
    build=ROOT/'build-source-control/data-only'
    for name,digest in read(build/'data-only-source-sha256.json').items():assert sha(ROOT/name)==digest;inputs[name]=digest
    for name in ['data-only-source-sha256.json','data-only-receipt.json','data-only-build.txt','data-only-configure.txt']:copy(build/name,TARGET/'data-only'/name)
    for name in ['tests/archive_source_mfm_validation.py','tests/check_mfm_header_compatibility.py','tests/build_source_readout_ecc_validation.py','tests/build_source_ring_validation.py',
                 'tests/build_ice_ingress_validation.py','tests/build_ice_data_only_validation.py','tests/run_ring_ice_source_tests.py','tests/run_managed_ice_ecc_tests.py','tests/run_local_ice_ecc_tests.py',
                 'tests/run_ecc_network_service_tests.py','tests/run_ice_ingress_tests.py','tests/receiver_transport_integration.py','tests/ice_ecc_source_fixture.h','apps/ecc/native/CMakeLists.txt',
                 'README.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','docs/SOURCE_MFM_BOUNDARIES.md','docs/SOURCE_RING_READOUT.md','docs/SOURCE_READOUT_LIFECYCLE.md']:
        inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (TARGET/'README.md').write_text('共享MFM头/源原始前缀检查、半帧拒绝End及实际ECC/接收器回归；本机合成内存，不是DMA/真机/生产吞吐/断电。新代码严格C++11/ASan/UBSan，旧vendor/系统Qt未整体插桩；无raw/实际凭据。\n')
    files={str(path.relative_to(TARGET)):sha(path) for path in sorted(TARGET.rglob('*')) if path.is_file()};(TARGET/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(not args.historical_only)
