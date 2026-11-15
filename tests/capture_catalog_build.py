#!/usr/bin/env python3
"""记录当前原生归档CMake编译输入/程序；执行同版增量构建并检查输入不变。"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    root=Path(__file__).resolve().parent.parent
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("build_dir",type=Path); parser.add_argument("--jobs",type=int,default=3)
    args=parser.parse_args(); build=args.build_dir.resolve()
    subprocess.run(["cmake","-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",str(root/"apps/datasupervisor")],cwd=build,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    compiled=json.loads((build/"compile_commands.json").read_text()); tracked=set()
    for row in compiled:
        file=Path(row["file"]).resolve()
        if root in file.parents: tracked.add(file)
    for folder in ("common/experiment","common/storage","common/text","common/http","apps/datasupervisor"):
        tracked.update((root/folder).rglob("*.h")); tracked.update((root/folder).rglob("CMakeLists.txt"))
    tracked.add(Path(__file__).resolve())
    before={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    with (build/"results-catalog-verified-build.txt").open("w") as log:
        result=subprocess.run(["cmake","--build",str(build),"-j"+str(args.jobs)],stdout=log,stderr=subprocess.STDOUT)
    if result.returncode: raise SystemExit(result.returncode)
    after={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    if before!=after: raise RuntimeError("Catalog build inputs changed during compilation")
    programs={name:sha(build/name) for name in ("getdaq-catalog","getdaq-supervisor-query","getdaq-supervisor","receiver-catalog-tests","data-supervisor-tests","supervisor-control-tests") if (build/name).is_file()}
    commands=[shlex.split(row["command"]) for row in compiled]
    sanitized=any("-fsanitize=address,undefined" in command for command in commands)
    if sanitized:
        assert all("-fsanitize=address,undefined" in command and "-std=c++11" in command and "-pedantic-errors" in command for command in commands)
    (build/"source-sha256.json").write_text(json.dumps(after,indent=2)+"\n")
    (build/"catalog-build-receipt.json").write_text(json.dumps({"schema":1,"sanitized":sanitized,"source_count":len(after),"programs":programs,"system_qt_instrumented":False},indent=2)+"\n")
    print("PASS catalog current compiled inputs",len(after),"programs",len(programs),"sanitized",sanitized)

if __name__=="__main__": main()
