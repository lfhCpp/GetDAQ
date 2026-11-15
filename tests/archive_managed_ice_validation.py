#!/usr/bin/env python3
"""Freeze actual managed ICE/mixed lifecycle proof, cleaned payload evidence and source hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build-managed-ice'
TARGET=STAGE/'validation-managed-ice'


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    found=list(parent.glob(pattern));assert len(found)==1,(str(parent),pattern,len(found));return found[0]


def verify(target,current=None):
    files=read(target/'files-sha256.json');inputs=read(target/'input-sha256.json')
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    for name,digest in files.items():
        path=target/name;assert not path.is_symlink() and path.suffix!='.graw' and sha(path)==digest,name
        assert name.startswith('verified-sources/') or not any(word in path.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest,name
        if current:assert sha(current/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=target/label
        for component,count in [('manager',50),('ice',10),('native',8)]:
            build=read(folder/component/'build/build-receipt.json');assert build['sanitized']==(label=='sanitized')
            actual=build['strict_cpp11_compile_count'] if component=='ice' else build['compile_count'];assert actual==count
            for name,digest in build['program_sha256'].items():assert sha(folder/component/'programs'/name)==digest
        for component in ['manager','ice']:
            generated=read(folder/component/'build/generated-sha256.json')
            for index,(name,digest) in enumerate(sorted(generated.items())):assert sha(folder/component/'generated'/(str(index)+'-'+Path(name).name))==digest
        if current:
            for name,digest in read(folder/'ice/build/dependency-sha256.json').items():assert sha(Path(name))==digest
        manager=read(folder/'manager/build/build-receipt.json')['program_sha256'];ice=read(folder/'ice/build/build-receipt.json')['program_sha256'];native=read(folder/'native/build/build-receipt.json')['program_sha256']
        for name,count in [('managed',10),('http',5),('ingress',14),('source',13)]:
            evidence=folder/(name+'-evidence');result=read(evidence/'result.json');assert len(result['cases'])==count and not result['real_hardware']
            assert result.get('raw_deleted',result.get('generated_raw_deleted'))
            maximum=result.get('maximum_aggregate_test_bytes',result.get('maximum_wire_bytes'));assert maximum<result['aggregate_limit_bytes']==20*1024**3
            assert read(evidence/'payload-cleanup.json').get('remaining_raw')==0
            if name=='managed':
                assert result['program_sha256']=={'fixture':manager['managed-ice-supervisor-tests'],'native':native['getdaq-receiver'],'ice':ice['getdaq-ice-receiver'],'source':ice['ice-ingress-fixture']}
                assert all(row['exit_code']==0 for row in result['cases'])
                for case in ['normal-0','normal-1','mixed-1','empty-0']:
                    stopped=read(evidence/case/'stopped.json');assert stopped['state']=='stopped'
                    for index,row in enumerate(stopped['receivers']):
                        is_ice=not(case=='mixed-1' and index==0)
                        assert row.get('source_end_confirmed',False)==is_ice and row['seal_verified'] and row['reaped'] and row['exit_code']==0
            elif name=='http':
                assert result['ice_sha256']==ice['getdaq-ice-receiver'] and result['native_sha256']==native['getdaq-receiver'] and result['source_sha256']==ice['ice-ingress-fixture']
                assert all(row['passed'] for row in result['cases'])
                for program,digest in result['program_sha256'].items():assert manager[program]==digest
            elif name=='ingress':assert result['receiver_sha256']==ice['getdaq-ice-receiver'] and result['fixture_sha256']==ice['ice-ingress-fixture']
            elif name=='source':
                assert result['receiver_sha256']==ice['getdaq-ice-receiver'] and result['source_fixture_sha256']==ice['ice-source-client-fixture']
                assert all(row['passed'] for row in result['cases'])
        regression=read(folder/'native-regression/result.json');assert len(regression['runs'])==2 and regression['maximum_wire_bytes']<20*1024**3
        assert all(row['exit_code']==0 for row in regression['runs']) and read(folder/'native-regression/payload-cleanup.json')['raw_files_remaining']==0
    for name in ['core-tests.txt','core-asan-tests.txt']:assert '100% tests passed' in (target/'additional-evidence'/name).read_text()
    assert 'PASS portable C++11 ECC codec' in (target/'additional-evidence/protocol-asan-tests.txt').read_text()
    qt=read(target/'gui/build/build-receipt.json');assert not qt['system_qt_instrumented'];assert sha(target/'gui/programs/GetDAQ')==qt['program_sha256']
    model=read(target/'gui-model/build/build-receipt.json');assert sha(target/'gui-model/programs/native-gui-tests')==model['program_sha256']
    assert '18 passed, 0 failed' in (target/'gui-model/results.txt').read_text()
    print('PASS managed ICE archive:',len(files),'files;',len(inputs),'inputs; actual mixed processes/end/HTTP/catalog; no raw/credentials; historical only',current is None)


def create():
    assert not TARGET.exists(),'preserve an existing immutable archive'
    TARGET.mkdir();inputs={}
    def copy(path,destination):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path)
        destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
    for label,manager,ice,native in [('ordinary',STAGE/'supervisor',STAGE/'ice-ordinary',STAGE/'native-ordinary'),
                                    ('sanitized',Path('/tmp/getdaq-managed-ice-supervisor-asan'),Path('/tmp/getdaq-managed-ice-asan'),Path('/tmp/getdaq-managed-ice-native-asan'))]:
        for component,build in [('manager',manager),('ice',ice),('native',native)]:
            source=read(build/'source-sha256.json')
            for name,digest in source.items():
                assert sha(ROOT/name)==digest,name
                if name in inputs:assert inputs[name]==digest
                inputs[name]=digest
            for path in build.glob('*'):
                if path.is_file() and (path.suffix in ['.json','.txt']) and path.name not in ['CMakeCache.txt','compile_commands.json']:
                    copy(path,TARGET/label/component/'build'/path.name)
            if (build/'generated-sha256.json').exists():
                for index,(name,digest) in enumerate(sorted(read(build/'generated-sha256.json').items())):
                    generated=Path(name) if Path(name).is_absolute() else build/name
                    assert sha(generated)==digest;copy(generated,TARGET/label/component/'generated'/(str(index)+'-'+Path(name).name))
            receipt=read(build/'build-receipt.json')
            for name,digest in receipt['program_sha256'].items():assert sha(build/name)==digest;copy(build/name,TARGET/label/component/'programs'/name)
        mapping=[('managed',label+'-final-v3','managed-ice-*'),('http',label+'-http-final-v3','managed-ice-http-*'),
                 ('ingress',label+'-ingress-v3','ice-ingress-*'),('source',label+'-source-v3','ice-source-*'),
                 ('native-regression',label+'-native-regression-v3','receiver-observation-*')]
        for name,parent,pattern in mapping:
            evidence=one(STAGE/parent,pattern)
            for path in evidence.rglob('*'):
                if not path.is_file() or path.is_symlink() or path.suffix=='.graw' or any(word in path.name.lower() for word in ['credential','token','bearer']):continue
                assert path.stat().st_size<=64*1024*1024,str(path)
                copy(path,TARGET/label/(name+'-evidence' if name!='native-regression' else name)/path.relative_to(evidence))
    for name in ['gui','gui-model']:
        build=STAGE/name;receipt=read(build/'build-receipt.json')
        for source,digest in read(build/'source-sha256.json').items():assert sha(ROOT/source)==digest;inputs[source]=digest
        for path in build.glob('*'):
            if path.is_file() and (path.name.endswith('.json') or path.name=='results-verified-build.txt' or path.name.startswith('moc_') and path.suffix=='.cpp' or path.name.startswith('ui_') and path.suffix=='.h'):
                copy(path,TARGET/name/'build'/path.name)
        program=(build/receipt['target']).resolve();copy(program,TARGET/name/'programs'/('GetDAQ' if name=='gui' else 'native-gui-tests'))
    copy(Path('/tmp/getdaq-managed-ice-gui-tests.txt'),TARGET/'gui-model/results.txt')
    for name in ['protocol-asan-tests','core-tests','core-asan-tests']:
        copy(Path('/tmp/getdaq-managed-ice-'+name+'.txt'),TARGET/'additional-evidence'/(name+'.txt'))
    copy(ROOT/'build-managed-ice/ecc-protocol-asan',TARGET/'additional-evidence/ecc-protocol-asan')
    copy(Path('/tmp/getdaq-managed-ice-ecc-build.txt'),TARGET/'additional-evidence/ecc-rebuild.log')
    copy(ROOT/'build-ice-compat/getdaq-ecc-server',TARGET/'additional-evidence/getdaq-ecc-server-ordinary')
    for name in ['README.md','docs/ARCHITECTURE.md','docs/ICE_DATA_INGRESS.md','docs/ICE_DATA_SOURCE.md','docs/MANAGED_ICE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md',
                 'tests/archive_managed_ice_validation.py','tests/run_managed_ice_tests.py','tests/managed_ice_http_test.py','tests/run_ice_ingress_tests.py','tests/run_ice_source_tests.py',
                 'tests/run_receiver_observation_tests.py','tests/run_file_naming_supervisor_tests.py','tests/run_supervisor_control_tests.py',
                 'tests/receiver_transport_integration.py','common/control/eccprotocol.cpp','tests/ecc_protocol_test.cpp']:
        inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (TARGET/'README.md').write_text('本机混合ICE/TCP进程管理和专属源结束归档证据。旧库/系统Qt未整体插桩；非板端、目标平台、持续吞吐或断电验收。所有测试raw已清理。历史模式只核对冻结字节，当前模式另核对当前输入。\n')
    files={str(path.relative_to(TARGET)):sha(path) for path in sorted(TARGET.rglob('*')) if path.is_file()}
    (TARGET/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(TARGET,None if args.historical_only else ROOT)


if __name__=='__main__':main()
