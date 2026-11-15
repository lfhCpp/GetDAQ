#!/usr/bin/env python3
"""Different receiver plans through original normal retirement and explicit successor startup."""
import argparse
import copy
import fcntl
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
from run_controller_handoff_tests import DropHandoffReply
from run_receiver_observation_tests import interrupted
from run_session_retirement_service_tests import ports
from run_supervisor_control_tests import Rig,frame,require,wait_for

CASES=['success','ack-loss','held-lease','copy-proof','tamper-seal','tamper-payload-size','changed-next-plan','retirement-cold','input-tamper-after-start','audit-tamper-after-start','lease-replaced-after-start','archive-lock-after-start']


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args,root,mode):
    rig=Rig.__new__(Rig);relay=None;archive_lease=None
    try:
        rig.__init__(args,root/mode,ports());old_plan=copy.deepcopy(rig.plan);old_config=copy.deepcopy(rig.config)
        old_identity=copy.deepcopy(rig.identity)
        next_plan=copy.deepcopy(rig.identity['plan'])
        next_plan.update(deployment_id='fixture-next',audit_root=str(rig.root/'next-deployment'),configuration_sha256='1'*64,artifact_sha256='2'*64)
        for source in next_plan['sources']:source.update(queue_mib=2,max_frame_mib=2,rotate_mib=2)
        inputs=rig.root/'next-inputs';inputs.mkdir(mode=0o700)
        next_config=copy.deepcopy(old_config);next_config['receiver_plan']=next_plan
        next_config_file=inputs/'service.json';next_config_file.write_text(json.dumps(next_config,indent=2)+'\n')
        plan_file=inputs/'next-plan.json'
        prepared=subprocess.run([str(args.service),'--config',str(next_config_file),'--write-plan',str(plan_file)],capture_output=True,timeout=15)
        (inputs/'write-plan.stdout').write_bytes(prepared.stdout);(inputs/'write-plan.stderr').write_bytes(prepared.stderr)
        require(prepared.returncode==0,'Offline canonical plan writer failed: '+prepared.stderr.decode(errors='replace'))
        plan_bytes=plan_file.read_bytes();next_sha=sha(plan_file)
        require(json.loads(prepared.stdout)['plan_sha256']==next_sha and plan_file.stat().st_mode&0o7777==0o400 and not Path(next_plan['audit_root']).exists(),'Offline plan writer changed state or produced wrong bytes')
        execution='stage-'+mode
        staged_options=copy.deepcopy(old_identity['plan'])
        staged_options.update(deployment_id='scan-'+hashlib.sha256(execution.encode()).hexdigest()[:24]+'-point-000',
            audit_root=old_plan['audit_root']+'.scan-'+execution+'/point-000',configuration_sha256='3'*64,artifact_sha256='4'*64)
        staged_config=copy.deepcopy(old_config);staged_config['receiver_plan']=staged_options
        staged_config_file=inputs/'staged-service.json';staged_config_file.write_text(json.dumps(staged_config,indent=2)+'\n')
        staged_source=inputs/'staged-source-plan.json'
        staged_created=subprocess.run([str(args.service),'--config',str(staged_config_file),'--write-plan',str(staged_source)],capture_output=True,timeout=15)
        require(staged_created.returncode==0,'Could not create canonical scan successor fixture: '+staged_created.stderr.decode(errors='replace'))
        staged_directory=inputs/'staged';staged_directory.mkdir(mode=0o700)
        staged_target=staged_directory/('scan-'+execution+'-point-000.json')
        stage=subprocess.run([str(args.service),'--config',str(rig.config_file),'--stage-successor-plan',str(staged_source),
            '--stage-plan-target',str(staged_target)],capture_output=True,timeout=15)
        require(stage.returncode==0,'Receiver-host scan successor staging failed: '+stage.stderr.decode(errors='replace'))
        stage_receipt=json.loads(stage.stdout);scan_parent=Path(staged_options['audit_root']).parent
        require(stage_receipt['staged'] and not stage_receipt['plan_retired'] and not stage_receipt['receiver_started'] and
            stage_receipt['plan_sha256']==sha(staged_source) and staged_target.read_bytes()==staged_source.read_bytes() and
            staged_target.stat().st_mode&0o7777==0o400 and scan_parent.is_dir() and scan_parent.stat().st_mode&0o7777==0o700 and
            not Path(staged_options['audit_root']).exists(),'Staging receipt, sealed plan or audit-parent boundary is invalid')
        duplicate_stage=subprocess.run([str(args.service),'--config',str(rig.config_file),'--stage-successor-plan',str(staged_source),
            '--stage-plan-target',str(staged_target)],capture_output=True,timeout=15)
        require(duplicate_stage.returncode==2 and sha(staged_target)==sha(staged_source),'Successor staging overwrote an existing plan')
        invalid_execution=execution+'-invalid'
        invalid_options=copy.deepcopy(staged_options)
        invalid_options.update(deployment_id='scan-'+hashlib.sha256(invalid_execution.encode()).hexdigest()[:24]+'-point-000',
            audit_root=old_plan['audit_root']+'.scan-'+invalid_execution+'/point-000')
        invalid_options['deployment_id']='scan-'+('0'*24)+'-point-000'
        invalid_config=copy.deepcopy(old_config);invalid_config['receiver_plan']=invalid_options
        invalid_config_file=inputs/'invalid-staged-service.json';invalid_config_file.write_text(json.dumps(invalid_config,indent=2)+'\n')
        invalid_source=inputs/'invalid-staged-source-plan.json'
        invalid_created=subprocess.run([str(args.service),'--config',str(invalid_config_file),'--write-plan',str(invalid_source)],capture_output=True,timeout=15)
        require(invalid_created.returncode==0,'Invalid successor binding fixture could not be encoded')
        invalid_target=staged_directory/'invalid-plan.json'
        rejected_stage=subprocess.run([str(args.service),'--config',str(rig.config_file),'--stage-successor-plan',str(invalid_source),
            '--stage-plan-target',str(invalid_target)],capture_output=True,timeout=15)
        require(rejected_stage.returncode==2 and not invalid_target.exists() and
            not Path(old_plan['audit_root']+'.scan-'+invalid_execution).exists(),
            'Invalid scan deployment binding created plan or audit state')
        def retirement(ident):
            value=rig.make_request('retire-plan',ident)
            value.update(expected_session_id=rig.status()['lifecycle']['session_id'] or '0'*64,
                successor_plan_file=str(plan_file),successor_plan_sha256=next_sha,
                successor_controller_snapshot_sha256='e'*64,successor_controller_epoch='f'*64)
            return value
        require(rig.request('POST','/v1/operations',retirement('fresh'))[0]==409,'Fresh unowned plan was retired')
        if mode=='success':
            remote_execution='http-'+mode
            remote_options=copy.deepcopy(old_identity['plan'])
            remote_options.update(deployment_id='scan-'+hashlib.sha256(remote_execution.encode()).hexdigest()[:24]+'-point-000',
                audit_root=old_plan['audit_root']+'.scan-'+remote_execution+'/point-000',configuration_sha256='5'*64,artifact_sha256='6'*64)
            long_output=rig.root/'long-output';long_output.mkdir(mode=0o700)
            long_output_text=str(long_output)
            while len(long_output_text)<3150:
                long_output_text+='/'+('d'*160);Path(long_output_text).mkdir(mode=0o700)
            require(len(long_output_text)<3500,'Large request fixture path exceeds the plan path bound')
            remote_options['sources'][0]['output_root']=long_output_text
            remote_config=copy.deepcopy(old_config);remote_config['receiver_plan']=remote_options
            remote_config_file=inputs/'remote-staged-service.json';remote_config_file.write_text(json.dumps(remote_config,indent=2)+'\n')
            remote_source=inputs/'remote-staged-source-plan.json'
            remote_created=subprocess.run([str(args.service),'--config',str(remote_config_file),'--write-plan',str(remote_source)],capture_output=True,timeout=15)
            require(remote_created.returncode==0,'Could not create remote-stage receiver plan: '+remote_created.stderr.decode(errors='replace'))
            remote_directory=inputs/'remote-staged';remote_directory.mkdir(mode=0o700)
            remote_target=remote_directory/'scan-http-success-point-000.json'
            remote_request=rig.make_request('stage-successor-plan','stage-http-success')
            remote_request.update(successor_plan_file=str(remote_target),successor_plan_sha256=sha(remote_source),
                successor_plan=json.loads(remote_source.read_text()),parent_audit_reference='p'*3400)
            request_file=inputs/'remote-stage-request.json';request_file.write_text(json.dumps(remote_request,separators=(',',':'))+'\n')
            os.chmod(request_file,0o600)
            require(request_file.stat().st_size>8192 and request_file.stat().st_size<2*1024*1024,
                'Remote-stage fixture does not exercise the narrow large-request budget')
            accepted_code,accepted=rig.request('POST','/v1/operations',remote_request)
            require(accepted_code==202,'Authenticated remote staging was rejected before acceptance: '+str(accepted))
            require(accepted['command']=='stage-successor-plan','Remote endpoint accepted a different operation')
            remote_operation=rig.completed('stage-http-success')
            remote_receipt=remote_operation['result']
            if isinstance(remote_receipt,str):remote_receipt=json.loads(remote_receipt)
            remote_scan_parent=Path(remote_options['audit_root']).parent
            require(remote_operation['phase']=='complete' and isinstance(remote_receipt,dict) and
                remote_receipt['kind']=='getdaq.staged-scan-successor-plan' and
                remote_receipt['staged'] and not remote_receipt['plan_retired'] and not remote_receipt['receiver_started'] and
                remote_target.read_bytes()==remote_source.read_bytes() and remote_target.stat().st_mode&0o7777==0o400 and
                remote_scan_parent.is_dir() and remote_scan_parent.stat().st_mode&0o7777==0o700 and
                not Path(remote_options['audit_root']).exists(),
                'Authenticated remote staging changed receiver state or produced an unsafe/incomplete sealed plan')
            replay_code,replay=rig.request('POST','/v1/operations',remote_request)
            require(replay_code==202 and replay['operation_id']=='stage-http-success' and replay['phase']=='complete' and
                replay['result']==remote_receipt and
                remote_target.read_bytes()==remote_source.read_bytes(),'Remote staging replay did not remain bound to its exact completed operation')
        preview=subprocess.run([str(args.service),'--config',str(next_config_file),'--plan-config'],capture_output=True,timeout=15)
        require(preview.returncode==0 and preview.stdout==plan_bytes,'Offline plan preview differs from sealed canonical bytes')
        duplicate=subprocess.run([str(args.service),'--config',str(next_config_file),'--write-plan',str(plan_file)],capture_output=True,timeout=15)
        require(duplicate.returncode==2 and sha(plan_file)==next_sha,'Canonical plan writer overwrote reviewed identity')
        rig.start();require(rig.request('POST','/v1/operations',retirement('active'))[0]==409,'Active receiver plan was retired')
        expected=[frame(1),frame(2,True)];rig.connect_senders();finished=rig.finish(expected)
        old_status=rig.status();old_raw={p:sha(p) for p in rig.root.rglob('*.graw')}
        request=retirement('retire-plan')
        wrong=dict(request,operation_id='wrong-session',expected_session_id='0'*64)
        require(rig.request('POST','/v1/operations',wrong)[0]==412,'Wrong original cycle retired plan')
        wrong=dict(request,operation_id='foreign',controller_epoch='0'*64)
        require(rig.request('POST','/v1/operations',wrong)[0]==409,'Foreign owner retired plan')
        require(rig.request('POST','/v1/operations',request,auth=False)[0]==401,'Unauthenticated retirement accepted')
        if mode=='success':
            invalid=[]
            value=copy.deepcopy(next_plan);value['max_total_queue_mib']=old_plan['max_total_queue_mib']+1;invalid.append(('memory-policy',value))
            value=copy.deepcopy(next_plan);value['audit_root']=str(rig.root/'missing-parent/next');invalid.append(('missing-parent',value))
            value=copy.deepcopy(next_plan);value['audit_root']=str(Path(old_plan['audit_root'])/'nested-next');invalid.append(('nested-root',value))
            value=copy.deepcopy(next_plan);value['sources'][0]['output_root']=str(rig.root/'missing-output');invalid.append(('missing-output',value))
            value=copy.deepcopy(next_plan);value['executable']='/bin/true';value['executable_sha256']=sha(Path('/bin/true'));invalid.append(('receiver-replacement',value))
            for label,value in invalid:
                config=copy.deepcopy(next_config);config['receiver_plan']=value;file=inputs/(label+'-service.json');file.write_text(json.dumps(config))
                target=inputs/(label+'-plan.json')
                command=subprocess.run([str(args.service),'--config',str(file),'--write-plan',str(target)],capture_output=True,timeout=15)
                require(command.returncode==0,'Invalid-target fixture itself did not satisfy static schema')
                rejected=dict(request,operation_id='invalid-'+label,successor_plan_file=str(target),successor_plan_sha256=sha(target))
                require(rig.request('POST','/v1/operations',rejected)[0] in (400,409,412),'Original plan accepted unsupported target: '+label)
            require(not rig.status()['control_needs_reconciliation'],'Rejected target preflights changed original control')
        if mode=='changed-next-plan':
            plan_file.chmod(0o600);plan_file.write_bytes(plan_bytes+b' ');plan_file.chmod(0o400)
            require(rig.request('POST','/v1/operations',request)[0] in (400,409,412),'Changed reviewed plan was accepted')
            require(not rig.status()['control_needs_reconciliation'],'Unaccepted invalid target changed original control')
            require(not any(row.get('event')=='accepted' and json.loads(row['request'])['command']=='retire-plan' for row in rig.journal()),'Invalid next plan acquired retirement intent')
            return dict(case=mode,passed=True,retirement_accepted=False)
        if mode=='ack-loss':
            relay=DropHandoffReply(rig);connection=http.client.HTTPConnection('127.0.0.1',relay.server.server_port,timeout=4)
            lost=False
            try:
                connection.request('POST','/v1/operations',json.dumps(request),{'Authorization':'Bearer '+rig.token,'Content-Type':'application/json'})
                connection.getresponse().read()
            except (ConnectionError,OSError,http.client.HTTPException):lost=True
            finally:connection.close()
            require(lost and relay.posts==1,'Retirement reply was not lost after one real POST');relay.close();relay=None
        else:rig.submit(request)
        operation=rig.completed('retire-plan');require(operation['plan_retirement']['successor_plan_sha256']==next_sha,'Retirement target differs')
        code,proof=rig.request('GET','/v1/plan-retirement');require(code==200,'Original retirement proof unavailable')
        proof_file=Path(old_plan['audit_root'])/'control/operations/retire-plan/plan-retirement.json'
        require(proof==json.loads(proof_file.read_text()) and sha(proof_file)==operation['plan_retirement']['proof_sha256'],'Original retirement proof digest differs')
        retired_status=rig.status()
        require(not retired_status['control_needs_reconciliation'] and retired_status['controller_epoch']=='d'*64,
            'Successful retirement did not preserve the original owner or clear reconciliation')
        for name in ['start','finish','verify-stopped']:
            require(rig.request('POST','/v1/operations',rig.make_request(name,'fenced-'+name))[0]==409,'Original retired plan accepted new command')
        if mode!='ack-loss':
            code,cached=rig.request('POST','/v1/operations',request);require(code==202 and cached['phase']=='complete','Same retirement request did not remain cached')
        require(sum(row['event']=='accepted' and json.loads(row['request'])['command']=='retire-plan' for row in rig.journal())==1,'Retirement was replayed')
        def launch(config,proof,okay=True,label='next'):
            log=(rig.root/(label+'.log')).open('w');rig.logs.append(log)
            command=[str(args.service),'--config',str(config),'--predecessor-proof',str(proof)]
            if not okay:
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
                require(process.wait(timeout=15)==2,'Invalid successor unexpectedly started');return None
            rig.process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
            def identity():
                require(rig.process.poll() is None,'Successor service exited: '+Path(log.name).read_text()[-2500:])
                try:
                    code,value=rig.request('GET','/v1/identity');return value if code==200 else None
                except (ConnectionError,OSError,http.client.HTTPException):return None
            return wait_for(identity)
        if mode=='held-lease':
            config=copy.deepcopy(next_config);config['port']=ports()[0];file=inputs/'held-service.json';file.write_text(json.dumps(config))
            launch(file,proof_file,False,'old-still-held')
            require(not Path(str(proof_file)+'.activation.json').exists() and not Path(next_plan['audit_root']).exists(),'Held predecessor consumed new generation')
        rig.stop_service()
        if mode=='retirement-cold':
            rig.start_service();require(rig.status()['control_needs_reconciliation'],'Cold original cleared retired plan')
            require(rig.request('POST','/v1/operations',rig.make_request('start','cold-start'))[0]==409,'Cold retired plan launched receiver')
            require(rig.request('GET','/v1/plan-retirement')[0]==503,'Cold instance exported original live retirement')
            rig.stop_service();launch(next_config_file,proof_file,False,'cold-predecessor-held')
            return dict(case=mode,passed=True,cold_original_fenced=True)
        if mode in ('tamper-seal','tamper-payload-size','copy-proof'):
            if mode=='tamper-seal':
                changed=Path(finished['result']['receivers'][0]['output_directory'])/'.getdaq-receiver-seal.json';changed.write_bytes(changed.read_bytes()+b' ')
            elif mode=='tamper-payload-size':
                changed=next(iter(old_raw));changed.write_bytes(changed.read_bytes()[:-1])
            else:
                import shutil
                destination=inputs/'copied-proof';destination.mkdir(mode=0o700)
                for name in ['request.json','successor-plan.json','plan-retirement.json','cycle-evidence.json']:shutil.copy2(proof_file.parent/name,destination/name)
                proof_file=destination/'plan-retirement.json'
            launch(next_config_file,proof_file,False,'invalid-predecessor')
            require(not Path(next_plan['audit_root']).exists(),'Invalid proof created successor root')
            return dict(case=mode,passed=True,successor_refused=True)
        rig.plan=next_plan;rig.config=next_config;rig.config_file=next_config_file
        rig.identity=launch(next_config_file,proof_file)
        require(rig.identity['plan_sha256']==next_sha and rig.identity['instance_id']!=old_identity['instance_id'],'Successor used old plan/instance')
        status=rig.status();require(status['lifecycle']['state']=='fresh' and not status['control_needs_reconciliation'] and status['controller_snapshot_sha256']=='e'*64 and status['controller_epoch']=='f'*64,'Successor lacks exact pre-authorized owner')
        code,previous=rig.request('GET','/v1/predecessor');require(code==200 and previous['proof']==proof and previous['proof_sha256']==sha(proof_file),'Successor lost original proof')
        rig.query('predecessor')
        original_request=rig.make_request
        def current(command,ident):
            value=original_request(command,ident);value.update(configuration_sha256=next_plan['configuration_sha256'],artifact_sha256=next_plan['artifact_sha256'],controller_snapshot_sha256='e'*64,controller_epoch='f'*64);return value
        rig.make_request=current
        if mode=='archive-lock-after-start':
            archive_lease=open(Path(finished['result']['receivers'][0]['output_directory'])/'.getdaq-receiver.lock','rb')
            fcntl.flock(archive_lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
            archive=rig.root/'archived-original';archive.mkdir(mode=0o700)
            moved={}
            for index,receiver in enumerate(finished['result']['receivers']):
                source=Path(receiver['output_directory']);destination=archive/str(index)
                source.rename(destination)
                for path,digest in old_raw.items():
                    if path.parent==source:moved[destination/path.name]=digest
            require(len(moved)==len(old_raw),'Archive did not relocate all original payloads')
            old_raw=moved
            require(rig.request('GET','/v1/predecessor')[0]==200,'Old archive reader lease blocked a different active data plan')
        stale=copy.deepcopy(request);stale.update(command='start',operation_id='old-generation');
        for key in ['expected_session_id','successor_plan_file','successor_plan_sha256','successor_controller_snapshot_sha256','successor_controller_epoch']:stale.pop(key)
        require(rig.request('POST','/v1/operations',stale)[0]==412,'Successor accepted old instance/plan')
        foreign=current('start','old-owner');foreign.update(controller_snapshot_sha256='c'*64,controller_epoch='d'*64)
        require(rig.request('POST','/v1/operations',foreign)[0]==409,'Successor accepted original controller owner')
        if mode in ('input-tamper-after-start','audit-tamper-after-start','lease-replaced-after-start'):
            if mode=='lease-replaced-after-start':
                seal=Path(old_plan['audit_root'])/'.supervisor.lock';backup=seal.with_name('.supervisor.lock.original')
                seal.rename(backup);seal.write_bytes(backup.read_bytes());seal.chmod(backup.stat().st_mode&0o7777)
                require(rig.request('GET','/v1/predecessor')[0]==503,'Live successor ignored retired deployment lease replacement')
                seal.unlink();backup.rename(seal)
            else:
                seal=proof_file.parent/'cycle-evidence.json' if mode=='input-tamper-after-start' else Path(old_plan['audit_root'])/'supervisor.jsonl'
                saved=seal.read_bytes();permissions=seal.stat().st_mode&0o7777;seal.chmod(0o600);seal.write_bytes(saved+b' ')
                require(rig.request('GET','/v1/predecessor')[0]==503,'Live successor ignored predecessor metadata drift');seal.write_bytes(saved);seal.chmod(permissions)
            require(rig.request('GET','/v1/predecessor')[0]==503 and rig.request('POST','/v1/operations',current('start','restored'))[0]==409,'Restoration cleared live predecessor hold')
        else:
            rig.start('new-start');arguments={}
            for receiver in rig.status()['lifecycle']['receivers']:
                words=(Path('/proc')/str(receiver['pid'])/'cmdline').read_bytes().split(b'\0');arguments[receiver['id']]=[x.decode() for x in words if x]
                for flag in ['--queue-mib','--max-frame-mib','--rotate-mib']:require(arguments[receiver['id']][arguments[receiver['id']].index(flag)+1]=='2','New receiver did not use changed parameters')
            (rig.root/'new-receiver-arguments.json').write_text(json.dumps(arguments,indent=2)+'\n')
            rig.connect_senders();rig.finish([frame(3,True),frame(4)],'new-finish')
        require(all(sha(path)==digest for path,digest in old_raw.items()),'Transition changed original raw bytes')
        (rig.root/'successor-final-status.json').write_text(json.dumps(rig.status(),indent=2)+'\n')
        rig.stop_service();launch(next_config_file,proof_file,False,'consumed-restart')
        return dict(case=mode,passed=True,changed_configuration_verified=True,original_raw_preserved=True,successor_epoch_verified=True)
    finally:
        if archive_lease:archive_lease.close()
        if relay:relay.close()
        if getattr(rig,'root',None):rig.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['service','query','receiver','evidence']:parser.add_argument(name,type=Path)
    parser.add_argument('--cases',nargs='+',choices=CASES,default=CASES)
    parser.add_argument('--raw-read-guard',type=Path)
    parser.add_argument('--sanitized',action='store_true')
    args=parser.parse_args()
    for name in ['service','query','receiver']:setattr(args,name,getattr(args,name).resolve(strict=True))
    args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='data-plan-transition-',dir=args.evidence)).resolve()
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    actual_service=args.service;before={name:sha(getattr(args,name)) for name in ['service','query','receiver']};results=[];deleted=[]
    runner_sha=sha(Path(__file__).resolve())
    if args.sanitized:
        os.environ['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';os.environ['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
    if args.raw_read_guard:
        require(not args.sanitized,'Use ordinary code for the payload read guard; sanitizer group separately instruments the same sources')
        guard=args.raw_read_guard.resolve(strict=True);log=root/'supervisor-raw-reads.log';self_log=root/'guard-selfcheck.log'
        sample=root/'guard-selfcheck.graw';sample.write_bytes(b'x')
        environment=dict(os.environ,LD_PRELOAD=str(guard),GETDAQ_RAW_READ_LOG=str(self_log))
        try:
            check=subprocess.run([sys.executable,'-c','import sys\ntry: open(sys.argv[1],"rb").read()\nexcept PermissionError: sys.exit(0)\nsys.exit(1)',str(sample)],env=environment,timeout=10)
            require(check.returncode==0 and self_log.read_bytes()==b'raw-read-denied\n','Raw read guard did not deny actual file read')
        finally:sample.unlink(missing_ok=True)
        wrapper=root/'guarded-supervisor.py'
        wrapper.write_text('#!/usr/bin/env python3\nimport os,sys\nos.environ["LD_PRELOAD"]='+repr(str(guard))+'\nos.environ["GETDAQ_RAW_READ_LOG"]='+repr(str(log))+'\nos.execv('+repr(str(args.service))+',['+repr(str(args.service))+',*sys.argv[1:]])\n')
        wrapper.chmod(0o700);before['service_wrapper']=sha(wrapper);before['raw_read_guard']=sha(guard)
        args.service=wrapper
    try:
        for mode in args.cases:results.append(run(args,root,mode))
    finally:
        for path in root.rglob('*.graw'):
            deleted.append(dict(file=str(path.relative_to(root)),bytes=path.stat().st_size,sha256=sha(path)));path.unlink()
        (root/'cleanup.json').write_text(json.dumps(dict(deleted=deleted,raw_remaining=len(list(root.rglob('*.graw')))),indent=2)+'\n')
    require(sum(x['bytes'] for x in deleted)<=2*1024**2,'Aggregate synthetic test payload exceeded 2 MiB')
    for name in ['query','receiver']:require(before[name]==sha(getattr(args,name)),'Program changed during test')
    require(before['service']==sha(actual_service),'Service binary changed during test')
    if args.raw_read_guard:
        require(not log.exists() or not log.read_bytes(),'Supervisor attempted to open raw payloads for reading')
        require(before['service_wrapper']==sha(wrapper) and before['raw_read_guard']==sha(guard),'Guard/launcher changed during test')
    else:require(before['service']==sha(args.service),'Service changed during test')
    require(runner_sha==sha(Path(__file__).resolve()),'Runner changed during validation')
    (root/'result.json').write_text(json.dumps(dict(passed=True,cases=results,program_sha256=before,runner_sha256=runner_sha,raw_remaining=0,maximum_aggregate_test_bytes=2*1024**2,aggregate_user_limit_bytes=20*1024**3,real_hardware=False,sanitized=args.sanitized,leak_detection=args.sanitized,raw_read_guard_enabled=bool(args.raw_read_guard)),indent=2)+'\n')
    print('PASS',len(results),'actual data plan transition cases; changed raw receiver parameters; owned payload removed;',root)


if __name__=='__main__':main()
