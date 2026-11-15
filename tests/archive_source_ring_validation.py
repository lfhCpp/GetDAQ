#!/usr/bin/env python3
"""Freeze ring readout builds and actual ordinary/sanitized/data-only evidence without raw files."""
import argparse
import json
from pathlib import Path
import shutil
from run_receiver_observation_tests import sha

ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build-source-ring'
TARGET=STAGE/'validation-source-ring'


def read(path):return json.loads(path.read_text())


def one(parent):
    values=list(parent.glob('ring-ice-*'));assert len(values)==1,(parent,values);return values[0]


def verify(current):
    files=read(TARGET/'files-sha256.json');inputs=read(TARGET/'input-sha256.json')
    assert {str(p.relative_to(TARGET)) for p in TARGET.rglob('*') if p.is_file()}==set(files)|{'files-sha256.json'}
    for name,digest in files.items():
        path=TARGET/name;assert path.suffix!='.graw' and not path.is_symlink() and sha(path)==digest,name
        assert name.startswith('verified-sources/') or not any(word in path.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(TARGET/'verified-sources'/name)==digest,name
        if current:assert sha(ROOT/name)==digest,name
    for label in ['ordinary','sanitized','data-only']:
        folder=TARGET/label;result=read(folder/'evidence/result.json');cases={row['case']:row for row in result['cases']}
        assert set(cases)=={'cycles-0','cycles-1','empty-0','publish-fault-1'} and all(row['passed'] for row in cases.values())
        assert result['raw_deleted'] and not result['real_hardware'] and result['maximum_aggregate_test_bytes']==2*1024**2<result['aggregate_limit_bytes']==20*1024**3
        assert sha(folder/'programs/ring-ice-source-fixture')==result['fixture_sha256'] and sha(folder/'programs/getdaq-ice-receiver')==result['receiver_sha256']
        assert sha(TARGET/'verified-sources/tests/run_ring_ice_source_tests.py')==result['runner_sha256']
        cleanup=read(folder/'evidence/payload-cleanup.json');assert cleanup['remaining_raw']==0 and sum(row['bytes'] for row in cleanup['deleted'])==73728
        for name in ['cycles-0','cycles-1']:
            row=cases[name];assert row['raw_bytes']==36864 and row['received_bytes']==36864 and row['source']['handed_total']==row['source']['published_total']==36864
        failure=cases['publish-fault-1']['source'];assert failure['ring_unknown'] and failure['source_unknown'] and failure['handed_total']==317 and failure['published_total']==0 and not failure['end_acknowledged']
        if label!='data-only':
            receipt=read(folder/'build/build-receipt.json');assert receipt['compile_count']==6 and receipt['sanitized']==(label=='sanitized') and not receipt['old_vendor_libraries_instrumented']
            assert receipt['programs']['ring-ice-source-fixture']['sha256']==result['fixture_sha256']
            assert 'PASS 12' in (folder/'build/core-result.txt').read_text()
        else:
            receipt=read(folder/'build/data-only-receipt.json');assert receipt['qt_configure_disabled'] and not receipt['qt_linked'] and receipt['get_interface_count']==3 and receipt['repo_source_count']==16
    print('PASS source ring archive:',len(files),'files;',len(inputs),'inputs; exact stream, no pointer replay, raw cleaned; historical only',not current)


def create():
    assert not TARGET.exists(),'Preserve existing immutable evidence';TARGET.mkdir();inputs={}
    def copy(path,target):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    for label,build,receiver,parent in [
        ('ordinary',STAGE/'strict-ordinary',ROOT/'build-controlled-ice/ice-ordinary/getdaq-ice-receiver','strict-ordinary-evidence'),
        ('sanitized',Path('/tmp/getdaq-source-ring-asan'),Path('/tmp/getdaq-controlled-ice-ingress-asan/getdaq-ice-receiver'),'sanitized-evidence')]:
        for name,digest in read(build/'source-sha256.json').items():
            assert sha(ROOT/name)==digest,name;inputs[name]=digest
        for path in build.iterdir():
            if path.is_file() and path.suffix in ['.json','.txt']:copy(path,TARGET/label/'build'/path.name)
        copy(build/'ring-ice-source-fixture',TARGET/label/'programs/ring-ice-source-fixture');copy(build/'source-ring-reader-tests',TARGET/label/'programs/source-ring-reader-tests');copy(receiver,TARGET/label/'programs/getdaq-ice-receiver')
        evidence=one(STAGE/parent)
        for path in evidence.rglob('*'):
            if path.is_file():copy(path,TARGET/label/'evidence'/path.relative_to(evidence))
    build=ROOT/'build-source-control/data-only'
    for name,digest in read(build/'data-only-source-sha256.json').items():assert sha(ROOT/name)==digest;inputs[name]=digest
    for name in ['data-only-receipt.json','data-only-source-sha256.json','data-only-configure.txt','data-only-build.txt','compile_commands.json']:copy(build/name,TARGET/'data-only/build'/name)
    for name in ['getdaq-ice-receiver','ring-ice-source-fixture']:copy(build/name,TARGET/'data-only/programs'/name)
    evidence=one(STAGE/'ordinary')
    for path in evidence.rglob('*'):
        if path.is_file():copy(path,TARGET/'data-only/evidence'/path.relative_to(evidence))
    for name in ['tests/archive_source_ring_validation.py','README.md','docs/ARCHITECTURE.md','docs/SOURCE_RING_READOUT.md','docs/CONTROLLED_ICE_SOURCE.md','docs/ICE_SOURCE_CONTROL.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md',
        'source/mordicus/MDaq-release-20210208/src/mdaq/daq/CircularBuffer.h','source/mordicus/MDaq-release-20210208/src/mdaq/daq/IceDataSender.cpp',
        'source/get/GetBench-software-release-20210208/src/get/daq/SyncedCircularBuffer.cpp','source/get/GetBench-software-release-20210208/src/get/daq/MemRead.cpp','source/get/GetBench-software-release-20210208/src/get/daq/DaqCtrlNodeI.cpp']:
        inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest,name;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (TARGET/'README.md').write_text('可移植源端环形缓冲/读指针和双线程实际ICE联调证据；三组raw清理，不含实际凭据。新代码严格C++11及ASan/UBSan，旧vendor库未整体插桩。非真实DMA/中断/目标平台/长期吞吐/断电验收。\n')
    files={str(path.relative_to(TARGET)):sha(path) for path in sorted(TARGET.rglob('*')) if path.is_file()};(TARGET/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(not args.historical_only)
