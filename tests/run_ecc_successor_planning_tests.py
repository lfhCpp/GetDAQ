#!/usr/bin/env python3
"""Check offline planned digests against prior actual startup snapshots, denying network calls."""
import argparse
import fcntl
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["service","guard","original_configuration","evidence"]:parser.add_argument(name,type=Path)
    parser.add_argument("--asan-runtime",type=Path,help="Preload ASan before the deny-network interposer; leak checks remain disabled")
    args=parser.parse_args()
    for name in ["service","guard","original_configuration"]:setattr(args,name,getattr(args,name).resolve(strict=True))
    args.evidence.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix="ecc-successor-plan-",dir=args.evidence)).resolve()
    original=json.loads(args.original_configuration.read_text())
    snapshot_file=Path(original["audit_root"])/"snapshot.json"
    original_snapshot=json.loads(snapshot_file.read_text())
    snapshot_sha=sha(snapshot_file)
    # Private replacement credentials keep the retired test artifacts untouched.
    credential=root/"credential";credential.write_text(os.urandom(32).hex()+"\n");credential.chmod(0o600)
    config=json.loads(args.original_configuration.read_text())
    config["credential_file"]=str(credential)
    for endpoint in config["remote_receivers"]["endpoints"]:endpoint["credential_file"]=str(credential)
    results=[]
    def environment(network):
        preload=([str(args.asan_runtime.resolve(strict=True))] if args.asan_runtime else [])+[str(args.guard)]
        env=dict(os.environ,LD_PRELOAD=':'.join(preload),GETDAQ_OFFLINE_NETWORK_LOG=str(network))
        if args.asan_runtime:
            env["ASAN_OPTIONS"]="detect_leaks=0:halt_on_error=1"
            env["UBSAN_OPTIONS"]="halt_on_error=1:print_stacktrace=1"
        return env
    def invoke(name, value, okay=True, flags=None):
        path=root/(name+".json");path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+"\n")
        network=root/(name+"-network.log")
        env=environment(network)
        command=[str(args.service),"--config",str(path)]+(flags or ["--plan-config"])
        before={p:sha(p) for p in root.rglob('*') if p.is_file()}
        run=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,timeout=30)
        (root/(name+".stdout")).write_bytes(run.stdout);(root/(name+".stderr")).write_bytes(run.stderr)
        assert (run.returncode==0)==okay,(name,run.stderr.decode(errors='replace'))
        assert credential.read_bytes().strip() not in run.stdout+run.stderr,"Planning disclosed credentials"
        assert not network.exists() or not network.read_bytes(),(name,network.read_text())
        assert all(sha(p)==digest for p,digest in before.items()),name
        results.append({"case":name,"passed":True,"exit_code":run.returncode})
        return json.loads(run.stdout) if okay else None
    def fixed_binding(path, value):
        inputs={str(path):sha(path)}
        def references(item):
            if isinstance(item,dict):
                if set(item)=={"path","sha256"}:
                    file=Path(item["path"]);actual=sha(file);assert actual==item["sha256"]
                    inputs[str(file)]=actual
                else:
                    for child in item.values():references(child)
            elif isinstance(item,list):
                for child in item:references(child)
        references(value)
        manifest="getdaq.fixed-ecc-inputs:1\n"+''.join(json.dumps(name,ensure_ascii=False)+' '+digest+'\n' for name,digest in sorted(inputs.items()))
        return hashlib.sha256(manifest.encode()).hexdigest()
    def expected_plan(plan, base, file, host=None):
        assert plan["input_binding_sha256"]==fixed_binding(file,base),"Planned fixed input binding differs from independent file hashes"
        expected=dict(original_snapshot)
        expected["deployment_source_sha256"]=plan["input_binding_sha256"]
        if host:expected["host_control_ownership_root"]=host
        canonical=(json.dumps(expected,ensure_ascii=False,separators=(',',':'))+"\n").encode()
        assert hashlib.sha256(canonical).hexdigest()==plan["snapshot_sha256"],"Offline plan differs from prior startup snapshot after fixed input rebinding"
        scope=hashlib.sha256(("getdaq.data-controller:1\n"+plan["snapshot_sha256"]+'\n'+base["audit_root"]).encode()).hexdigest()
        assert scope==plan["data_controller_scope_sha256"]
        for key in ["configuration_sha256","artifact_sha256"]:assert plan[key]==original_snapshot[key]
        assert not any(plan[key] for key in ["audit_roots_created","host_reservations_acquired","hardware_commands_sent","receiver_started"])
    plan=invoke("existing-root",config);expected_plan(plan,config,root/"existing-root.json")
    # A repeated invocation of identical input must not generate a new identity.
    path=root/"existing-root.json";network=root/"repeat-network.log"
    again=subprocess.run([str(args.service),"--config",str(path),"--plan-config"],capture_output=True,
        env=environment(network),timeout=30)
    assert again.returncode==0 and again.stdout==(root/"existing-root.stdout").read_bytes()
    assert not network.exists() or not network.read_bytes()
    results.append({"case":"deterministic-repeat","passed":True,"exit_code":0})
    fresh=dict(config,audit_root=str(root/"never-created-controller"))
    fresh["host_control_ownership"]={"directory":str(root/"never-created-host-reservations")}
    plan=invoke("fresh-unreserved-root",fresh);expected_plan(plan,fresh,root/"fresh-unreserved-root.json",fresh["host_control_ownership"]["directory"])
    assert not Path(fresh["audit_root"]).exists() and not Path(fresh["host_control_ownership"]["directory"]).exists()
    reserved=root/"held-reservations";reserved.mkdir(mode=0o700)
    lock=reserved/(hashlib.sha256(b"getdaq.host-board:1\n127.0.0.1").hexdigest()+".lock")
    lock.write_text("getdaq.offline-test-held-reservation\n");lock.chmod(0o600)
    held=dict(config,audit_root=str(root/"never-created-reserved-controller"))
    held["host_control_ownership"]={"directory":str(reserved)}
    with lock.open("r+b") as lease:
        fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
        plan=invoke("already-reserved-board",held);expected_plan(plan,held,root/"already-reserved-board.json",str(reserved))
    assert not Path(held["audit_root"]).exists()
    invalid=dict(config,audit_root="relative/root");invoke("relative-root-rejected",invalid,False)
    invalid=json.loads(json.dumps(config));invalid["senders"][1]["id"]=invalid["senders"][0]["id"]
    invoke("duplicate-sender-rejected",invalid,False)
    invoke("conflicting-mode-rejected",config,False,["--plan-config","--check-config"])
    assert sha(snapshot_file)==snapshot_sha
    assert not list(root.rglob("*.graw"))
    record={"passed":True,"cases":results,"raw_bytes":0,"network_attempts":0,"actual_hardware":False,
        "comparison_basis":"prior actual startup snapshot, with only fixed-input binding and explicit host-reservation declaration replaced",
        "original_configuration":str(args.original_configuration),"original_snapshot_file":str(snapshot_file),"original_snapshot_sha256":snapshot_sha,"program_sha256":sha(args.service),"guard_sha256":sha(args.guard),"runner_sha256":sha(Path(__file__)),
        "sanitized":bool(args.asan_runtime),"leak_detection":False if args.asan_runtime else None}
    (root/"result.json").write_text(json.dumps(record,indent=2)+"\n")
    print("PASS",len(results),"offline successor planning cases; no network attempts, roots, host reservations or raw data;",root)


if __name__=="__main__":main()
