#!/usr/bin/env python3
"""Freeze source-side quiesce/drain/UNKNOWN and Qt-free data-only evidence."""
import argparse
import json
from pathlib import Path
import shutil
from run_receiver_observation_tests import sha

ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build-controlled-ice'
TARGET=STAGE/'validation-controlled-ice'


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    found=list(parent.glob(pattern));assert len(found)==1,(parent,pattern,len(found));return found[0]


def verify(current):
    manifest=read(TARGET/'files-sha256.json');inputs=read(TARGET/'input-sha256.json')
    assert {str(p.relative_to(TARGET)) for p in TARGET.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        p=TARGET/name;assert p.suffix!='.graw' and not p.is_symlink() and sha(p)==digest,name
        assert name.startswith('verified-sources/') or not any(word in p.name.lower() for word in ['credential','bearer','token'])
    for name,digest in inputs.items():
        assert sha(TARGET/'verified-sources'/name)==digest,name
        if current:assert sha(ROOT/name)==digest,name
    for label in ['ordinary','sanitized','data-only']:
        folder=TARGET/label;result=read(folder/'evidence/result.json');cases={row['case']:row for row in result['cases']}
        assert len(cases)==12 and all(row['passed'] for row in cases.values()) and result['raw_deleted'] and not result['real_hardware']
        assert result['maximum_aggregate_test_bytes']==8*1024**2<result['aggregate_limit_bytes']==20*1024**3
        assert result['runner_sha256']==sha(TARGET/'verified-sources/tests/run_controlled_ice_source_tests.py')
        assert sha(folder/'programs/getdaq-ice-receiver')==result['receiver_sha256'] and sha(folder/'programs/controlled-ice-source-fixture')==result['fixture_sha256']
        cleanup=read(folder/'evidence/payload-cleanup.json');assert cleanup['remaining_raw']==0 and len(cleanup['deleted'])==8 and sum(row['bytes'] for row in cleanup['deleted'])==61440
        for mode in ['cycles-0','cycles-1','empty-0']:
            source=json.loads((folder/'evidence'/mode/'source.log').read_text().splitlines()[-1]);assert source['state']==8 and source['source_end_acknowledged']
            receiver=json.loads((folder/'evidence'/mode/'stdout.log').read_text().splitlines()[-1]);assert receiver['sender_end_confirmed'] and not receiver['sender_eof_confirmed'] and receiver['completion_receipt_published']
        for mode in ['drain-fail-0','nonstd-drain-1','idle-stop-1','late-feed-0','end-loss-0']:
            source=json.loads((folder/'evidence'/mode/'source.log').read_text().splitlines()[-1]);assert source['state']==9 and not source['source_end_acknowledged']
        if label!='data-only':
            build=read(folder/'source-build/build-receipt.json');assert build['compile_count']==2 and build['sanitized']==(label=='sanitized') and not build['old_vendor_libraries_instrumented']
            assert build['program_sha256']['controlled-ice-source-fixture']==result['fixture_sha256']
            assert len([row for row in read(folder/'source-build/commands.json') if '-c' in row])==2
            dependency=read(folder/'ice-build/build-receipt.json');assert dependency['strict_cpp11_compile_count']==10 and not dependency['qt_used']
            assert dependency['program_sha256']['getdaq-ice-receiver']==result['receiver_sha256']
        else:
            build=read(folder/'build/data-only-receipt.json');assert build['qt_configure_disabled'] and not build['qt_linked'] and build['get_interface_count']==1 and build['repo_source_count']==11
            for name,digest in build['program_sha256'].items():assert sha(folder/'programs'/name)==digest
    print('PASS controlled ICE archive:',len(manifest),'files;',len(inputs),'inputs; source join/drain/retained stream/UNKNOWN; no Qt data-only/raw/credentials; historical only',not current)


def create():
    assert not TARGET.exists(),'preserve an existing immutable archive';TARGET.mkdir();inputs={}
    def copy(path,target):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw',str(path)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    for label,source,ice,parent in [('ordinary',STAGE/'source-ordinary',STAGE/'ice-ordinary','ordinary-final'),
                                    ('sanitized',Path('/tmp/getdaq-controlled-ice-source-asan'),Path('/tmp/getdaq-controlled-ice-ingress-asan'),'sanitized-final')]:
        for kind,build in [('source-build',source),('ice-build',ice)]:
            for name,digest in read(build/'source-sha256.json').items():assert sha(ROOT/name)==digest;inputs[name]=digest
            for path in build.glob('*'):
                if path.is_file() and path.suffix in ['.json','.txt']:copy(path,TARGET/label/kind/path.name)
        for name,path in [('getdaq-ice-receiver',ice/'getdaq-ice-receiver'),('controlled-ice-source-fixture',source/'controlled-ice-source-fixture')]:copy(path,TARGET/label/'programs'/name)
        evidence=one(STAGE/parent,'controlled-ice-*')
        for path in evidence.rglob('*'):
            if path.is_file() and not path.is_symlink() and path.suffix!='.graw':copy(path,TARGET/label/'evidence'/path.relative_to(evidence))
    build=STAGE/'data-only'
    for name,digest in read(build/'data-only-source-sha256.json').items():assert sha(ROOT/name)==digest;inputs[name]=digest
    for name in ['data-only-receipt.json','data-only-source-sha256.json','data-only-configure.txt','data-only-build.txt','compile_commands.json']:copy(build/name,TARGET/'data-only/build'/name)
    for name in read(build/'data-only-receipt.json')['program_sha256']:copy(build/name,TARGET/'data-only/programs'/name)
    evidence=one(STAGE/'data-only-evidence','controlled-ice-*')
    for path in evidence.rglob('*'):
        if path.is_file() and not path.is_symlink() and path.suffix!='.graw':copy(path,TARGET/'data-only/evidence'/path.relative_to(evidence))
    for name in ['tests/archive_controlled_ice_validation.py','tests/run_controlled_ice_source_tests.py','tests/build_controlled_ice_source_validation.py','tests/build_ice_data_only_validation.py',
                 'tests/run_receiver_observation_tests.py','tests/receiver_transport_integration.py','README.md','docs/ARCHITECTURE.md','docs/CONTROLLED_ICE_SOURCE.md','docs/ICE_DATA_SOURCE.md',
                 'docs/MANAGED_ICE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']:
        inputs[name]=sha(ROOT/name)
    for name,digest in inputs.items():assert sha(ROOT/name)==digest;copy(ROOT/name,TARGET/'verified-sources'/name)
    (TARGET/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    (TARGET/'README.md').write_text('源端线程停止/排空、同会话多周期及UNKNOWN、Qt-free构建的本机证据；非真实板端/完整ECC/目标平台/持续吞吐验收。三组raw均清理，旧库未整体插桩。\n')
    files={str(p.relative_to(TARGET)):sha(p) for p in sorted(TARGET.rglob('*')) if p.is_file()};(TARGET/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if args.create:create()
    verify(not args.historical_only)


if __name__=='__main__':main()
