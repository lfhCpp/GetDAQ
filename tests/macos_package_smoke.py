#!/usr/bin/env python3
"""Open a packaged GUI and empty original tool dialogs; never accepts hardware work."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True);parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='darwin':parser.error('Run the native app smoke check on macOS')
    package=args.package.resolve();binary=package/'GetDAQ.app/Contents/MacOS'
    evidence=args.evidence.resolve();evidence.mkdir(parents=True,exist_ok=True)
    result={'status':'incomplete','hardware_commands':False,'programs':{}}
    with tempfile.TemporaryDirectory(prefix='getdaq-macos-app-smoke-') as temporary:
        home=Path(temporary);env=os.environ.copy();env['HOME']=str(home)
        for key in ('XDG_CONFIG_HOME','XDG_CACHE_HOME','XDG_DATA_HOME','XDG_RUNTIME_DIR'):
            folder=home/key.lower();folder.mkdir(mode=0o700);env[key]=str(folder)
        env['QT_QPA_PLATFORM']='offscreen'
        screenshot=evidence/'GetDAQ.png'
        with (evidence/'GetDAQ.log').open('w') as log:
            subprocess.run([str(binary/'GetDAQ'),'--screenshot',str(screenshot)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
        if not screenshot.is_file() or screenshot.stat().st_size==0:raise RuntimeError('Packaged main window did not render')
        result['programs']['GetDAQ']='rendered'
        formats=package/'GetDAQ.app/Contents/share/get-bench/format/CoboFormats.xcfg'
        for name,options in [('getdaq-cconfig-editor',[]),('getdaq-getcontroller',['--format',str(formats)])]:
            with (evidence/(name+'.log')).open('w') as log:
                process=subprocess.Popen([str(binary/name)]+options,env=env,stdout=log,stderr=subprocess.STDOUT)
                try:
                    time.sleep(2)
                    if process.poll() is not None:raise RuntimeError('Packaged original tool exited before its empty dialog was opened: '+name)
                    result['programs'][name]='opened; terminated before accepting any work'
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:process.wait(timeout=5)
                        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    result['status']='complete';(evidence/'receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: packaged main GUI and original tool dialogs; no hardware work accepted')

if __name__=='__main__':main()
