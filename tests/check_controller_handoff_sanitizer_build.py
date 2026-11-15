#!/usr/bin/env python3
"""Verify the new fixture's headers against values recorded before overlay compilation."""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base",type=Path);parser.add_argument("overlay",type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    old=json.loads((args.base/"source-sha256.json").read_text())
    receipt=json.loads((args.overlay/"build-receipt.json").read_text())
    known=dict(receipt["dependencies"]);known.update(receipt["tracked_inputs"])
    command=next(c.copy() for c in receipt["commands"] if "-c" in c and Path(c[c.index("-c")+1]).name=="native_ecc_remote_receiver_test.cpp")
    command.remove("-c");i=command.index("-o");del command[i:i+2];command += ["-MM","-MT","handoff-fixture"]
    text=subprocess.check_output(command,text=True,cwd=root)
    paths=[Path(p).resolve() for p in shlex.split(text.replace("\\\n"," ").split(":",1)[1])]
    verified={}
    for path in paths:
        expected=known.get(str(path))
        if expected is None and root in path.parents:expected=old.get(str(path.relative_to(root)))
        if expected is None or sha(path)!=expected:raise RuntimeError("New/changed fixture dependency lacks precompile evidence: "+str(path))
        verified[str(path)]=expected
    target=args.overlay/"fixture-dependencies.json"
    target.write_text(json.dumps(verified,indent=2)+"\n")
    print("PASS",len(verified),"fixture dependencies match pre-existing baseline/overlay source records")


if __name__=="__main__":main()
