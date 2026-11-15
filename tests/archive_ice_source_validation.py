#!/usr/bin/env python3
"""Freeze native ICE source/receiver pairing and bounded cleaned failure evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    matches=list(parent.glob(pattern));assert len(matches)==1,(str(parent),pattern,len(matches))
    return matches[0]


def verify(target,current=None):
    manifest=read(target/'files-sha256.json');inputs=read(target/'input-sha256.json')
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        path=target/name;assert path.suffix!='.graw' and not path.is_symlink() and sha(path)==digest,name
        assert name.startswith('verified-sources/') or not any(word in name.lower() for word in ['credential','token','bearer'])
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current:assert sha(current/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=target/label;build=read(folder/'build/build-receipt.json')
        assert build['strict_cpp11_compile_count']==10 and build['sanitized']==(label=='sanitized') and not build['qt_used'] and not build['local_vendor_libraries_instrumented']
        commands=read(folder/'build/commands.json');compiled=[row for row in commands if '-c' in row]
        assert len(compiled)==10 and all('-std=c++11' in row and '-pedantic-errors' in row for row in compiled)
        if label=='sanitized':assert all('-fsanitize=address,undefined' in row for row in compiled)
        for name,digest in build['program_sha256'].items():assert sha(folder/'programs'/name)==digest
        result=read(folder/'source-evidence/result.json');cases={row['case']:row for row in result['cases']}
        assert len(cases)==13 and all(row['passed'] for row in cases.values()) and result['generated_raw_deleted'] and not result['real_hardware']
        assert result['maximum_wire_bytes']==8*1024**2<result['aggregate_limit_bytes']==20*1024**3
        mapping={'receiver_sha256':'getdaq-ice-receiver','source_fixture_sha256':'ice-source-client-fixture','legacy_fixture_sha256':'ice-ingress-fixture','sender_sha256':'getdaq-ice-send'}
        for key,name in mapping.items():assert result[key]==sha(folder/'programs'/name)
        assert result['runner_sha256']==sha(target/'verified-sources/tests/run_ice_source_tests.py')
        for name in ['normal-0','normal-1','discover-1','empty-0']:assert cases[name]['source_status']['source_end_acknowledged']
        for name in ['send-loss-0','finish-loss-1']:
            row=cases[name];assert row['replacement_received_bytes']==0 and row['source_status']['state']==6 and not row['source_status']['source_end_acknowledged']
        assert cases['legacy-peer-0']['legacy_data_calls']==0 and not cases['destroy-0']['source_status']['finish_issued']
        cleanup=read(folder/'source-evidence/payload-cleanup.json');assert cleanup['remaining_raw']==0 and len(cleanup['deleted'])==11 and sum(row['bytes'] for row in cleanup['deleted'])==73728
        ingress=read(folder/'ingress-evidence/result.json');assert len(ingress['cases'])==14 and ingress['generated_raw_deleted']
        assert ingress['receiver_sha256']==sha(folder/'programs/getdaq-ice-receiver') and ingress['fixture_sha256']==sha(folder/'programs/ice-ingress-fixture')
        assert ingress['runner_sha256']==sha(target/'verified-sources/tests/run_ice_ingress_tests.py')
        assert all(not row['final_status']['sender_eof_confirmed'] for row in ingress['cases'])
        for name,digest in read(folder/'build/generated-sha256.json').items():assert sha(folder/'generated'/Path(name).name)==digest
    print('PASS ICE source archive:',len(manifest),'files;',len(inputs),'inputs; actual fixed source connection/UNKNOWN/false EOF; no raw/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];base=root/'build-ice-source';target=base/'validation-ice-source'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files={};inputs={}
    def add(path,name):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw' and name not in files and '..' not in Path(name).parts
        assert name.startswith('verified-sources/') or not any(word in name.lower() for word in ['credential','token','bearer'])
        files[name]=path
    for label,build in [('ordinary',base/'ordinary-build'),('sanitized',Path('/tmp/getdaq-ice-source-asan'))]:
        for name,digest in read(build/'source-sha256.json').items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest),name
            inputs[name]=digest
        for pattern in ['*.json','*.compile.txt','*.link.txt']:
            for path in build.glob(pattern):add(path,label+'/build/'+path.name)
        for name,digest in read(build/'build-receipt.json')['program_sha256'].items():assert sha(build/name)==digest;add(build/name,label+'/programs/'+name)
        for path,digest in read(build/'generated-sha256.json').items():assert sha(Path(path))==digest;add(Path(path),label+'/generated/'+Path(path).name)
        for kind,parent,pattern in [('source',base/(label+'-final'),'ice-source-*'),('ingress',base/(label+'-ingress'),'ice-ingress-*')]:
            folder=one(parent,pattern);assert not list(folder.rglob('*.graw'))
            for path in folder.rglob('*'):
                relative=str(path.relative_to(folder))
                if path.is_file() and not path.is_symlink() and not any(word in relative.lower() for word in ['credential','token','bearer']):add(path,label+'/'+kind+'-evidence/'+relative)
    for name in ['tests/archive_ice_source_validation.py','tests/run_ice_source_tests.py','tests/run_ice_ingress_tests.py',
                 'tests/receiver_transport_integration.py','tests/run_receiver_observation_tests.py',
                 'docs/ICE_DATA_SOURCE.md','docs/ICE_DATA_INGRESS.md','docs/VALIDATION.md','docs/REFACTOR_STATUS.md','docs/ARCHITECTURE.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir()
    for name,path in files.items():dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
