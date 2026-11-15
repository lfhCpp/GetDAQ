#!/usr/bin/env python3
"""One-fixture strict sanitizer overlay; verify unchanged baseline inputs and programs."""
import argparse,json,shlex,subprocess
from pathlib import Path
from run_receiver_observation_tests import sha

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base",type=Path);parser.add_argument("output",type=Path);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];base=args.base.resolve();out=args.output.resolve();assert base!=out
    out.mkdir(parents=True,exist_ok=True)
    changed=root/"tests/ecc_network_service_test.cpp";added=root/"tests/receiver_observation_barrier.h"
    old=json.loads((base/"input-sha256.json").read_text());receipt=json.loads((base/"build-receipt.json").read_text())
    for name,digest in old.items():
        if Path(name)!=changed:assert sha(Path(name))==digest,name
    for name,digest in receipt["program_sha256"].items():assert sha(base/name)==digest,name
    for name,digest in receipt["vendor_library_sha256"].items():
        path=Path(name);path=path if path.is_absolute() else root/path;assert sha(path)==digest,path
    commands=json.loads((base/"commands.json").read_text())
    compile=next(c.copy() for c in commands if "-c" in c and str(changed) in c)
    assert all(flag in compile for flag in ["-std=c++11","-pedantic-errors","-fsanitize=address,undefined"])
    previous=compile[compile.index("-o")+1];current=str(out/"fixture.o");compile[compile.index("-o")+1]=current
    dependencies=compile.copy();dependencies.remove("-c");i=dependencies.index("-o");del dependencies[i:i+2];dependencies+=["-MM","-MT","barrier"]
    values=subprocess.check_output(dependencies,text=True).replace("\\\n"," ").split(":",1)[1]
    inputs={str(Path(p).resolve()):sha(Path(p).resolve()) for p in shlex.split(values)}
    for name,digest in inputs.items():
        if Path(name) not in (changed,added):assert old.get(name)==digest,name
    with (out/"compile.txt").open("w") as log:subprocess.run(compile,stdout=log,stderr=subprocess.STDOUT,check=True)
    link=next(c.copy() for c in commands if "-c" not in c and "-o" in c and Path(c[c.index("-o")+1])==base/"ecc-network-service-tests")
    reused={str(Path(word)):sha(Path(word)) for word in link if word.endswith(".o") and word!=previous}
    link=[current if word==previous else word for word in link];link[link.index("-o")+1]=str(out/"ecc-network-service-tests")
    with (out/"link.txt").open("w") as log:subprocess.run(link,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(sha(Path(name))==digest for name,digest in inputs.items())
    assert all(sha(Path(name))==digest for name,digest in reused.items())
    inputs[str(Path(__file__).resolve())]=sha(Path(__file__))
    (out/"input-sha256.json").write_text(json.dumps(inputs,indent=2)+"\n")
    (out/"commands.json").write_text(json.dumps([compile,link],indent=2)+"\n")
    (out/"build-receipt.json").write_text(json.dumps({"schema":1,"sanitized":True,"compile_count":1,"reused_objects":reused,
        "base_receipt_sha256":sha(base/"build-receipt.json"),"program_sha256":sha(out/"ecc-network-service-tests"),
        "borrowed_ecc_service":str(base/"getdaq-ecc-server"),"borrowed_ecc_service_sha256":sha(base/"getdaq-ecc-server"),
        "system_qt_instrumented":False,"old_vendor_libraries_instrumented":False},indent=2)+"\n")
    print("PASS strict receiver-observation barrier overlay; one compile,",len(reused),"objects checked")

if __name__=="__main__":main()
