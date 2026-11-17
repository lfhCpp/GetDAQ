#!/usr/bin/env python3
"""Build natively on an Intel/Apple Silicon Mac and package the original GET tools."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=['control','full'],default='full')
    parser.add_argument('--build-root',type=Path,default=ROOT/'build-macos')
    parser.add_argument('--output',type=Path,default=ROOT/'dist-macos')
    parser.add_argument('--qmake',default=os.environ.get('GETDAQ_QMAKE'))
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--build-only',action='store_true')
    args=parser.parse_args()
    if sys.platform!='darwin':parser.error('Build this package on macOS; copied Ubuntu executables cannot be reused')
    qmake=args.qmake
    if not qmake and shutil.which('qmake'):
        candidate=shutil.which('qmake')
        version=subprocess.check_output([candidate,'-query','QT_VERSION'],text=True).strip()
        if version.startswith('5.'):qmake=candidate
    if not qmake and shutil.which('brew'):
        prefix=subprocess.check_output(['brew','--prefix','qt@5'],text=True).strip();qmake=str(Path(prefix)/'bin/qmake')
    if not qmake:parser.error('Qt 5 qmake is required; set GETDAQ_QMAKE to the selected Qt 5 kit')
    qmake=str(Path(shutil.which(qmake) or qmake).absolute())
    version=subprocess.check_output([qmake,'-query','QT_VERSION'],text=True).strip()
    if not version.startswith('5.'):parser.error('Select Qt 5 rather than Qt '+version)
    subprocess.run([sys.executable,str(ROOT/'tools/verify-legacy-sources.py')],check=True)
    build=args.build_root.resolve()
    subprocess.run([sys.executable,str(ROOT/'tools/build-suite.py'),'--profile',args.profile,
                    '--build-root',str(build),'--qmake',qmake,'--jobs',str(args.jobs)],check=True)
    if not args.build_only:
        subprocess.run([sys.executable,str(ROOT/'tools/package-macos.py'),'--receipt',str(build/args.profile/'build-receipt.json'),
                        '--destination',str(args.output.resolve()),'--macdeployqt',str(Path(qmake).parent/'macdeployqt')],check=True)

if __name__=='__main__':main()
