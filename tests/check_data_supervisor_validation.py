#!/usr/bin/env python3
"""检查当前来源、完整归档、链接与控制/数据依赖边界，不替代实机验收。"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote

def digest(path):
    sha=hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b""):
            sha.update(chunk)
    return sha.hexdigest()

def main():
    root=Path(__file__).resolve().parent.parent
    archive=root/"build-ice-compat/validation-data-supervisor"
    sources=json.loads((archive/"source-sha256.json").read_text())
    for name,sha in sources.items():
        assert digest(root/name)==sha, name
        assert digest(archive/"verified-sources"/name)==sha, name
    for name,sha in json.loads((archive/"archive-sha256.json").read_text()).items():
        assert digest(archive/name)==sha, name
    for name,sha in json.loads((archive/"validated-binary-sha256.json").read_text()).items():
        assert digest(Path(name))==sha, name
    for name,sha in json.loads((archive/"native-dependency-sha256.json").read_text()).items():
        assert digest(root/name)==sha, name
    print("PASS current/archived source inputs:",len(sources),"; archive files/binaries/existing dependencies match hashes")
    links=0
    for path in [root/"README.md",root/"REWRITE_PROPOSAL.md",root/"third_party/README.md"]+list((root/"docs").glob("*.md")):
        text=path.read_text()
        for match in re.finditer(r"\]\(",text):
            start=match.end()
            if text[start:start+1]=="<":
                end=text.index(">",start+1); target=text[start+1:end]
            else:
                depth,end=1,start
                while end<len(text) and depth:
                    if text[end]=="(" and (not end or text[end-1]!="\\"): depth+=1
                    if text[end]==")" and (not end or text[end-1]!="\\"): depth-=1
                    end+=1
                assert depth==0,(path,start)
                target=text[start:end-1].split(' "',1)[0]
            if target.startswith(("https:","http:","mailto:","#","app:")): continue
            target=unquote(target.split("#",1)[0]).replace("\\(","(").replace("\\)",")")
            if target: assert (path.parent/target).exists(),(path,target); links+=1
    manifest=json.loads((root/"third_party/dependencies.json").read_text())
    for dep in manifest["dependencies"]:
        if "sha256" in dep:
            path=root/"third_party"/dep["local_path"]; assert path.stat().st_size==dep["size_bytes"] and digest(path)==dep["sha256"]
        for name in dep.get("license_files",[])+([dep["license_file"]] if "license_file" in dep else []): assert (root/"third_party"/name).is_file()
    assert (root/"source/get/GetBench-software-release-20210208/COPYING").read_bytes()==(root/"third_party/licenses/getbench/COPYING").read_bytes()
    manager=manifest["data_supervisor"]; native=manifest["native_ecc_coordinator"]
    assert manager["new_third_party_dependencies"]==[] and native["receiver_process_management"] and native["real_receiver_validation_by_controller"]
    for flag in ("qt_dependency","ice_dependency","yaml_dependency","raw_data_passes_through_client","remote_supervisor_service","production_network_server","unknown_recovery_implemented","automatic_kill_on_finish_timeout","automatic_restart_or_signal_retry"): assert manager[flag] is False,flag
    for name,forbidden in (("build-data-supervisor/data-supervisor-tests",("Qt","Ice","yaml")),("build-receiver/getdaq-receiver",("Qt","Ice","yaml")),("build-ice-compat/native-ecc-receiver-tests",("Qt",))):
        libraries=subprocess.check_output(["ldd",str(root/name)],text=True); assert not any(word in libraries for word in forbidden),(name,libraries)
    tests=subprocess.check_output(["ctest","-N"],cwd=str(root/"build-ice-compat"),text=True)
    assert "Total Tests: 52" in tests
    print("PASS",links,"local Markdown links; embedded archives/licenses; control/data dependency boundaries; 52 registered tests (not a 52/52 execution claim)")

if __name__=="__main__":
    main()
