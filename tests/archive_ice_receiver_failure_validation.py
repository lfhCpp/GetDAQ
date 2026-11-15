#!/usr/bin/env python3
"""Verify retained ICE source workers/UNKNOWN/no-End after receiver failure."""
import argparse,json
from pathlib import Path
from run_receiver_observation_tests import sha
from run_receiver_failure_ecc_tests import ICE_SCENARIOS

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sanitizer_build",type=Path);parser.add_argument("checkpoint",type=Path)
    parser.add_argument("--ordinary",nargs="+",type=Path,required=True)
    parser.add_argument("--sanitized",nargs="+",type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    inputs={};programs={};evidence={};cases={"ordinary":[],"sanitized":[]}
    def record(p):
        p=p.resolve();assert p.is_file(),p;evidence[str(p)]=sha(p)
    for mode,folders in [("ordinary",args.ordinary),("sanitized",args.sanitized)]:
        for folder in folders:
            folder=folder.resolve();scope=json.loads((folder/"scope.json").read_text());cleanup=json.loads((folder/"payload-cleanup.json").read_text())
            assert scope["passed"] and scope["raw_deleted"] and cleanup["remaining_raw"]==0 and not list(folder.rglob("*.graw"))
            assert scope["maximum_aggregate_test_bytes"]<=scope["aggregate_limit_bytes"]==20*1024**3
            results=json.loads((folder/"results.json").read_text())
            assert len(results)==scope["cases"] and all(r["exit_code"]==0 for r in results)
            for name,value in json.loads((folder/"program-sha256.json").read_text()).items():
                path=Path(value["path"]);assert sha(path)==value["sha256"],path;programs[str(path)]=value["sha256"]
            for name in ["scope.json","payload-cleanup.json","results.json","program-sha256.json"]:record(folder/name)
            for result in results:
                scenario=result["scenario"];assert scenario in ICE_SCENARIOS;cases[mode].append(scenario);case=folder/scenario
                for name in ["scope.json","identity.json","final-status.json","before-receiver-failure.json","receiver-failure-policy.json","operation-receiver-stop.json",
                             "before-receiver-failure-sources.json","after-receiver-failure-sources.json","ice-receiver-failure-proof.json"]:record(case/name)
                record(folder/(scenario+".log"))
                proof=json.loads((case/"ice-receiver-failure-proof.json").read_text())
                assert proof["passed"] and proof["retained_source_instances_sessions"] and proof["source_workers_stopped"] and proof["device_stop_acknowledged"]
                assert not any(proof[k] for k in ["drain_requested","end_requested","receiver_cleanup_requested"])
                before=json.loads((case/"before-receiver-failure-sources.json").read_text());after=json.loads((case/"after-receiver-failure-sources.json").read_text())
                assert len(before["sources"])==len(after["sources"])==2
                for old,new in zip(before["sources"],after["sources"]):
                    assert new["phase"]=="available" and new["state"]=="unknown" and new["local_state"]==9 and new["failure_stop_issued"] and new["failure_stop_acknowledged"]
                    assert not new["source_end_acknowledged"] and not new["drain_acknowledged"] and not new["finish_issued"]
                    assert all(new[k]==old[k] for k in ["id","source_instance_id","receiver_session","issued_bytes","locally_sent_bytes"])
                assert not list(case.rglob(".getdaq-ice-receiver-seal.json"))
                op=json.loads((case/"operation-receiver-stop.json").read_text())
                assert op["request"]["command"]=="stop-production-after-failure" and op["phase"]==("failed" if scenario=="ice-receiver-stop-loss" else "complete")
    assert sorted(cases["ordinary"])==sorted(ICE_SCENARIOS)
    assert cases["sanitized"] and len(cases["sanitized"])==len(set(cases["sanitized"]))
    build=args.sanitizer_build.resolve();receipt=json.loads((build/"build-receipt.json").read_text())
    assert receipt["sanitized"] and not receipt["old_vendor_libraries_instrumented"] and not receipt["system_qt_instrumented"]
    for name,digest in json.loads((build/"input-sha256.json").read_text()).items():
        p=Path(name);assert sha(p)==digest,p;inputs[str(p)]=digest
    for name,digest in receipt["reused_objects"].items():assert sha(Path(name))==digest,name
    for name,digest in receipt["vendor_library_sha256"].items():
        p=Path(name);p=p if p.is_absolute() else root/p;assert sha(p)==digest,p
    for name in ["input-sha256.json","build-receipt.json","commands.json"]:record(build/name)
    for name in ["tests/run_receiver_failure_ecc_tests.py","docs/VALIDATION.md","docs/RECEIVER_FAILURE_STOP_POLICY.md","README.md","docs/REFACTOR_STATUS.md"]:
        p=root/name;inputs[str(p)]=sha(p)
    inputs[str(Path(__file__).resolve())]=sha(Path(__file__))
    args.checkpoint.parent.mkdir(parents=True,exist_ok=True)
    args.checkpoint.write_text(json.dumps({"schema":1,"kind":"getdaq.ice-receiver-failure-checkpoint","local_index_only":True,"cases":cases,
        "inputs":inputs,"programs":programs,"evidence":evidence,"strict_sanitizer_compiles":receipt["compile_count"],"real_hardware":False,
        "source_implementation_in_fixture_process":True,"system_qt_instrumented":False,"old_vendor_libraries_instrumented":False,
        "throughput_verified":False,"target_platforms_verified":False,"raw_payload_retained":False},indent=2)+"\n")
    print("PASS current ICE receiver failure checkpoint;",len(inputs),"inputs,",len(programs),"programs,",len(evidence),"evidence")

if __name__=="__main__":main()
