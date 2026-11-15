#!/usr/bin/env python3
"""Actual metadata HTTP/native child/production dialog tests; owned raw always cleaned."""
import argparse
import http.client
import json
import os
import resource
from pathlib import Path
import select
import signal
import socket
import subprocess
import tempfile
import time
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def require(okay, why):
    if not okay:
        raise AssertionError(why)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'receiver', 'evidence'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--window-probe', type=Path)
    parser.add_argument('--native-only', action='store_true')
    parser.add_argument('--service', type=Path)
    parser.add_argument('--catalog', type=Path)
    args = parser.parse_args()
    build, receiver = args.build.resolve(), args.receiver.resolve()
    service_program = args.service.resolve() if args.service else build/'getdaq-supervisor'
    catalog_program = args.catalog.resolve() if args.catalog else build/'getdaq-catalog'
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='transfer-jobs-', dir=args.evidence)).resolve()
    temporary = folder / 'temporary'
    temporary.mkdir(mode=0o700)
    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    maximum = 64 * 1024**2  # Native fixtures, hardlink aliases and two HTTP copies combined.
    require(maximum < 20 * 1024**3, 'Aggregate test budget')
    env = dict(os.environ, GETDAQ_CATALOG_TEST_ROOT=str(temporary),
               XDG_DATA_HOME=str(folder / 'local-window-data'), QT_QPA_PLATFORM='offscreen')
    processes, logs, rows = [], [], []
    token_file = None

    def run(name, command, timeout=60):
        log = (folder / (name + '.log')).open('w')
        logs.append(log)
        p = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env, start_new_session=True)
        processes.append(p)
        code = p.wait(timeout=timeout)
        require(code == 0, name + ': ' + (folder / (name + '.log')).read_text()[-2000:])
        rows.append({'name': name, 'exit_code': code})

    try:
        run('native', [str(build / 'transfer-jobs-tests'), str(catalog_program)])
        if args.native_only:
            (folder / 'result.json').write_text(json.dumps({'passed': True, 'rows': rows,
                'scope': 'native-controller-and-copy-child', 'maximum_aggregate_bytes': maximum,
                'physical_hardware': False, 'program_sha256': {n: sha(build/n) for n in ('getdaq-catalog','transfer-jobs-tests')}},indent=2)+'\n')
            print('PASS native task controller and actual child;', folder)
            return
        fixture = subprocess.Popen([str(build / 'transfer-jobs-tests'), '--hold-fixture', str(receiver)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env=env, start_new_session=True, text=True)
        processes.append(fixture)
        require(select.select([fixture.stdout], [], [], 15)[0], 'Fixture creation deadline')
        info = json.loads(fixture.stdout.readline())
        root, plan = Path(info['root']), info['plan']
        token_file = root / 'credential'
        run('provision', [str(service_program), '--generate-token', str(token_file)])
        token = token_file.read_text().strip()
        with socket.socket() as bound:
            bound.bind(('127.0.0.1', 0))
            port = bound.getsockname()[1]
        config = {'schema': 1, 'kind': 'getdaq.supervisor-service', 'agent_id': 'node-A',
                  'controller_id': 'fixture-ecc', 'bind': '127.0.0.1', 'port': port,
                  'credential_file': str(token_file), 'receiver_plan': plan,
                  'transfers': {'backup_root': str(root / 'backups'), 'audit_root': str(root / 'jobs'),
                                'executable': str(catalog_program),
                                'executable_sha256': sha(catalog_program)}}
        config_file = root / 'service.json'
        config_file.write_text(json.dumps(config) + '\n')
        run('check-config', [str(service_program), '--config', str(config_file), '--check-config'])
        require(not (root / 'jobs/policy.json').exists(), 'Offline check created transfer policy')

        def request(method, path, body=None, auth=True):
            connection = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
            headers = {'Content-Type': 'application/json'}
            if auth:
                headers['Authorization'] = 'Bearer ' + token
            try:
                connection.request(method, path, json.dumps(body) if body is not None else None, headers)
                result = connection.getresponse()
                data = result.read()
                require(token.encode() not in data, 'Credential leaked')
                return result.status, json.loads(data)
            finally:
                connection.close()

        def start(index, file_limit=None):
            log = (folder / ('service-' + str(index) + '.log')).open('w')
            logs.append(log)
            def limit_child():
                signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
                soft,hard=resource.getrlimit(resource.RLIMIT_FSIZE)
                resource.setrlimit(resource.RLIMIT_FSIZE,(file_limit,hard))
            service = subprocess.Popen([str(service_program), '--config', str(config_file)],
                                       stdout=log, stderr=subprocess.STDOUT, env=env, start_new_session=True, preexec_fn=limit_child if file_limit else None)
            processes.append(service)
            deadline = time.monotonic() + 10
            while True:
                require(service.poll() is None, 'Service exited during startup')
                try:
                    code, identity = request('GET', '/v1/transfers/identity')
                    require(code == 200, 'Transfer identity GET')
                    return service, identity
                except (OSError, http.client.HTTPException):
                    require(time.monotonic() < deadline, 'Service startup deadline')
                    time.sleep(.025)

        service, identity = start(0)
        before = request('GET', '/v1/status')[1]
        require(request('GET', '/v1/transfers/identity', auth=False)[0] == 401, 'Identity without auth accepted')
        require(request('POST', '/v1/transfers/operations', {}, auth=False)[0] == 401, 'POST without auth accepted')
        require(request('POST', '/v1/transfers/operations', {})[0] == 400, 'Malformed request accepted')
        require(request('GET', '/v1/transfers/source/Test/0')[0] == 200, 'Source metadata unavailable')
        source = request('GET', '/v1/transfers/source/Test/0')[1]
        task = {'schema': 1, 'kind': 'getdaq.transfer-request', 'operation_id': 'http-copy',
                'instance_id': identity['instance_id'], 'policy_sha256': identity['policy_sha256'],
                'experiment': 'Test', 'run_number': 0, 'source_request_key': source['request_key']}
        require(request('POST', '/v1/transfers/operations', dict(task, instance_id='e'*64))[0] == 409, 'Wrong instance accepted')
        require(request('POST', '/v1/transfers/operations', task)[0] == 202, 'Copy not accepted')
        require(request('POST', '/v1/transfers/operations', task)[0] == 202, 'Duplicate ID failed observation')
        deadline = time.monotonic() + 20
        while True:
            code, operation = request('GET', '/v1/transfers/operations/http-copy')
            require(code == 200, 'Original operation unavailable')
            if operation['phase'] in ('complete', 'failed', 'unknown'):
                break
            require(time.monotonic() < deadline, 'Child completion deadline')
            time.sleep(.05)
        require(operation['phase'] == 'complete' and operation['receipt']['content_verified'], 'Native HTTP copy failed')
        child = (root / 'jobs/job-http-copy/child.json').read_bytes()
        require(request('POST', '/v1/transfers/operations', task)[0] == 202, 'Completed ID observation')
        require((root / 'jobs/job-http-copy/child.json').read_bytes() == child, 'Duplicate launched a child')
        require(request('POST', '/v1/transfers/operations', dict(task, run_number=1))[0] == 409, 'Changed same ID accepted')
        run('verify-copy', [str(catalog_program), '--verify-transfer', operation['destination']])
        require(request('GET', '/v1/status')[1] == before, 'Transfer changed receiver/ECC control state')
        # The client deliberately does not read the acceptance response. Wait for
        # the durable intent, disconnect, then recover exclusively by original ID.
        lost = dict(task, operation_id='lost-response-copy')
        encoded = json.dumps(lost).encode()
        with socket.create_connection(('127.0.0.1', port), timeout=3) as disconnected:
            header = ('POST /v1/transfers/operations HTTP/1.1\r\nHost: 127.0.0.1\r\n'
                      'Authorization: Bearer '+token+'\r\nContent-Type: application/json\r\n'
                      'Content-Length: '+str(len(encoded))+'\r\nConnection: close\r\n\r\n').encode()
            disconnected.sendall(header+encoded)
            deadline = time.monotonic()+10
            while not (root/'jobs/job-lost-response-copy/request.json').exists():
                require(time.monotonic()<deadline, 'Lost-response acceptance deadline')
                time.sleep(.01)
        while True:
            code, observed = request('GET', '/v1/transfers/operations/lost-response-copy')
            if code == 200 and observed['phase'] in ('complete','failed','unknown'):
                break
            require(time.monotonic()<deadline, 'Lost-response original-ID deadline')
            time.sleep(.025)
        require(observed['phase']=='complete', 'Disconnected submit did not complete original task')
        original_child = (root/'jobs/job-lost-response-copy/child.json').read_bytes()
        require(request('POST', '/v1/transfers/operations', lost)[0]==202 and
                (root/'jobs/job-lost-response-copy/child.json').read_bytes()==original_child,
                'Lost-response same-ID observation replayed child')
        if args.window_probe:
            run('window', [str(args.window_probe.resolve()), str(port), str(token_file), 'node-A', str(folder / 'window-result.json')])
            proof = json.loads((folder / 'window-result.json').read_text())
            require(proof['passed'] and len(list((root / 'jobs').glob('job-*'))) == 3, 'Cancel/confirmation created wrong number of jobs')
            require(request('GET', '/v1/transfers/operations/' + proof['confirmed_id'])[1]['phase'] == 'complete', 'Window original ID mismatch')
        service.send_signal(signal.SIGTERM)
        require(service.wait(timeout=10) == 0, 'Service graceful exit failed')
        service, reopened = start(1)
        require(reopened['instance_id'] != identity['instance_id'], 'Restart adopted old instance')
        require(request('GET', '/v1/transfers/operations/http-copy')[1]['phase'] == 'complete', 'History lost')
        require(request('POST', '/v1/transfers/operations', task)[0] == 202 and (root / 'jobs/job-http-copy/child.json').read_bytes() == child, 'Restart replayed original ID')
        service.send_signal(signal.SIGTERM)
        require(service.wait(timeout=10) == 0, 'Second graceful exit failed')
        saved = root / 'jobs/job-http-copy/operation.json'
        unfinished = json.loads(saved.read_text())
        unfinished['phase'], unfinished['receipt'] = 'running', None
        saved.write_text(json.dumps(unfinished) + '\n')
        if args.window_probe:
            second=root/'jobs/job-lost-response-copy/operation.json'
            second_record=json.loads(second.read_text());second_record['phase']='running';second_record['receipt']=None
            second.write_text(json.dumps(second_record)+'\n')
        service, reopened = start(2)
        require(request('GET', '/v1/transfers/operations/http-copy')[1]['phase'] == 'unknown', 'Unfinished restart promoted complete')
        another = dict(task, operation_id='fresh-copy', instance_id=reopened['instance_id'])
        require(request('POST', '/v1/transfers/operations', another)[0] == 409 and not (root / 'jobs/job-fresh-copy').exists(), 'Unfinished restart launched new copy')
        require((root / 'jobs/job-http-copy/child.json').read_bytes() == child, 'Unfinished restart adopted/replayed child')
        verify = {'schema':1, 'kind':'getdaq.transfer-reconciliation-request',
                  'verification_id':'http-verify', 'operation_id':'http-copy',
                  'instance_id':reopened['instance_id'], 'policy_sha256':reopened['policy_sha256'],
                  'original_request_key':request('GET','/v1/transfers/operations/http-copy')[1]['request_key']}
        require(request('POST','/v1/transfers/reconciliations',verify,auth=False)[0]==401,'Unauthenticated verification accepted')
        require(request('POST','/v1/transfers/reconciliations',{})[0]==400,'Malformed verification accepted')
        require(request('POST','/v1/transfers/reconciliations',dict(verify,instance_id=identity['instance_id']))[0]==409,'Verification adopted old service')
        require(request('POST','/v1/transfers/reconciliations',dict(verify,original_request_key='f'*64))[0]==409,'Verification rebound original request')
        before_verification=request('GET','/v1/status')[1]
        original_source=Path(operation['source'])
        offline_source=original_source.with_name(original_source.name+'-offline')
        original_source.rename(offline_source)
        require(request('POST','/v1/transfers/reconciliations',verify)[0]==202,'Manual full verification rejected')
        deadline=time.monotonic()+20
        while True:
            reconciled=request('GET','/v1/transfers/operations/http-copy')[1]
            if reconciled['phase'] in ('complete','unknown','failed'):break
            require(time.monotonic()<deadline,'Native verification deadline')
            time.sleep(.025)
        require(reconciled['phase']=='complete' and reconciled['request']['instance_id']==task['instance_id'],'Verification failed or changed original ownership')
        verifier=root/'jobs/job-http-copy/verify-http-verify/child.json'
        verification_child=verifier.read_bytes()
        require(request('POST','/v1/transfers/reconciliations',verify)[0]==202 and verifier.read_bytes()==verification_child,'Verification POST replayed child')
        require((root/'jobs/job-http-copy/child.json').read_bytes()==child,'Verification changed original copy PID')
        require(request('GET','/v1/status')[1]==before_verification,'Verification changed receiver/ECC state')
        if args.window_probe:
            require(request('POST','/v1/transfers/operations',another)[0]==409,'Other unresolved job lost its gate')
            run('verification-window',[str(args.window_probe.resolve()),str(port),str(token_file),'node-A',str(folder/'verification-window-result.json'),'lost-response-copy'])
            require(json.loads((folder/'verification-window-result.json').read_text())['confirmed_verification_complete'],'Production verification window did not complete original task')
            require(len(list((root/'jobs/job-lost-response-copy').glob('verify-*')))==1,'Window Cancel/Yes created more than one verification intent')
        offline_source.rename(original_source)
        require(request('POST','/v1/transfers/operations',another)[0]==202,'Reconciled copy still blocked fresh submission')
        deadline=time.monotonic()+20
        while request('GET','/v1/transfers/operations/fresh-copy')[1]['phase'] not in ('complete','failed','unknown'):
            require(time.monotonic()<deadline,'Fresh copy after reconciliation deadline');time.sleep(.025)
        require(request('GET','/v1/transfers/operations/fresh-copy')[1]['phase']=='complete','Fresh copy after reconciliation failed')
        service.send_signal(signal.SIGTERM)
        require(service.wait(timeout=10) == 0, 'Final graceful exit failed')
        # Fail actual copy children at 16 KiB; do not fabricate partial files.
        service, limited_identity=start(3,16384)
        partials={}
        ids=['resume-copy']+(['window-resume-copy'] if args.window_probe else [])
        for operation_id in ids:
            partial_task=dict(task,operation_id=operation_id,instance_id=limited_identity['instance_id'])
            require(request('POST','/v1/transfers/operations',partial_task)[0]==202,'Limited child not accepted')
            deadline=time.monotonic()+20
            while True:
                partial=request('GET','/v1/transfers/operations/'+operation_id)[1]
                if partial['phase'] in ('failed','unknown','complete'):break
                require(time.monotonic()<deadline,'Limited actual child deadline');time.sleep(.025)
            require(partial['phase']=='failed','Limited actual child did not fail')
            prefix=Path(partial['destination'])/'source_0/data0.graw'
            require(prefix.stat().st_size==16384,'Expected actual 16 KiB partial prefix')
            partials[operation_id]=(partial,prefix.stat().st_ino,(root/('jobs/job-'+operation_id)/'child.json').read_bytes())
        service.send_signal(signal.SIGTERM);require(service.wait(timeout=10)==0,'Limited service failed to exit')
        service,current=start(4);resume_before=request('GET','/v1/status')[1]
        partial,inode,original_pid=partials['resume-copy']
        resume={'schema':1,'kind':'getdaq.transfer-resume-request','verification_id':'http-resume',
                'operation_id':'resume-copy','instance_id':current['instance_id'],
                'policy_sha256':current['policy_sha256'],'original_request_key':partial['request_key']}
        require(request('POST','/v1/transfers/resumptions',resume,auth=False)[0]==401,'Unauthenticated resume accepted')
        require(request('POST','/v1/transfers/resumptions',{})[0]==400,'Malformed resume accepted')
        require(request('POST','/v1/transfers/resumptions',dict(resume,instance_id=limited_identity['instance_id']))[0]==409,'Resume adopted prior service')
        require(request('POST','/v1/transfers/resumptions',resume)[0]==202,'Original partial continuation rejected')
        deadline=time.monotonic()+20
        while True:
            resumed=request('GET','/v1/transfers/operations/resume-copy')[1]
            if resumed['phase'] in ('complete','failed','unknown'):break
            require(time.monotonic()<deadline,'Native continuation deadline');time.sleep(.025)
        require(resumed['phase']=='complete' and resumed['request']==partial['request'],'Continuation failed/rebound original intent')
        prefix=Path(resumed['destination'])/'source_0/data0.graw'
        require(prefix.stat().st_ino==inode and prefix.stat().st_size==1024*1024,'Continuation replaced original file or did not append')
        require((root/'jobs/job-resume-copy/child.json').read_bytes()==original_pid,'Continuation changed original child identity')
        recovery_child=root/'jobs/job-resume-copy/resume-http-resume/child.json';recovery_pid=recovery_child.read_bytes()
        require(request('POST','/v1/transfers/resumptions',resume)[0]==202 and recovery_child.read_bytes()==recovery_pid,'Resume request replayed child')
        if args.window_probe:
            run('resume-window',[str(args.window_probe.resolve()),str(port),str(token_file),'node-A',str(folder/'resume-window-result.json'),'window-resume-copy','resume'])
            proof=json.loads((folder/'resume-window-result.json').read_text());require(proof['resume'] and proof['confirmed_verification_complete'],'Actual resume window failed')
            require(len(list((root/'jobs/job-window-resume-copy').glob('resume-*')))==1,'Resume Cancel/Yes created wrong number of intents')
            recovered=request('GET','/v1/transfers/operations/window-resume-copy')[1];old,ino,pid=partials['window-resume-copy']
            require(recovered['phase']=='complete' and (Path(recovered['destination'])/'source_0/data0.graw').stat().st_ino==ino and (root/'jobs/job-window-resume-copy/child.json').read_bytes()==pid,'Window resume replaced raw file/adopted old PID')
        require(request('GET','/v1/status')[1]==resume_before,'Resume changed receiver/ECC status')
        service.send_signal(signal.SIGTERM);require(service.wait(timeout=10)==0,'Resume service exit failed')
        (folder / 'result.json').write_text(json.dumps({'passed': True, 'rows': rows,
            'actual_native_child': True, 'authenticated_http': True, 'receiver_state_unchanged': True,
            'duplicate_id_no_replay': True, 'unfinished_restart_unknown': True, 'manual_native_full_verification':True, 'verification_duplicate_no_replay':True, 'original_catalog_absent_during_verification':True,
            'lost_response_observed_by_original_id': True,
            'production_window': bool(args.window_probe), 'actual_partial_child_continuation':True, 'resume_duplicate_no_replay':True, 'prefix_inode_preserved':True, 'maximum_aggregate_bytes': maximum,
            'physical_hardware': False, 'throughput_benchmark': False,
            'program_sha256': {'getdaq-supervisor': sha(service_program), 'getdaq-catalog': sha(catalog_program), 'transfer-jobs-tests': sha(build/'transfer-jobs-tests')}}, indent=2)+'\n')
    finally:
        for p in reversed(processes):
            stop_owned_group(p)
        for log in logs:
            log.close()
        deleted = []
        for p in folder.rglob('*.graw'):
            if p.is_file() and not p.is_symlink():
                deleted.append({'path': str(p.relative_to(folder)), 'bytes': p.stat().st_size})
                p.unlink()
        for p in folder.rglob('credential'):
            if p.is_file() and not p.is_symlink():
                p.unlink()
        (folder / 'cleanup.json').write_text(json.dumps({'deleted': deleted, 'remaining_raw': list(map(str,folder.rglob('*.graw')))},indent=2)+'\n')
    print('PASS native jobs, authenticated HTTP, no duplicate/restart replay; owned raw removed;', folder)


if __name__ == '__main__':
    main()
