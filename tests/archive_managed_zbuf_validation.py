#!/usr/bin/env python3
"""Freeze managed ZBUF builds, actual programs and small cleaned evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def verify(target,current=None):
    manifest=json.loads((target/'files-sha256.json').read_text())
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        assert sha(target/name)==digest,name
        assert not name.endswith('.graw') and (name.startswith('verified-sources/') or 'credential' not in name.lower())
    inputs=json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current:assert sha(current/name)==digest,name
    for label in ('ordinary','sanitized'):
        folder=target/label
        for kind,count in [('receiver',8),('supervisor',49)]:
            receipt=json.loads((folder/(kind+'-build')/'build-receipt.json').read_text())
            assert receipt['sanitized']==(label=='sanitized') and receipt['compile_count']==count
            for name,digest in receipt['program_sha256'].items():assert sha(folder/kind/name)==digest
        evidence=folder/'supervisor-evidence';result=json.loads((evidence/'result.json').read_text())
        assert result['raw_files_remaining']==0 and result['maximum_wire_bytes']==9043968<result['aggregate_limit_bytes']==20*1024**3
        assert result['receiver_sha256']==sha(folder/'receiver/getdaq-receiver')
        for name,digest in result['program_sha256'].items():assert sha(folder/'supervisor'/name)==digest
        native=next((evidence/'native').glob('managed-file-naming-*'));cases=json.loads((native/'result.json').read_text())
        assert len(cases['cases'])==23 and all(row['exit_code']==0 for row in cases['cases']) and cases['generated_raw_deleted']
        assert all(source['input_protocol']=='ZBUF' for source in json.loads((native/'zbuf-managed/deployment/plan.json').read_text())['sources'])
        http=next((evidence/'http').glob('remote-receiver-*'));http_cases=json.loads((http/'results.json').read_text())
        assert len(http_cases)==22 and all(row['exit_code']==0 for row in http_cases)
        network=folder/'ecc-evidence';network_result=json.loads((network/'result.json').read_text())
        assert network_result['generated_raw_deleted'] and network_result['maximum_wire_bytes']==3145728 and not network_result['real_hardware']
        assert {row['scenario'] for row in network_result['results']}=={'zbuf-0','zbuf-1','normal-0'}
        for name,digest in network_result['program_sha256'].items():assert sha(folder/'programs'/name)==digest
        network_root=next(network.glob('ecc-network-system-*'))
        for case in ('zbuf-0','zbuf-1'):
            snapshot=json.loads((network_root/case/'controller/snapshot.json').read_text())
            assert all(sender['protocol']=='ZBUF' for sender in snapshot['senders'])
        local_result=json.loads((folder/'local-evidence/result.json').read_text())
        assert local_result['passed'] and local_result['generated_raw_deleted']
        assert local_result['fixture_sha256']==sha(folder/'programs/local-fixture') and local_result['receiver_sha256']==sha(folder/'receiver/getdaq-receiver')
        assert local_result['runner_sha256']==sha(target/'verified-sources/tests/run_local_receiver_cleanup.py')
        cleanup=json.loads((folder/'local-evidence/payload-cleanup.json').read_text());assert cleanup['remaining_raw']==0 and len(cleanup['verified_fixture_roots'])==1
        transport=json.loads((folder/'transport/result.json').read_text());assert transport['generated_raw_deleted'] and len(transport['cases'])==12
        assert (folder/'results-core.txt').read_text().startswith('PASS:')
    reference=json.loads((target/'reference/result.json').read_text())
    assert reference['native_plan_cases']==6 and len(reference['timestamp_cases'])==27 and len(reference['actual_filename_comparisons'])==21
    integrated=json.loads((target/'ordinary/integrated-normal-receipt.json').read_text())
    mapping={'getdaq-ecc-server':'ecc_service','getdaq-supervisor':'data_service','ecc-network-service-tests':'fixture','hardware-data-sender-tests':'sender-fixture','native-ecc-receiver-tests':'local-fixture'}
    for name,program in integrated['programs'].items():assert sha(target/'ordinary/programs'/mapping[name])==program['sha256']
    assert 'PASS sender lifecycle/unknown/preflight/checkpoint encoding 1.0' in (target/'ordinary/results-sender.txt').read_text()
    commands=json.loads((target/'sanitized/native-build/commands.json').read_text())
    commands=[row if isinstance(row,list) else row['command'] for row in commands];compiled=[row for row in commands if '-c' in row]
    assert len(compiled)==75 and all('-std=c++11' in row and '-pedantic-errors' in row and '-fsanitize=address,undefined' in row for row in compiled)
    print('PASS managed ZBUF archive:',len(manifest),'files;',len(inputs),'inputs; same programs/sources; no raw/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    target=root/'build-managed-zbuf/validation-managed-zbuf'
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files={};inputs={}
    def add(path,name):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw' and name not in files
        assert '..' not in Path(name).parts and 'credential' not in name.lower();files[name]=path
    def merge(build):
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest),name
            inputs[name]=digest
    def tree(parent,prefix,pattern):
        matches=list(parent.glob(pattern));assert len(matches)==1 and not list(matches[0].rglob('*.graw'))
        for path in matches[0].rglob('*'):
            relative=str(path.relative_to(matches[0]))
            if path.is_file() and not path.is_symlink() and 'credential' not in relative.lower():add(path,prefix+'/'+relative)
    native=Path('/tmp/getdaq-managed-zbuf-ecc-asan');merge(native)
    for label,receiver,supervisor,build in [('ordinary',root/'build-receiver',root/'build-managed-zbuf',root/'build-ice-compat'),
                                          ('sanitized',Path('/tmp/getdaq-managed-zbuf-receiver-asan'),Path('/tmp/getdaq-managed-zbuf-supervisor-asan'),native)]:
        for kind,source_build in [('receiver',receiver),('supervisor',supervisor)]:
            merge(source_build);receipt=json.loads((source_build/'build-receipt.json').read_text())
            for name,digest in receipt['program_sha256'].items():assert sha(source_build/name)==digest;add(source_build/name,label+'/'+kind+'/'+name)
            for name in ('build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt'):
                add(source_build/name,label+'/'+kind+'-build/'+name)
        add(receiver/'results-managed-zbuf-core.txt',label+'/results-core.txt')
        add(receiver/'results-managed-zbuf-transports.txt',label+'/results-transports.txt')
        add(supervisor/'results-supervisor-current.txt',label+'/results-supervisor.txt')
        tree(root/('build-managed-zbuf/'+label+'-supervisor-evidence'),label+'/supervisor-evidence','receiver-observation-*')
        tree(root/('build-managed-zbuf/'+label+'-ecc-evidence'),label+'/ecc-evidence','receiver-gui-service-*')
        tree(root/('build-managed-zbuf/'+label+'-transports'),label+'/transport','receiver-transports-*')
        local=root/('build-managed-zbuf/local-marker-final-evidence' if label=='ordinary' else 'build-managed-zbuf/sanitized-local-evidence')
        tree(local,label+'/local-evidence','local-receiver-check-*')
        for name,path in [('fixture',build/'ecc-network-service-tests'),('ecc_service',build/'getdaq-ecc-server'),
                          ('receiver',receiver/'getdaq-receiver'),('data_service',build/'getdaq-supervisor' if label=='ordinary' else supervisor/'getdaq-supervisor'),
                          ('local-fixture',build/'native-ecc-receiver-tests')]:add(path,label+'/programs/'+name)
    add(root/'build-ice-compat/hardware-data-sender-tests','ordinary/programs/sender-fixture')
    add(root/'build-ice-compat/results-managed-zbuf-sender-current.txt','ordinary/results-sender.txt')
    add(root/'build-managed-zbuf/integrated-normal-receipt.json','ordinary/integrated-normal-receipt.json')
    add(root/'build-ice-compat/results-managed-zbuf-current-build.txt','ordinary/results-integrated-build.txt')
    add(root/'build-ice-compat/results-managed-zbuf-local-final-build.txt','ordinary/results-local-build.txt')
    for name in ('commands.json','source-sha256.json','generated-sha256.json','dependency-sha256.json'):add(native/name,'sanitized/native-build/'+name)
    for path,digest in json.loads((native/'generated-sha256.json').read_text()).items():assert sha(Path(path))==digest;add(Path(path),'sanitized/native-build/generated/'+Path(path).name)
    for path in native.glob('*.compile.txt'):
        if 'credential' not in path.name.lower():add(path,'sanitized/native-build/logs/'+path.name)
    for path in native.glob('*.link.txt'):add(path,'sanitized/native-build/logs/'+path.name)
    tree(root/'build-managed-zbuf/reference-final-evidence','reference','file-naming-reference-*')
    add(root/'build-managed-zbuf/results-default-plan-reference-final.txt','results-reference.txt')
    for name in ['tests/archive_managed_zbuf_validation.py','tests/run_local_receiver_cleanup.py','tests/run_receiver_observation_tests.py','tests/run_receiver_gui_observation_tests.py',
                 'tests/run_file_naming_supervisor_tests.py','tests/run_supervisor_control_tests.py','tests/run_ecc_network_service_tests.py','tests/receiver_transport_integration.py',
                 'tests/file_naming_original_reference.py','tests/build_native_ecc_sanitizers.py','tests/hardware_data_sender_test.cpp',
                 'source/get/GetBench-software-release-20210208/src/get/daq/DaqCtrlNodeI.cpp','source/get/GetBench-software-release-20210208/src/get/daq/DataRouter.cpp',
                 'source/mordicus/MDaq-release-20210208/src/mdaq/daq/TcpDataSender.hpp','source/mordicus/MDaq-release-20210208/src/mdaq/utl/net/ZBufConnection.cpp',
                 'docs/MANAGED_ZBUF.md','docs/RECEIVER_TRANSPORTS.md','docs/REMOTE_RECEIVER_CONTROL.md','docs/DATA_SERVICE.md','docs/DATA_SENDER_CONTROL.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:files['verified-sources/'+name]=root/name
    target.mkdir()
    for name,path in files.items():dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
