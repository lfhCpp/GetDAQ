#!/usr/bin/env python3
"""Verify current Qt receiver-failure display build/run receipts and local evidence."""
import argparse,json,re
from pathlib import Path
from run_receiver_observation_tests import sha

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint",type=Path)
    parser.add_argument("--qt-builds",type=Path,nargs=6,required=True)
    parser.add_argument("--client-logs",type=Path,nargs=2,required=True)
    parser.add_argument("--model-logs",type=Path,nargs=2,required=True)
    parser.add_argument("--integration",type=Path,required=True)
    parser.add_argument("--widgets",type=Path,required=True)
    parser.add_argument("--protocol-program",type=Path,required=True)
    parser.add_argument("--protocol-log",type=Path,required=True)
    parser.add_argument("--root-build",type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    inputs={};programs={};evidence={}
    def record(p):
        p=p.resolve();assert p.is_file(),p;evidence[str(p)]=sha(p)
    for build in args.qt_builds:
        build=build.resolve();r=json.loads((build/"build-receipt.json").read_text())
        values=json.loads((build/"source-sha256.json").read_text())
        for name,digest in values.items():
            p=root/name;assert sha(p)==digest,p;inputs[str(p)]=digest
        name=r["target"];p=build/name
        assert sha(p)==r["program_sha256"],p;programs[str(p)]=r["program_sha256"]
        assert sha(build/"Makefile")==r["makefile_sha256"] and not r["system_qt_instrumented"]
        for name in ("source-sha256.json","build-receipt.json","results-verified-build.txt"):record(build/name)
        generated=json.loads((build/"generated-sha256.json").read_text()) if (build/"generated-sha256.json").exists() else {}
        for name,digest in generated.items():
            p=Path(name);p=p if p.is_absolute() else build/p;assert sha(p)==digest,p
    for logs,total in ((args.client_logs,23),(args.model_logs,22)):
        for p in logs:
            text=p.read_text();assert re.search(r"Totals:\s+"+str(total)+r" passed, 0 failed",text),p;record(p)
    integration=args.integration.resolve();result=json.loads((integration/"result.json").read_text())
    assert result["passed"] and result["cases"]==2 and result["raw_deleted"] and not list(integration.rglob("*.graw"))
    assert result["maximum_aggregate_test_bytes"]<=result["aggregate_limit_bytes"]==20*1024**3
    for name,digest in result["program_sha256"].items():assert sha(Path(name))==digest,name;programs[name]=digest
    for name in ("result.json","results.json","payload-cleanup.json"):record(integration/name)
    for scenario in ("qt-receiver-failure-0","qt-receiver-failure-1"):
        case=integration/scenario
        for name in ("scope.json","final-status.json","identity.json","receiver-failure-policy.json","operation-receiver-stop.json",
                     "gui-before-receiver-failure.json","gui-probe.log"):record(case/name)
        record(integration/(scenario+".log"))
        journal=case/"gui-runs/hardware/NativeIntegration/run_0000/journal.json";record(journal)
        j=json.loads(journal.read_text());assert j["phase"]!="Complete" and j["duration_incomplete"] and not (journal.parent/"run.csv").exists()
        c=json.loads(j["native_receiver_failure_cache"]);assert c["latched"] and c["phase"]=="complete"
    widgets=args.widgets.resolve();w=json.loads((widgets/"result.json").read_text())
    assert w["passed"] and w["raw_deleted"] and not json.loads((widgets/"payload-cleanup.json").read_text())["remaining_raw"]
    assert re.search(r"Totals:\s+76 passed, 0 failed",(widgets/"widgets.log").read_text())
    assert sha(Path(w["program"]))==w["program_sha256"]
    for name in ("result.json","payload-cleanup.json","widgets.log"):record(widgets/name)
    assert args.protocol_log.read_text().startswith("PASS portable C++11 ECC codec:")
    p=args.protocol_program.resolve();programs[str(p)]=sha(p);record(args.protocol_log)
    assert "error:" not in (args.root_build/"build.log").read_text();record(args.root_build/"build.log")
    assert "error:" not in (args.root_build/"build-final.log").read_text();record(args.root_build/"build-final.log")
    for name in ("GetDAQ","getdaq-sentry","getdaq-simulator","getdaq-config","getdaq-config-check"):
        p=root/"bin"/name;assert p.is_file(),p;programs[str(p)]=sha(p)
    for name in ("config/envoy.native.example.yml","docs/RECEIVER_FAILURE_STOP_POLICY.md","docs/VALIDATION.md","README.md",
                 "docs/REFACTOR_STATUS.md","tests/receiver_failure_fixture.h","tests/ecc_protocol_test.cpp",
                 "tests/ecc_network_service_test.cpp","tests/run_receiver_failure_gui_tests.py","tests/run_receiver_failure_widget_tests.py"):
        p=root/name;inputs[str(p)]=sha(p)
    inputs[str(Path(__file__).resolve())]=sha(Path(__file__))
    args.checkpoint.parent.mkdir(parents=True,exist_ok=True)
    args.checkpoint.write_text(json.dumps({"schema":1,"kind":"getdaq.receiver-failure-gui-checkpoint","local_index_only":True,
        "inputs":inputs,"programs":programs,"evidence":evidence,"client_passed_each":23,"model_passed_each":22,"widgets_passed":76,
        "actual_qt_fault_encodings":[0,1],"raw_payload_retained":False,"system_qt_instrumented":False,"native_ecc_instrumented_this_phase":False,
        "real_hardware":False,"target_platforms_verified":False,"throughput_verified":False},indent=2)+"\n")
    print("PASS current receiver-failure GUI checkpoint;",len(inputs),"inputs,",len(programs),"programs,",len(evidence),"evidence")

if __name__=="__main__":main()
