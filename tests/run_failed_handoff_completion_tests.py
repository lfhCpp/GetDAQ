#!/usr/bin/env python3
"""Live accepted-but-failed control handoff recovery; localhost synthetic data only."""
import argparse,copy,hashlib,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_supervisor_control_tests import Rig,frame,require,wait_for
from run_session_retirement_service_tests import ports
from run_receiver_observation_tests import interrupted

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

class HandoffRig(Rig):
    def start_service(self):
        log=(self.root/("service-"+str(len(self.logs))+".log")).open("w");self.logs.append(log)
        env=dict(os.environ);env["LD_PRELOAD"]=str(self.args.injector)
        env["GETDAQ_TEST_HANDOFF_PROOF"]=str(self.original_dir/"handoff-proof.json")
        self.process=subprocess.Popen([str(self.args.service),"--config",str(self.config_file)],stdout=log,stderr=subprocess.STDOUT,env=env)
        def identity():
            require(self.process.poll() is None,"Control service exited during startup")
            try:
                code,value=self.request("GET","/v1/identity");return value if code==200 else None
            except (ConnectionError,OSError):return None
        self.identity=wait_for(identity)
        require(self.identity["agent_id"]=="data-node-fixture" and self.identity["controller_id"]=="ecc-fixture","Service scope differs")

