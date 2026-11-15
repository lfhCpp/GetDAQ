#!/usr/bin/env python3
"""Original accepted-but-failed retirement: explicit metadata completion, no hardware."""
import argparse,copy,hashlib,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_supervisor_control_tests import Rig,frame,require,wait_for
from run_session_retirement_service_tests import ports
from run_receiver_observation_tests import interrupted

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,root,mode):
    rig=None;saved_env={k:os.environ.get(k) for k in ['LD_PRELOAD','GETDAQ_TEST_RETIREMENT_PLAN','GETDAQ_TEST_RETIREMENT_MARKER','GETDAQ_TEST_RETIREMENT_BACKUP']}
    case=root/mode;plan_file=case/'next-inputs/next-plan.json';backup=case/'next-inputs/next-plan.backup'
    original_dir=case/'deployment/control/operations/retire-original'
    os.environ.update(GETDAQ_TEST_RETIREMENT_PLAN=str(plan_file),GETDAQ_TEST_RETIREMENT_BACKUP=str(backup),
        GETDAQ_TEST_RETIREMENT_MARKER=str(original_dir/('plan-retirement.json' if mode=='existing-proof' else 'request.json')))
    preload=str(args.injector)
    if args.asan_runtime:preload=str(args.asan_runtime)+':'+preload
    os.environ['LD_PRELOAD']=preload
    try:
        rig=Rig.__new__(Rig);rig.__init__(args,case,ports());old_identity=copy.deepcopy(rig.identity)
        next_plan=copy.deepcopy(rig.identity['plan']);next_plan.update(deployment_id='completed-successor',audit_root=str(case/'next-deployment'),configuration_sha256='1'*64,artifact_sha256='2'*64)
        next_config=copy.deepcopy(rig.config);next_config['receiver_plan']=next_plan
        plan_file.parent.mkdir(mode=0o700);next_config_file=plan_file.parent/'service.json';next_config_file.write_text(json.dumps(next_config)+'\n')
        prepared=subprocess.run([str(args.service),'--config',str(next_config_file),'--write-plan',str(plan_file)],capture_output=True,timeout=15)
        require(prepared.returncode==0,'Canonical plan preparation failed')
        plan_bytes=plan_file.read_bytes();next_sha=sha(plan_file)
        rig.start();rig.connect_senders();finished=rig.finish([frame(1),frame(2,True)])
        old_raw={p:sha(p) for p in case.rglob('*.graw')}
        original=rig.make_request('retire-plan','retire-original');original.update(expected_session_id=finished['result']['session_id'],successor_plan_file=str(plan_file),successor_plan_sha256=next_sha,successor_controller_snapshot_sha256='e'*64,successor_controller_epoch='f'*64)
        rig.submit(original);failure=rig.completed('retire-original','failed')
        require(backup.exists() and not plan_file.exists(),'Did not reach deterministic post-acceptance filesystem fault')
        backup.rename(plan_file);require(plan_file.read_bytes()==plan_bytes and plan_file.stat().st_mode&0o7777==0o400,'Reviewed source was not restored exactly')
        request_bytes=(original_dir/'request.json').read_bytes();old_failure=json.dumps(failure,sort_keys=True)
        completion=dict(original,operation_id='complete-original',command='complete-plan-retirement',failed_retirement_operation_id=original['operation_id'])
        require(rig.request('GET','/v1/plan-retirement-completion')[0]==503,'Failed retirement claims an explicit completed repair')
        require(rig.request('GET','/v1/plan-retirement-completion',auth=False)[0]==401,'Unauthenticated repair observation accepted')
        code,_=rig.request('POST','/v1/operations',completion,auth=False);require(code==401,'Unauthenticated completion accepted')
        for field,value in [('failed_retirement_operation_id','start'),('successor_controller_epoch','9'*64),('expected_session_id','8'*64)]:
            wrong=dict(completion);wrong[field]=value;wrong['operation_id']='wrong-'+field
            require(rig.request('POST','/v1/operations',wrong)[0] in (409,412),'Changed original binding accepted: '+field)
        require(not (case/'deployment/control/operations/complete-original').exists(),'Rejected preflight wrote intent')
        if mode=='cold':
            rig.stop_service();rig.start_service()
            require(rig.request('POST','/v1/operations',completion)[0]==412,'Old instance recovered from cold service')
            forged=dict(completion,instance_id=rig.identity['instance_id'])
            require(rig.request('POST','/v1/operations',forged)[0]==409,'Cold identity adopted failed retirement')
            return {'mode':mode,'passed':True,'cold_recovery_refused':True}
        if mode=='seal-fault':
            seal=Path(finished['result']['receivers'][0]['output_directory'])/'.getdaq-receiver-seal.json';before=seal.read_bytes()
            invalid=json.loads(before);invalid['complete']=False;seal.write_text(json.dumps(invalid)+'\n')
            require(rig.request('POST','/v1/operations',completion)[0]==409,'Changed completed cycle accepted')
            seal.write_bytes(before)
            require(rig.request('POST','/v1/operations',completion)[0]==409,'Restoration cleared lifecycle fault')
            return {'mode':mode,'passed':True,'independent_hold_preserved':True}
        existing_inode=None
        if mode=='partial-metadata':
            file=original_dir/'successor-plan.json';file.write_bytes(plan_bytes);file.chmod(0o600);existing_inode=file.stat().st_ino
        if mode=='different-metadata':
            file=original_dir/'cycle-evidence.json';file.write_bytes(b'not the original cycle\n');file.chmod(0o600)
        original_proof_sha=sha(original_dir/'plan-retirement.json') if mode=='existing-proof' else None
        request_file=case/'completion-request.json';request_file.write_text(json.dumps(completion)+'\n')
        response=rig.query('submit','--request-file',str(request_file));require(response['phase'] in ['accepted','running','complete'],'Native client did not submit exact completion')
        result=rig.completed(completion['operation_id'],'failed' if mode=='different-metadata' else 'complete')
        require(json.dumps(rig.operation(original['operation_id']),sort_keys=True)==old_failure,'Original terminal failure rewritten')
        require((original_dir/'request.json').read_bytes()==request_bytes,'Original request rewritten')
        require(all(sha(p)==digest for p,digest in old_raw.items()),'Completion changed raw bytes')
        if mode=='different-metadata':
            require(not (original_dir/'successor-plan.json').exists() and (original_dir/'cycle-evidence.json').read_bytes()==b'not the original cycle\n','Different metadata caused partial overwrite')
            return {'mode':mode,'passed':True,'different_existing_bytes_preserved':True}
        proof_file=original_dir/'plan-retirement.json';proof=json.loads(proof_file.read_text())
        require(result['plan_retirement']['proof_sha256']==sha(proof_file) and proof['request_key']==failure['request_key'],'Completion proof not bound to original request')
        if existing_inode is not None:require((original_dir/'successor-plan.json').stat().st_ino==existing_inode and (original_dir/'successor-plan.json').stat().st_mode&0o7777==0o400,'Exact partial file was overwritten or left unsealed')
        if original_proof_sha:require(sha(proof_file)==original_proof_sha,'Already published proof changed')
        journal_before=rig.journal();again=rig.query('submit','--request-file',str(request_file));require(again['phase']=='complete' and rig.journal()==journal_before,'Same ID replayed metadata completion')
        repeat=dict(completion,operation_id='another-completion');require(rig.request('POST','/v1/operations',repeat)[0]==409,'Another completion after success accepted')
        require(rig.request('POST','/v1/operations',rig.make_request('start','old-start'))[0]==409 and rig.status()['control_needs_reconciliation'],'Completion restored old positive control')
        code,exported=rig.request('GET','/v1/plan-retirement');require(code==200 and exported==proof,'Live endpoint lost completed original proof')
        code,observed=rig.request('GET','/v1/plan-retirement-completion')
        require(code==200 and observed['kind']=='getdaq.supervisor-plan-retirement-completion' and json.loads(observed['request'])==completion and observed['operation']==result,'Repair observation changed the completed request/result')
        if mode=='recovery-cold':
            rig.stop_service();rig.start_service()
            require(rig.operation(original['operation_id'])['phase']=='failed' and rig.operation(completion['operation_id'])['phase']=='complete','Cold replay changed original/completion history')
            require(rig.request('GET','/v1/plan-retirement')[0]==503 and rig.request('POST','/v1/operations',rig.make_request('start','cold-start'))[0]==409,'Cold replay restored live proof or control')
            require(rig.request('GET','/v1/plan-retirement-completion')[0]==503,'Cold instance advertised old repair as live')
            return {'mode':mode,'passed':True,'original_failure_preserved_on_reopen':True,'cold_control_fenced':True}
        rig.stop_service();rig.plan=next_plan;rig.config=next_config;rig.config_file=next_config_file
        log=(case/'successor.log').open('w');rig.logs.append(log)
        rig.process=subprocess.Popen([str(args.service),'--config',str(next_config_file),'--predecessor-proof',str(proof_file)],stdout=log,stderr=subprocess.STDOUT)
        def identity():
            require(rig.process.poll() is None,'Successor rejected completed original failure history')
            try:
                code,value=rig.request('GET','/v1/identity');return value if code==200 else None
            except (OSError,ConnectionError):return None
        rig.identity=wait_for(identity)
        require(rig.identity['instance_id']!=old_identity['instance_id'] and rig.identity['plan_sha256']==next_sha,'Successor did not use the new plan')
        old_make=rig.make_request
        def current(command,operation):
            value=old_make(command,operation);value.update(configuration_sha256='1'*64,artifact_sha256='2'*64,controller_snapshot_sha256='e'*64,controller_epoch='f'*64);return value
        rig.make_request=current
        require(not rig.status()['control_needs_reconciliation'],'Successor inherited an unrelated old hold')
        rig.start('successor-start');rig.connect_senders();rig.finish([frame(3,True),frame(4)],'successor-finish')
        require(all(sha(p)==digest for p,digest in old_raw.items()),'Successor changed old raw')
        return {'mode':mode,'passed':True,'original_failed_preserved':True,'old_control_fenced':True,'explicit_successor_cycle':True,'proof_sha256':sha(proof_file)}
    finally:
        if rig is not None:rig.close()
        for key,value in saved_env.items():
            if value is None:os.environ.pop(key,None)
            else:os.environ[key]=value

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--service',type=Path,required=True);ap.add_argument('--receiver',type=Path,required=True);ap.add_argument('--client',type=Path,required=True);ap.add_argument('--injector',type=Path,required=True);ap.add_argument('--evidence',type=Path,required=True);ap.add_argument('--asan-runtime',type=Path);args=ap.parse_args()
    for name in ['service','receiver','client','injector']:setattr(args,name,getattr(args,name).resolve(strict=True))
    args.query=args.client
    if args.asan_runtime is None:
        linked=subprocess.check_output(['ldd',str(args.service)],text=True)
        for line in linked.splitlines():
            if line.strip().startswith('libasan.so') and '=>' in line:args.asan_runtime=Path(line.split('=>',1)[1].strip().split()[0])
    if args.asan_runtime:args.asan_runtime=args.asan_runtime.resolve(strict=True)
    if args.asan_runtime:
        os.environ.setdefault('ASAN_OPTIONS','detect_leaks=1:halt_on_error=1')
        os.environ.setdefault('UBSAN_OPTIONS','halt_on_error=1:print_stacktrace=1')
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='failed-retirement-',dir=args.evidence)).resolve();rows=[];deleted=[]
    try:
        for mode in ['missing-metadata','existing-proof','partial-metadata','different-metadata','seal-fault','cold','recovery-cold']:rows.append(run(args,root,mode))
    finally:
        for p in root.rglob('*.graw'):
            if p.is_file() and not p.is_symlink():deleted.append({'path':str(p.relative_to(root)),'bytes':p.stat().st_size});p.unlink()
        for p in root.rglob('credential'):
            if p.is_file() and not p.is_symlink():p.unlink()
        (root/'cleanup.json').write_text(json.dumps({'deleted':deleted,'raw_remaining':len(list(root.rglob('*.graw')))},indent=2)+'\n')
    require(len(rows)==7 and not list(root.rglob('*.graw')),'Incomplete cases or cleanup')
    for log in root.rglob('*.log'):
        text=log.read_text(errors='replace')
        require(not any(marker in text for marker in ['ERROR: AddressSanitizer','ERROR: LeakSanitizer','AddressSanitizer:DEADLYSIGNAL','runtime error:']), 'Sanitizer failure: '+str(log))
    (root/'result.json').write_text(json.dumps({'passed':True,'cases':rows,'program_sha256':{k:sha(getattr(args,k)) for k in ['service','receiver','client','injector']},'runner_sha256':sha(Path(__file__)),'raw_remaining':0,'maximum_aggregate_test_bytes':64*1024**2,'real_hardware':False},indent=2)+'\n')
    print('PASS seven accepted-failed retirement completion scenarios; original failure retained; explicit successor cycle; raw removed:',root)
if __name__=='__main__':main()
