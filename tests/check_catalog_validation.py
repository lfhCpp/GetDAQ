#!/usr/bin/env python3
"""只读核对GUI/ECC实际数据归档：成功及独立节点失败，保留旧归档检查器。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_ecc_network_validation import require, sha

SCENARIOS={"qt-catalog-0","qt-catalog-1","qt-catalog-failure"}
BASE={"describe":1,"prepare":1,"configure":1,"start":1,"pause":1,"resume":1,"stop":1,"breakup":1,"catalog":1,"reset":2}

def rows(path): return [json.loads(line) for line in path.read_text().splitlines()]

def check_run(root,snapshots=None):
    results=json.loads((root/"results.json").read_text())
    require(len(results)==3 and {r["scenario"] for r in results}==SCENARIOS and all(r["exit_code"]==0 for r in results),"Catalog actual case set/exit differs")
    programs=json.loads((root/"program-sha256.json").read_text())
    require(set(programs)=={"fixture","receiver","data_service","ecc_service","qt_probe"},"Catalog tested executable set differs")
    for name,record in programs.items(): require(sha(snapshots/name if snapshots else Path(record["path"]))==record["sha256"],"Catalog tested program changed: "+name)
    for result in results:
        case=root/result["scenario"]; failed=result["scenario"]=="qt-catalog-failure"
        scope=json.loads((case/"scope.json").read_text())
        require(scope["passed"] and scope["real_qt_runcontrol"] and scope["real_data_catalog"] and scope["real_receivers"] and scope["real_data_services"]==2 and
                scope["full_ecc_workflow_tested"]== (not failed) and scope["physical_hosts"]==1 and not scope["real_hardware"] and not scope["throughput_verified"],"Catalog scope overclaims")
        identity=json.loads((case/"identity.json").read_text()); require(sha(case/"controller/snapshot.json")==identity["snapshot_sha256"],"Catalog parent snapshot changed")
        events=rows(case/"controller/network/operations.jsonl"); accepted={e["operation_id"]:e for e in events if e["event"]=="accepted"}
        require(len(accepted)==sum(e["event"]=="accepted" for e in events),"Catalog parent accepted duplicate IDs")
        requests={key:json.loads(e["request"]) for key,e in accepted.items()}
        counts=dict(Counter(r["command"] for r in requests.values())); expected=dict(BASE)
        if failed: expected.pop("reset")
        require(counts==expected,"Catalog global command count differs")
        for key,event in accepted.items():
            require((case/"controller/network/operations"/key/"request.json").read_text()==event["request"] and
                    hashlib.sha256(("getdaq.ecc-control-request:1\n"+event["request"]).encode()).hexdigest()==event["request_key"],"Catalog parent intent/key differs")
        catalog=[r for r in requests.values() if r["command"]=="catalog"][0]
        require(catalog["catalog_experiment"]=="NativeIntegration" and catalog["catalog_run_number"]==0,"Catalog experiment/Run missing from original intent")
        run=case/"gui-runs/hardware/NativeIntegration/run_0000"; journal=json.loads((run/"journal.json").read_text())
        require((journal["phase"]=="Complete")== (not failed) and (run/"run.csv").exists()== (not failed) and (run/"data-catalogs.json").exists()== (not failed),"Failed catalog became completed GUI Run")
        final=json.loads((case/"final-status.json").read_text())
        require(final["controller"]["state"]==(3 if failed else 1) and final["control_needs_reconciliation"]==failed and final["data"]["needs_reconciliation"]==failed,"Catalog final state/gate differs")
        groups=final["data"]["nodes"]; require(len(groups)==2,"Catalog node coverage differs")
        successes=0
        for index,node in enumerate(groups):
            original_case=Path(result["command"][6])
            for receiver in node["lifecycle"]["receivers"]:
                require(receiver["reaped"] and receiver["exit_code"]==0 and receiver["seal_verified"] and receiver["frames"]=="4" and receiver["received_bytes"]=="16384","Catalog changed confirmed receiver data")
            operations=rows(case/("node-"+str(index))/"deployment/control/operations.jsonl")
            require(sum(e["event"]=="accepted" and json.loads(e["request"])["command"]=="catalog" for e in operations)==1,"Catalog data request replayed")
            if "catalog" not in node:
                require(failed and index==1,"Missing independent catalog completion")
                marker=case/("catalog-node-"+str(index))/"NativeIntegration/run_0000/ecc-network-fixture/existing-marker.txt"
                require(marker.read_text()=="Preserve this existing dataset\n","Pre-existing dataset overwritten")
                continue
            successes+=1; receipt=node["catalog"]
            directory=case/Path(receipt["directory"]).relative_to(original_case)
            require(json.loads((directory/"catalog-complete.json").read_text())==receipt and receipt["payload_method"]=="hardlink" and receipt["raw_payload_bytes_read"]=="0","Catalog actual completion receipt differs")
            require(sha(directory/"manifest.json")==receipt["manifest_sha256"],"Catalog manifest SHA differs")
            manifest=json.loads((directory/"manifest.json").read_text()); require(len(manifest["sources"])==1,"Catalog source set differs")
            linked=0
            for source in manifest["sources"]:
                copied_seal=directory/source["stored_directory"]/"receiver-seal.json"
                require(sha(copied_seal)==source["seal_sha256"],"Catalog copied seal binding differs")
                source_directory=case/Path(source["original_directory"]).relative_to(original_case)
                for file in source["files"]:
                    original=source_directory/file["name"]; archived=directory/source["stored_directory"]/file["name"]
                    a,b=original.stat(),archived.stat()
                    require((a.st_dev,a.st_ino,a.st_size)==(b.st_dev,b.st_ino,int(file["bytes"])) and a.st_nlink==2,"Catalog copied/moved payload or inode differs")
                    linked+=1
            require(linked==int(receipt["linked_files"]),"Catalog link count differs")
        require(successes==(1 if failed else 2),"Independent catalog success count differs")
        if not failed:
            require(json.loads((run/"data-catalogs.json").read_text())["data"]["nodes"]==groups,"GUI catalog references differ from actual nodes")
        credentials={p for name in ("credential","ecc-credential") for p in case.rglob(name)}
        require(not snapshots or not credentials,"Catalog archive contains credentials")
        values=[p.read_bytes().strip() for p in credentials]
        for path in case.rglob("*"):
            if path.is_file() and path not in credentials: require(not any(v in path.read_bytes() for v in values),"Catalog proof exposed credential value")
    return len(programs)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal",type=Path); parser.add_argument("sanitized",type=Path)
    parser.add_argument("--qt-build-dirs",type=Path,nargs="+",required=True); parser.add_argument("--native-sanitizer-build-dir",type=Path,required=True)
    parser.add_argument("--archive",type=Path); parser.add_argument("--historical-only",action="store_true")
    args=parser.parse_args(); root=Path(__file__).resolve().parent.parent
    require(not args.historical_only or args.archive,"Historical checking requires archive")
    archive=args.archive.resolve() if args.archive else None
    if archive:
        manifest=json.loads((archive/"files-sha256.json").read_text())
        require(set(manifest)=={str(p.relative_to(archive)) for p in archive.rglob("*") if p.is_file() and p.name!="files-sha256.json"},"Catalog archive file set differs")
        for name,digest in manifest.items(): require(sha(archive/name)==digest,"Catalog archived bytes changed")
    for kind,directory in (("normal",args.normal),("sanitized",args.sanitized)): check_run(directory.resolve(),archive/"programs"/kind if args.historical_only else None)
    source_root=archive/"verified-sources" if args.historical_only else root
    for build in args.qt_build_dirs+[args.native_sanitizer_build_dir]:
        for name,digest in json.loads((build/"source-sha256.json").read_text()).items(): require(sha(source_root/name)==digest,"Catalog compiled source changed: "+name)
    if not args.historical_only:
        programs=json.loads((args.normal/"program-sha256.json").read_text())
        for name,pattern in (("receiver",r"lib(?:Qt|Ice|yaml|curl)"),("fixture",r"libQt"),("qt_probe",r"lib(?:Ice|curl)")):
            require(not re.search(pattern,subprocess.check_output(["ldd",programs[name]["path"]],text=True)),"Catalog raw/control dependency boundary changed")
    print("PASS six actual Qt/ECC catalog cases: two encodings and partial failure, single original requests, preserved source inodes, existing target, GUI completion/gates, source hashes and credentials; historical",args.historical_only)

if __name__=="__main__": main()
