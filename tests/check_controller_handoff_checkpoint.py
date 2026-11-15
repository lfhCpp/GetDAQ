#!/usr/bin/env python3
"""Create or verify the bounded phase 70.50 evidence index; never execute network tests."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/"build-controller-handoff"


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):return json.loads(path.read_text())


def check_map(values, base=ROOT):
    for name, expected in values.items():
        path=Path(name);path=path if path.is_absolute() else base/path
        if not path.is_file() or sha(path)!=expected:raise RuntimeError("Missing or changed recorded file: "+str(path))
    return len(values)


def collect():
    results={};evidence={};cases=0
    for family in ["ordinary-final-evidence","sanitized-evidence","remote-final-evidence","remote-sanitized-evidence"]:
        paths=list((PHASE/family).glob("*/result.json"))
        assert len(paths)==1,(family,paths)
        path=paths[0];data=read(path);assert data["passed"] and data["raw_remaining"]==0
        assert all(row["passed"] for row in data["cases"])
        cases+=len(data["cases"])
        subprocess.run(["python3",str(ROOT/"tests/check_controller_handoff_evidence.py"),str(path.parent)],check=True)
        results[family]={"path":str(path.relative_to(ROOT)),"sha256":sha(path),"cases":len(data["cases"])}
        for file in path.parent.rglob("*"):
            if file.is_file():evidence[str(file.relative_to(ROOT))]=sha(file)
    assert cases==26
    regression=PHASE/"regression-evidence/remote-receiver-4105k2fa"
    assert all(row["exit_code"]==0 for row in read(regression/"results.json"))
    assert read(regression/"payload-cleanup.json")["raw_remaining"]==0
    for path in regression.rglob("*"):
        if path.is_file():evidence[str(path.relative_to(ROOT))]=sha(path)
    production={};programs={}
    for name in ["build-delivery/full/build-receipt.json","build-windows-qt5142/windows-build-receipt.json"]:
        path=ROOT/name;data=read(path);assert data["status"]=="complete";check_map(data["inputs"])
        production[name]={"sha256":sha(path),"inputs":data["inputs"]}
        for rec in data["programs"].values():
            file=Path(rec["path"]);assert sha(file)==rec["sha256"];programs[str(file.relative_to(ROOT))]=sha(file)
    builds={}
    for kind in ["ordinary","sanitized"]:
        base=PHASE/kind;receipt=read(base/"build-receipt.json")
        assert receipt["compile_count"]==60 and receipt["tracked_input_count"]==121
        sources=read(base/"source-sha256.json");check_map(sources)
        check_map(read(base/"generated-sha256.json"),base)
        for name,digest in receipt["program_sha256"].items():
            path=base/name;assert sha(path)==digest;programs[str(path.relative_to(ROOT))]=digest
        builds[kind]={"receipt_sha256":sha(base/"build-receipt.json"),"sources":sources,
            "generated":read(base/"generated-sha256.json"),"compile_count":60}
        for name in ["build-receipt.json","source-sha256.json","generated-sha256.json","verified-compile-commands.json"]:
            path=base/name;evidence[str(path.relative_to(ROOT))]=sha(path)
    overlay=PHASE/"remote-sanitized";receipt=read(overlay/"build-receipt.json")
    assert receipt["compile_count"]==16 and len(receipt["reused_objects"])==62
    check_map(receipt["dependencies"]);check_map(receipt["tracked_inputs"]);check_map(receipt["reused_objects"])
    check_map(read(overlay/"fixture-dependencies.json"))
    assert len(read(overlay/"fixture-dependencies.json"))==168
    for name in ["build-receipt.json","fixture-dependencies.json"]:
        path=overlay/name;evidence[str(path.relative_to(ROOT))]=sha(path)
    for path in [overlay/"native-ecc-remote-receiver-tests",ROOT/"build-ice-compat/native-ecc-remote-receiver-tests",ROOT/"build-source-retirement/runtime-baseline/getdaq-receiver"]:
        programs[str(path.relative_to(ROOT))]=sha(path)
    for family,result in results.items():
        values=read(ROOT/result["path"])["program_sha256"]
        if family.startswith("remote"):
            expected={"fixture":sha(overlay/"native-ecc-remote-receiver-tests" if family=="remote-sanitized-evidence" else ROOT/"build-ice-compat/native-ecc-remote-receiver-tests"),
                "receiver":sha(ROOT/"build-source-retirement/runtime-baseline/getdaq-receiver"),
                "service":sha(PHASE/("sanitized" if family=="remote-sanitized-evidence" else "ordinary")/"getdaq-supervisor")}
        else:
            base=PHASE/("sanitized" if family=="sanitized-evidence" else "ordinary")
            expected={"service":sha(base/"getdaq-supervisor"),"query":sha(base/"getdaq-supervisor-query"),"receiver":sha(ROOT/"build-source-retirement/runtime-baseline/getdaq-receiver")}
        assert values==expected,family
    packages={}
    for name in ["getdaq-host-20261015-controller-handoff-candidate","getdaq-windows-qt5142-20261015-controller-handoff-candidate"]:
        path=ROOT/"build-packages"/name;manifest=read(path/"manifest.json");check_map(manifest["files"],path)
        packages[str(path.relative_to(ROOT))]={"manifest_sha256":sha(path/"manifest.json"),"files":manifest["files"]}
    inputs=["tests/run_controller_handoff_tests.py","tests/run_remote_controller_handoff_tests.py","tests/check_controller_handoff_evidence.py",
        "tests/build_controller_handoff_sanitizers.py","tests/check_controller_handoff_sanitizer_build.py","tests/check_controller_handoff_checkpoint.py",
        "tests/supervisor_control_test.cpp","tests/native_ecc_remote_receiver_test.cpp","tests/native_receiver_fixture.h","tests/native_remote_service_fixture.h",
        "tests/run_supervisor_control_tests.py","tests/build_data_supervisor_validation.py"]
    docs=["README.md","docs/DATA_CONTROL_HANDOFF.md","docs/REFACTOR_STATUS.md","docs/VALIDATION.md","docs/ECC_CONFIGURATION_SELECTION.md","docs/REMOTE_RECEIVER_CONTROL.md"]
    assert not list(PHASE.rglob("*.graw"))
    return {"schema":1,"phase":"70.50 explicit original receiver-control handoff and successor adoption","goal_complete":False,
        "current_scenarios":26,"passed_results":results,"production_inputs":production,"programs":programs,"builds":builds,
        "sanitizer_overlay_receipt_sha256":sha(overlay/"build-receipt.json"),"evidence":evidence,"packages":packages,
        "validation_inputs":{p:sha(ROOT/p) for p in inputs},"documentation":{p:sha(ROOT/p) for p in docs},"raw_remaining":0,
        "new_test_group_budget_bytes":2*1024**2,"aggregate_user_limit_bytes":20*1024**3,
        "package_network_smoke":{"executed":False,"reason":"Automatic approval review unavailable because usage limit; command not executed"},
        "windows_smoke_reused_from_identical_programs_and_dlls":True,"independent_target_platforms_and_hardware_accepted":False,
        "remaining":["Complete ECC configuration-generation activation and GUI","Receiver plan parameter switching","Continuous board host-reservation handoff",
            "Cold/partial handoff recovery","Ubuntu24/macOS/independent Windows and physical hardware/sustained cross-host DAQ acceptance"]}


def verify(path):
    record=read(path);assert record["schema"]==1 and not record["goal_complete"] and record["current_scenarios"]==26
    count=0
    for key in ["programs","evidence","validation_inputs","documentation"]:count+=check_map(record[key])
    for name,rec in record["production_inputs"].items():count+=check_map({name:rec["sha256"]});count+=check_map(rec["inputs"])
    for name,rec in record["packages"].items():
        base=ROOT/name;count+=check_map({"manifest.json":rec["manifest_sha256"]},base);count+=check_map(rec["files"],base)
    assert not list(PHASE.rglob("*.graw"))
    print("PASS",count,"recorded digest checks; 26 local scenarios; owned raw zero; full project remains incomplete")


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--write",action="store_true");args=parser.parse_args()
    path=PHASE/"checkpoint.json"
    if args.write:path.write_text(json.dumps(collect(),ensure_ascii=False,indent=2)+"\n")
    verify(path)
