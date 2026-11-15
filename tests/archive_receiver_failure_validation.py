#!/usr/bin/env python3
"""Record current receiver-failure policy proof; this is a local index, not deployment."""
import argparse
import json
from pathlib import Path
from run_receiver_observation_tests import sha
from run_receiver_failure_ecc_tests import SCENARIOS

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native_sanitizer_build",type=Path)
    parser.add_argument("checkpoint",type=Path)
    parser.add_argument("--ordinary",type=Path,nargs="+",required=True)
    parser.add_argument("--sanitized",type=Path,nargs="+",required=True)
    parser.add_argument("--regression",type=Path,required=True)
    parser.add_argument("--protocol-build",type=Path,required=True)
    parser.add_argument("--protocol-log",type=Path,required=True)
    parser.add_argument("--fixed-evidence",type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    inputs={};evidence={};programs={};cases={"ordinary":[],"sanitized":[]}
    def record(p):
        p=p.resolve();assert p.is_file(),p;evidence[str(p)]=sha(p)
    for mode,folders in (("ordinary",args.ordinary),("sanitized",args.sanitized)):
        for folder in folders:
            folder=folder.resolve();scope=json.loads((folder/"scope.json").read_text())
            cleanup=json.loads((folder/"payload-cleanup.json").read_text())
            assert scope["passed"] and scope["raw_deleted"] and cleanup["remaining_raw"]==0
            assert scope["maximum_aggregate_test_bytes"]<=scope["aggregate_limit_bytes"]==20*1024**3
            assert not list(folder.rglob("*.graw")),folder
            rows=json.loads((folder/"results.json").read_text())
            assert len(rows)==scope["cases"] and all(r["exit_code"]==0 for r in rows)
            cases[mode].extend(r["scenario"] for r in rows)
            receipts=json.loads((folder/"program-sha256.json").read_text())
            for name,value in receipts.items():
                path=Path(value["path"]);assert sha(path)==value["sha256"],path
                programs[str(path)]=value["sha256"]
            for name in ("scope.json","payload-cleanup.json","results.json","program-sha256.json"):record(folder/name)
            for row in rows:
                case=folder/row["scenario"]
                for name in ("scope.json","identity.json","final-status.json","before-receiver-failure.json","receiver-failure-policy.json"):record(case/name)
                record(folder/(row["scenario"]+".log"))
                policy=json.loads((case/"receiver-failure-policy.json").read_text())
                assert policy["enabled"]==(row["scenario"]!="receiver-disabled")
                actionable=row["scenario"] not in ("receiver-disabled","receiver-normal-exit","receiver-offline")
                assert policy["latched"]==actionable
                if actionable:
                    record(case/"operation-receiver-stop.json")
                    op=json.loads((case/"operation-receiver-stop.json").read_text())
                    assert op["request"]["command"]=="stop-production-after-failure"
                    assert op["phase"]==("failed" if row["scenario"]=="receiver-stop-loss" else "complete")
    assert sorted(cases["ordinary"])==sorted(SCENARIOS),cases
    assert cases["sanitized"] and len(set(cases["sanitized"]))==len(cases["sanitized"])
    regression=args.regression.resolve();r=json.loads((regression/"result.json").read_text())
    assert r["passed"] and r["cases"]==2 and r["raw_deleted"] and not list(regression.rglob("*.graw"))
    assert r["maximum_aggregate_test_bytes"]<=r["aggregate_limit_bytes"]==20*1024**3
    for name,digest in r["program_sha256"].items():assert sha(Path(name))==digest,name
    for name in ("result.json","results.json","payload-cleanup.json","normal-1.log","alarm-running-1.log"):record(regression/name)
    for name in ("normal-1","alarm-running-1"):
        record(regression/name/"scope.json");record(regression/name/"final-status.json")
    assert args.protocol_log.read_text().startswith("PASS portable C++11 ECC codec:")
    record(args.protocol_log)
    path=args.protocol_build.resolve()/"ecc-protocol-tests";programs[str(path)]=sha(path)
    record(args.protocol_build/"CMakeCache.txt")
    flags=args.protocol_build/"CMakeFiles/ecc-protocol-tests.dir/flags.make"
    text=flags.read_text()
    assert all(flag in text for flag in ("-std=c++11","-pedantic-errors","-fsanitize=address,undefined"))
    record(flags)
    assert args.fixed_evidence.read_text().startswith("PASS fixed input/");record(args.fixed_evidence)
    build=args.native_sanitizer_build.resolve();receipt=json.loads((build/"build-receipt.json").read_text())
    assert receipt["sanitized"] and not receipt["old_vendor_libraries_instrumented"] and not receipt["system_qt_instrumented"]
    for name,digest in json.loads((build/"input-sha256.json").read_text()).items():
        path=Path(name);assert sha(path)==digest,path;inputs[str(path)]=digest
    for name,digest in receipt["reused_objects"].items():assert sha(Path(name))==digest,name
    for name,digest in receipt["vendor_library_sha256"].items():
        path=Path(name);path=path if path.is_absolute() else root/path;assert sha(path)==digest,path
    record(build/"build-receipt.json");record(build/"commands.json");record(build/"input-sha256.json")
    for name in ("README.md","docs/RECEIVER_FAILURE_STOP_POLICY.md","docs/REFACTOR_STATUS.md","docs/ECC_NETWORK_SERVICE.md",
                 "docs/VALIDATION.md","tests/run_receiver_failure_ecc_tests.py","config/ecc/service.example.json"):
        path=root/name;inputs[str(path)]=sha(path)
    inputs[str(Path(__file__).resolve())]=sha(Path(__file__))
    for name in ("tests/run_receiver_failure_regression.py","tests/ecc_protocol_test.cpp","tests/fixed_ecc_configuration_test.cpp"):
        path=root/name;inputs[str(path)]=sha(path)
    args.checkpoint.parent.mkdir(parents=True,exist_ok=True)
    args.checkpoint.write_text(json.dumps({"schema":1,"kind":"getdaq.receiver-failure-checkpoint","local_index_only":True,"cases":cases,
        "strict_native_sanitizer_compiles":receipt["compile_count"],"inputs":inputs,"programs":programs,"evidence":evidence,
        "real_hardware":False,"target_platforms_verified":False,"throughput_verified":False,"system_qt_instrumented":False,
        "old_vendor_libraries_instrumented":False,"raw_payload_retained":False},indent=2)+"\n")
    print("PASS current receiver failure checkpoint;",len(inputs),"inputs,",len(programs),"programs,",len(evidence),"evidence; raw deleted")

if __name__=="__main__":
    main()
