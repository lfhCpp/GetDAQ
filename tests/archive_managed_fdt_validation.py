#!/usr/bin/env python3
"""Freeze actual managed FDT programs, cleaned test evidence and same-source inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):value.update(block)
    return value.hexdigest()


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    paths=list(parent.glob(pattern));assert len(paths)==1,(str(parent),pattern,len(paths))
    return paths[0]


def verify(target,current=None):
    manifest=read(target/'files-sha256.json');inputs=read(target/'input-sha256.json')
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        path=target/name;assert not path.is_symlink() and path.suffix!='.graw' and sha(path)==digest,name
        assert name.startswith('verified-sources/') or not any(word in name.lower() for word in ['credential','token','bearer'])
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest,name
        if current:assert sha(current/name)==digest,name
    source=target/'verified-sources'
    for label in ('ordinary','sanitized'):
        folder=target/label;receiver_sha=sha(folder/'receiver/getdaq-receiver')
        for kind,count in [('receiver',8),('supervisor',49)]:
            receipt=read(folder/(kind+'-build')/'build-receipt.json')
            assert receipt['compile_count']==count and receipt['sanitized']==(label=='sanitized')
            for name,digest in receipt['program_sha256'].items():assert sha(folder/kind/name)==digest
        direct=read(folder/'private-control/result.json')
        assert len(direct['cases'])==8 and direct['generated_raw_deleted'] and direct['wire_bytes']==20520
        assert direct['receiver_sha256']==receiver_sha and direct['runner_sha256']==sha(source/'tests/receiver_cycle_control_test.py')
        unused=next(row for row in direct['cases'] if row['case']=='finish-unused')
        assert unused['final_status']['source_unused_confirmed'] and not unused['final_status']['sender_eof_confirmed'] and unused['final_status']['connections']==0
        observed=read(folder/'supervisor-evidence/result.json')
        assert observed['raw_files_remaining']==0 and observed['maximum_wire_bytes']==11425536<observed['aggregate_limit_bytes']==20*1024**3
        assert observed['receiver_sha256']==receiver_sha and all(row['exit_code']==0 for row in observed['runs'])
        for name,digest in observed['program_sha256'].items():assert sha(folder/'supervisor'/name)==digest
        native=read(one(folder/'supervisor-evidence/native','managed-file-naming-*')/'result.json')
        assert len(native['cases'])==29 and all(row['exit_code']==0 for row in native['cases']) and native['generated_raw_deleted']
        assert native['runner_sha256']==sha(source/'tests/run_file_naming_supervisor_tests.py')
        http=read(folder/'cycle-http/result.json')
        assert len(http['cases'])==5 and all(row['passed'] for row in http['cases']) and http['generated_raw_deleted'] and http['wire_bytes']==36864
        assert http['receiver_sha256']==receiver_sha and http['runner_sha256']==sha(source/'tests/receiver_cycle_http_test.py')
        for name,digest in http['program_sha256'].items():assert sha(folder/'supervisor'/name)==digest
        ecc=read(folder/'ecc-evidence/result.json')
        assert {row['scenario'] for row in ecc['results']}=={'fdt-0','fdt-1','fdt-empty-0','normal-0'} and all(row['exit_code']==0 for row in ecc['results'])
        assert ecc['generated_raw_deleted'] and ecc['maximum_wire_bytes']==4202496 and not ecc['qt_gui_executed'] and not ecc['real_hardware']
        assert ecc['runner_sha256']==sha(source/'tests/run_managed_fdt_tests.py') and ecc['underlying_runner_sha256']==sha(source/'tests/run_ecc_network_service_tests.py')
        for name,digest in ecc['program_sha256'].items():assert sha(folder/'programs'/name)==digest
        for name in ('fdt-0','fdt-empty-0'):
            local=read(folder/('local-'+name)/'result.json')
            assert local['passed'] and local['generated_raw_deleted'] and local['receiver_sha256']==receiver_sha
            assert local['fixture_sha256']==sha(folder/'programs/local-fixture') and local['runner_sha256']==sha(source/'tests/run_local_receiver_cleanup.py')
            assert local['command'][-1]==name
            cleanup=read(folder/('local-'+name)/'payload-cleanup.json');assert cleanup['remaining_raw']==0 and len(cleanup['verified_fixture_roots'])==1
        qt=read(folder/'qt-model-build/build-receipt.json')
        assert qt['sanitized']==(label=='sanitized') and qt['source_count']==78 and qt['tracked_input_count']==199
        assert qt['program_sha256']==sha(folder/'programs/qt-model')
        model=(folder/'results-qt-model.txt').read_text();assert '17 passed, 0 failed' in model and 'unusedReceiverRemainsVisibleWithoutAutomaticCommands' in model
        assert (folder/'results-protocol.txt').read_text().startswith('PASS portable C++11 ECC codec:')
    integrated=read(target/'ordinary/integrated-normal-receipt.json')
    for name,digest in integrated['program_sha256'].items():assert sha(target/'ordinary/programs'/name)==digest
    assert not integrated['strict_cpp11_native_build'] and integrated['strict_cpp11_verified_in_separate_sanitized_build']
    commands=read(target/'sanitized/native-build/commands.json')
    compiled=[r if isinstance(r,list) else r['command'] for r in commands]
    compiled=[r for r in compiled if '-c' in r]
    assert len(compiled)==75 and all('-std=c++11' in r and '-pedantic-errors' in r and '-fsanitize=address,undefined' in r for r in compiled)
    assert '67 passed, 0 failed' in (target/'ordinary/results-qt-full.txt').read_text()
    assert 'encoding 1.0' in (target/'ordinary/results-sender.txt').read_text() and 'encoding 1.1' in (target/'ordinary/results-sender.txt').read_text()
    for kind,program in [('qt-full','qt-full'),('qt-console','console')]:
        assert read(target/'ordinary'/(kind+'-build')/'build-receipt.json')['program_sha256']==sha(target/'ordinary/programs'/program)
    print('PASS managed FDT archive:',len(manifest),'files;',len(inputs),'inputs; actual programs, explicit unused/false EOF; no raw/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];base=root/'build-managed-fdt';target=base/'validation-managed-fdt'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files={};inputs={}
    def add(path,name):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw' and name not in files
        assert '..' not in Path(name).parts and (name.startswith('verified-sources/') or not any(w in name.lower() for w in ['credential','token','bearer']))
        files[name]=path
    def merge(build):
        for name,digest in read(build/'source-sha256.json').items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest),name
            inputs[name]=digest
    def tree(folder,prefix):
        assert folder.is_dir() and not list(folder.rglob('*.graw'))
        for path in folder.rglob('*'):
            relative=str(path.relative_to(folder))
            if path.is_file() and not path.is_symlink() and not any(w in relative.lower() for w in ['credential','token','bearer']):add(path,prefix+'/'+relative)
    def evidence(parent,prefix,pattern):tree(one(parent,pattern),prefix)
    def build_receipts(build,prefix):
        for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt']:
            if (build/name).is_file():add(build/name,prefix+'/'+name)
        if (build/'generated-sha256.json').exists():
            add(build/'generated-sha256.json',prefix+'/generated-sha256.json')
            for name,digest in read(build/'generated-sha256.json').items():assert sha(build/name)==digest;add(build/name,prefix+'/generated/'+name)
    native=Path('/tmp/getdaq-managed-fdt-ecc-asan');merge(native)
    for label,receiver,supervisor,sdk,model in [
        ('ordinary',root/'build-unused-source-receiver',root/'build-unused-source-supervisor',root/'build-ice-compat',base/'qt-model'),
        ('sanitized',Path('/tmp/getdaq-managed-fdt-receiver-asan'),Path('/tmp/getdaq-managed-fdt-supervisor-asan'),native,Path('/tmp/getdaq-managed-fdt-qt-model-asan'))]:
        for kind,build in [('receiver',receiver),('supervisor',supervisor)]:
            merge(build);build_receipts(build,label+'/'+kind+'-build')
            for name,digest in read(build/'build-receipt.json')['program_sha256'].items():assert sha(build/name)==digest;add(build/name,label+'/'+kind+'/'+name)
        control=supervisor/'ordinary-control' if label=='ordinary' else base/'sanitized-control'
        observation=supervisor/'ordinary-observation-final' if label=='ordinary' else base/'sanitized-observation'
        http=supervisor/'ordinary-http' if label=='ordinary' else base/'sanitized-http'
        evidence(control,label+'/private-control','receiver-cycle-*')
        evidence(observation,label+'/supervisor-evidence','receiver-observation-*')
        evidence(http,label+'/cycle-http','receiver-cycle-http-*')
        evidence(base/(label+'-ecc'),label+'/ecc-evidence','managed-fdt-*')
        for case,dirname in [('fdt-0',label+'-local'),('fdt-empty-0',label+'-local-unused')]:
            wrapper=one(base/dirname,'local-receiver-check-*');tree(wrapper,label+'/local-'+case)
            cleanup=read(wrapper/'payload-cleanup.json');assert len(cleanup['verified_fixture_roots'])==1
            tree(Path(cleanup['verified_fixture_roots'][0]),label+'/local-'+case+'/owned-root-metadata')
        for name,path in [('fixture',sdk/'ecc-network-service-tests'),('ecc_service',sdk/'getdaq-ecc-server'),('local-fixture',sdk/'native-ecc-receiver-tests'),
                          ('receiver',receiver/'getdaq-receiver'),('data_service',supervisor/'getdaq-supervisor'),('qt-model',model/'native-gui-tests')]:add(path,label+'/programs/'+name)
        merge(model);build_receipts(model,label+'/qt-model-build');add(model/'results-managed-fdt.txt',label+'/results-qt-model.txt')
        protocol=root/'build-unused-source-protocol' if label=='ordinary' else Path('/tmp/getdaq-managed-fdt-protocol-asan')
        add(protocol/'ecc-protocol-tests',label+'/programs/protocol')
        log=Path('/tmp/getdaq-managed-fdt-protocol-tests.txt' if label=='ordinary' else '/tmp/getdaq-managed-fdt-protocol-asan-tests.txt')
        add(log,label+'/results-protocol.txt')
    for name in ('commands.json','source-sha256.json','generated-sha256.json','dependency-sha256.json'):add(native/name,'sanitized/native-build/'+name)
    for path,digest in read(native/'generated-sha256.json').items():assert sha(Path(path))==digest;add(Path(path),'sanitized/native-build/generated/'+Path(path).name)
    for path in native.glob('*.compile.txt'):
        if 'credential' not in path.name.lower():add(path,'sanitized/native-build/logs/'+path.name)
    for path in native.glob('*.link.txt'):add(path,'sanitized/native-build/logs/'+path.name)
    for kind in ['qt-full','qt-console']:
        build=base/kind;merge(build);build_receipts(build,'ordinary/'+kind+'-build')
        program=build/read(build/'build-receipt.json')['target'];add(program,'ordinary/programs/'+('console' if kind=='qt-console' else 'qt-full'))
    add(base/'qt-full/results-managed-fdt.txt','ordinary/results-qt-full.txt')
    add(root/'build-ice-compat/hardware-data-sender-tests','ordinary/programs/sender-fixture')
    add(Path('/tmp/getdaq-managed-fdt-sender-tests.txt'),'ordinary/results-sender.txt')
    add(base/'integrated-normal-receipt.json','ordinary/integrated-normal-receipt.json')
    for name in ['tests/archive_managed_fdt_validation.py','tests/ecc_protocol_test.cpp','tests/hardware_data_sender_test.cpp',
                 'tests/run_managed_fdt_tests.py','tests/receiver_cycle_control_test.py','tests/receiver_cycle_http_test.py',
                 'tests/run_receiver_observation_tests.py','tests/run_file_naming_supervisor_tests.py','tests/run_supervisor_control_tests.py',
                 'tests/run_ecc_network_service_tests.py','tests/run_local_receiver_cleanup.py',
                 'docs/MANAGED_FDT.md','docs/FDT_RECEIVER_CYCLES.md','docs/DATA_SERVICE.md','docs/DATA_SENDER_CONTROL.md',
                 'docs/REMOTE_RECEIVER_CONTROL.md','docs/RECEIVER_TRANSPORTS.md','docs/MANAGED_ZBUF.md','docs/REFACTOR_STATUS.md',
                 'docs/ARCHITECTURE.md','docs/VALIDATION.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir()
    for name,path in files.items():dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
