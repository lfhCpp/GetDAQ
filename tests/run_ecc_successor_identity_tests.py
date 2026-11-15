#!/usr/bin/env python3
"""Exercise frozen successor identities offline, rejecting and recording all network calls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['service','fixture','guard','configuration','evidence']:
        parser.add_argument(name,type=Path)
    parser.add_argument('--asan-runtime',type=Path)
    args=parser.parse_args()
    for name in ['service','fixture','guard','configuration']:
        setattr(args,name,getattr(args,name).resolve(strict=True))
    args.evidence.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='successor-identity-',dir=args.evidence)).resolve()
    credential=root/'credential';credential.write_text(os.urandom(32).hex()+'\n');credential.chmod(0o600)
    base=json.loads(args.configuration.read_text());results=[]
    def config(directory):
        directory.mkdir(mode=0o700)
        value=json.loads(json.dumps(base));value['credential_file']=str(credential)
        for endpoint in value['remote_receivers']['endpoints']:
            endpoint['credential_file']=str(credential)
        value['audit_root']=str(directory/'audit')
        value['host_control_ownership']={'directory':str(directory/'uncreated-host-reservations')}
        path=directory/'service.json';path.write_text(json.dumps(value,ensure_ascii=False)+'\n')
        return path
    def run(name,command,okay=True):
        network=root/(name+'.network')
        preloads=([str(args.asan_runtime.resolve(strict=True))] if args.asan_runtime else [])+[str(args.guard)]
        env=dict(os.environ,LD_PRELOAD=':'.join(preloads),GETDAQ_OFFLINE_NETWORK_LOG=str(network))
        if args.asan_runtime:
            env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        result=subprocess.run([str(word) for word in command],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=40)
        (root/(name+'.stdout')).write_bytes(result.stdout);(root/(name+'.stderr')).write_bytes(result.stderr)
        assert (result.returncode==0)==okay,(name,result.stderr.decode(errors='replace'))
        assert not network.exists() or network.stat().st_size==0,name
        assert credential.read_bytes().strip() not in result.stdout+result.stderr,name
        results.append(dict(case=name,passed=True,exit_code=result.returncode))
        return result.stdout
    # Each case owns a distinct immutable input and new audit path. No data
    # producer is run, so failure paths cannot leave large files behind.
    for scenario in ['exclusive','hardlink','tamper','marker-tamper','existing-root','activation','binding','receipt-gate','jobs-invalid-receipt','consumed','review-lease','review-started','review-target','review-tamper']:
        directory=root/scenario;path=config(directory)
        run(scenario,[args.fixture,path,directory/'successor.json',scenario])
    directory=root/'cli';path=config(directory);plan=directory/'successor.json'
    def cli(*flags):return [args.service,'--config',path,*flags]
    planned=json.loads(run('plan-config',cli('--plan-config')))
    created=run('create',cli('--create-successor-plan',plan));value=json.loads(created)
    assert created==plan.read_bytes() and plan.stat().st_mode&0o7777==0o400
    assert value['snapshot_sha256']==planned['snapshot_sha256']
    assert value['data_controller_scope_sha256']==planned['data_controller_scope_sha256']
    assert len(value['receiver_controller_epoch'])==64 and value['plan_file']==str(plan)
    assert run('check',cli('--check-successor-plan',plan))==created
    run('duplicate-create',cli('--create-successor-plan',plan),False)
    copy=directory/'copied-service.json';copy.write_bytes(path.read_bytes())
    run('changed-config-path',[args.service,'--config',copy,'--check-successor-plan',plan],False)
    link=directory/'symlink.json';link.symlink_to(plan)
    run('symlink-plan',cli('--check-successor-plan',link),False)
    run('conflicting-modes',cli('--plan-config','--create-successor-plan',directory/'other.json'),False)
    run('missing-receipt',cli('--successor-plan',plan),False)
    run('missing-plan',cli('--handoff-receipt',directory/'absent.json'),False)
    assert not (directory/'audit').exists() and not (directory/'uncreated-host-reservations').exists()
    assert not list(root.rglob('*.graw'))
    record=dict(schema=1,passed=True,cases=results,raw_remaining=0,network_attempts=0,
        actual_network_handoff_startup_tested=False,leak_detection=False,
        sanitized=bool(args.asan_runtime),program_sha256={'service':sha(args.service),'fixture':sha(args.fixture)},
        runner_sha256=sha(Path(__file__).resolve()),guard_sha256=sha(args.guard),
        configuration_sha256=sha(args.configuration))
    (root/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS',len(results),'offline successor identity cases;',root)


if __name__=='__main__':main()