def run(args,root):
    rig=None
    try:
        test_ports=ports()
        case=root/"live-handoff"
        rig=HandoffRig.__new__(HandoffRig);rig.original_dir=case/"deployment/control/operations/handoff-original"
        rig.__init__(args,case,test_ports)
        rig.start();rig.connect_senders();finished=rig.finish([frame(1),frame(2,True)])
        raw={path:sha(path) for path in case.rglob("*.graw")}
        original=rig.make_request("handoff-control","handoff-original")
        original.update(successor_controller_snapshot_sha256="e"*64,successor_controller_epoch="f"*64)
        rig.submit(original);failed=rig.completed(original["operation_id"],"failed")
        require(not (rig.original_dir/"handoff-proof.json").exists(),"Fault injector did not stop original proof publication")
        require(failed["request_key"] and failed["handoff"]["proof_sha256"]=="","Failed handoff advertised a committed proof")
        require(failed["result"]["state"]=="stopped" and not failed["result"]["needs_reconciliation"] and
            all(r["reaped"] and r["exit_code"]==0 and r["seal_verified"] for r in failed["result"]["receivers"]),
            "Injected metadata failure changed the independently sealed data cycle")
        require(rig.status()["control_needs_reconciliation"],"Failed handoff did not retain a control hold")

        original_file=rig.original_dir/"request.json"
        completion_id="complete-handoff-original"
        changed_file=case/"changed-original-request.json"
        changed_original=dict(original,successor_controller_epoch="9"*64)
        changed_file.write_text(json.dumps(changed_original)+"\n")
        rig.query("build-handoff-completion",option="--request-file",argument=changed_file,
            override={"--failed-operation-id":original["operation_id"],"--operation-id":"must-not-exist"},okay=False)
        require(not (rig.root/"deployment/control/operations"/"must-not-exist").exists(),"Rejected completion request builder wrote intent")
        noncanonical_file=case/"noncanonical-original-request.json"
        noncanonical_file.write_text(json.dumps(original,indent=2)+"\n")
        rig.query("build-handoff-completion",option="--request-file",argument=noncanonical_file,
            override={"--failed-operation-id":original["operation_id"],"--operation-id":"noncanonical-must-not-exist"},okay=False)
        require(not (rig.root/"deployment/control/operations"/"noncanonical-must-not-exist").exists(),"Noncanonical original request was accepted")

        if args.window_probe:
            completion_path=case/"completion-request.json"
            probe=subprocess.run([str(args.window_probe),str(args.client),"127.0.0.1",str(test_ports[0]),
                str(rig.token_file),"data-node-fixture","ecc-fixture",rig.identity["plan_sha256"],str(original_file),
                original["operation_id"],completion_id,str(completion_path)],capture_output=True,text=True,timeout=120)
            (case/"failed-handoff-window-probe.log").write_text(probe.stdout+probe.stderr)
            require(probe.returncode==0,"Qt failed-handoff completion window failed: "+probe.stdout+probe.stderr)
            completion=json.loads(completion_path.read_text())
            require(json.loads(Path(str(completion_path)+".window-result.json").read_text()).get("passed") is True,
                "Qt failed-handoff completion window receipt is missing")
        else:
            completion=rig.query("build-handoff-completion",option="--request-file",argument=original_file,
                override={"--failed-operation-id":original["operation_id"],"--operation-id":completion_id})
        require(completion["command"]=="complete-control-handoff" and completion["failed_handoff_operation_id"]==original["operation_id"] and
            completion["successor_controller_snapshot_sha256"]==original["successor_controller_snapshot_sha256"] and
            completion["successor_controller_epoch"]==original["successor_controller_epoch"],
            "Completion request builder changed the exact original successor")
        if not args.window_probe:
            require(not (rig.root/"deployment/control/operations"/completion_id).exists(),"Completion request builder sent a POST")
        request_file=case/"completion-request.json";request_file.write_text(json.dumps(completion)+"\n")
        if not args.window_probe:
            accepted=rig.query("submit","--request-file",request_file)
            require(accepted["phase"] in ("accepted","running","complete"),"Completion CLI did not submit exact request")
        result=rig.completed(completion["operation_id"])
        require(result["phase"]=="complete" and result["handoff"]["proof_sha256"]==sha(rig.original_dir/"handoff-proof.json"),
            "Explicit completion did not publish a hash-bound proof")
        require(rig.operation(original["operation_id"])["phase"]=="failed","Original failed handoff was rewritten")
        proof=json.loads((rig.original_dir/"handoff-proof.json").read_text())
        require(proof["operation_id"]==original["operation_id"] and proof["request_key"]==failed["request_key"] and
            proof["successor_controller_snapshot_sha256"]==completion["successor_controller_snapshot_sha256"] and
            proof["successor_controller_epoch"]==completion["successor_controller_epoch"] and
            proof["lifecycle"]["state"]=="stopped","Proof is not bound to the original failed request/cycle/successor")
        require((rig.original_dir/"handoff-proof.json").stat().st_mode&0o777==0o600,"Live handoff proof mode differs")
        require(rig.request("POST","/v1/operations",rig.make_request("start","old-owner-start"))[0]==409,
            "Original owner retained positive control after handoff completion")
        successor=rig.make_request("validate","successor-observe")
        successor["controller_snapshot_sha256"]=completion["successor_controller_snapshot_sha256"]
        successor["controller_epoch"]=completion["successor_controller_epoch"]
        require(rig.request("POST","/v1/operations",successor)[0]==202,"New successor owner could not use the live control service")
        rig.completed(successor["operation_id"])
        rig.stop_service();rig.start_service()
        require(rig.operation(original["operation_id"])["phase"]=="failed" and
            rig.operation(completion["operation_id"])["phase"]=="complete","Cold replay changed original/completion history")
        distinct=dict(completion,operation_id="cold-second-completion",instance_id=rig.identity["instance_id"])
        require(rig.request("POST","/v1/operations",distinct)[0] in (409,412),"Cold instance accepted a new live repair")
        require(rig.status()["control_needs_reconciliation"],"Cold restart cleared the independent control fence")
        require(all(sha(path)==digest for path,digest in raw.items()),"Handoff completion changed sealed raw files")
        return {"passed":True,"original_failed_preserved":True,"exact_live_completion":True,
            "successor_owner_observed":True,"cold_restart_fenced":True,"raw_files":len(raw),
            "raw_bytes":sum(path.stat().st_size for path in raw),"proof_sha256":sha(rig.original_dir/"handoff-proof.json")}
    finally:
        if rig is not None:rig.close()

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("--service",type=Path,required=True);ap.add_argument("--receiver",type=Path,required=True)
    ap.add_argument("--client",type=Path,required=True);ap.add_argument("--injector",type=Path,required=True);ap.add_argument("--evidence",type=Path,required=True)
    ap.add_argument("--window-probe",type=Path,help="Run the production Qt window against the live test service")
    args=ap.parse_args()
    for name in ("service","receiver","client","injector"):setattr(args,name,getattr(args,name).resolve(strict=True))
    if args.window_probe:args.window_probe=args.window_probe.resolve(strict=True)
    args.query=args.client
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix="failed-handoff-",dir=args.evidence)).resolve();deleted=[]
    try:
        result=run(args,root)
        # Raw files are verification fixtures only and are removed before the final receipt.
        for path in root.rglob("*.graw"):
            if path.is_file() and not path.is_symlink():deleted.append({"path":str(path.relative_to(root)),"bytes":path.stat().st_size});path.unlink()
        require(not list(root.rglob("*.graw")),"Synthetic raw cleanup incomplete")
        total=sum(item["bytes"] for item in deleted)
        require(total<=64*1024**2,"Failed-handoff recovery fixture exceeded its 64 MiB aggregate guard")
        payload={"passed":True,"scenario":result,"program_sha256":{k:sha(getattr(args,k)) for k in ("service","receiver","client","injector")},
            "runner_sha256":sha(Path(__file__)),"deleted_raw":deleted,"raw_remaining":len(list(root.rglob("*.graw"))),
            "maximum_aggregate_test_bytes":64*1024**2,"real_hardware":False}
        if args.window_probe:payload["window_probe_sha256"]=sha(args.window_probe)
        (root/"result.json").write_text(json.dumps(payload,indent=2)+"\n")
        (root/"cleanup.json").write_text(json.dumps({"deleted":deleted,"raw_remaining":0},indent=2)+"\n")
        print("PASS live failed handoff completion, owner transfer, cold fence; removed synthetic raw:",root)
    finally:
        for path in root.rglob("*.graw"):
            if path.is_file() and not path.is_symlink():path.unlink()
if __name__=="__main__":main()
