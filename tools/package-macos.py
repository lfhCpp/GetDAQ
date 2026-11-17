#!/usr/bin/env python3
"""Package a completed native macOS suite as GetDAQ.app with its helper programs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)


def program_plan(receipt):
    if receipt.get('status')!='complete':raise ValueError('A completed macOS build receipt is required')
    programs=receipt.get('programs',{})
    required={'GetDAQ','getdaq-cconfig-editor','getdaq-cobo-info','getdaq-waveform-viewer'}
    if not required.issubset(programs):raise ValueError('Missing required GUI/helper programs: '+str(sorted(required-set(programs))))
    result={}
    for name,record in programs.items():
        if not re.fullmatch(r'[A-Za-z0-9_-]+',name):raise ValueError('Invalid program name: '+name)
        path=Path(record['path'])
        if not path.is_file() or suite.sha(path)!=record['sha256']:raise ValueError('Build program missing or changed: '+name)
        if not record.get('runtime',{}).get('mach_o'):raise ValueError('Program has no native Mach-O evidence: '+name)
        result[name]=path
    return result


def minimum_macos(program):
    output=subprocess.check_output(['otool','-l',str(program)],text=True)
    for command in output.split('Load command'):
        if 'LC_BUILD_VERSION' in command or 'LC_VERSION_MIN_MACOSX' in command:
            match=re.search(r'^\s*(?:minos|version)\s+([0-9.]+)',command,re.M)
            if match:return match.group(1)
    raise ValueError('No macOS deployment target in Mach-O program: '+str(program))


def package(receipt_path,destination,macdeployqt,sign=True):
    receipt=json.loads(receipt_path.read_text());programs=program_plan(receipt)
    if destination.exists():raise ValueError('Choose a new output directory: '+str(destination))
    for path in programs.values():suite.macos_architectures(path)
    destination.mkdir(parents=True)
    incomplete=destination/'BUILD_INCOMPLETE';incomplete.write_text('Do not deploy until packaging finishes.\n')
    app=destination/'GetDAQ.app';contents=app/'Contents';binaries=contents/'MacOS';binaries.mkdir(parents=True)
    architecture=suite.macos_architectures(programs['GetDAQ'])['host_architecture']
    info={'CFBundleExecutable':'GetDAQ','CFBundleName':'GetDAQ','CFBundleDisplayName':'GetDAQ',
          'CFBundleIdentifier':'org.getdaq.GetDAQ','CFBundlePackageType':'APPL',
          'CFBundleShortVersionString':'0.1.0','CFBundleVersion':'1',
          'LSMinimumSystemVersion':minimum_macos(programs['GetDAQ']),
          'NSHighResolutionCapable':True}
    (contents/'Info.plist').write_bytes(plistlib.dumps(info))
    for name,path in programs.items():shutil.copy2(path,binaries/name)
    for source,relative in suite.stage_files():
        target=contents/relative if relative.startswith('share/') else destination/relative
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    deploy=[macdeployqt,str(app),'-always-overwrite']
    deploy += ['-executable='+str(binaries/name) for name in programs if name!='GetDAQ']
    subprocess.run(deploy,check=True)
    # Inspect the packaged Mach-O helpers as well, not the Linux binaries in a copied bin/.
    dependencies={name:subprocess.check_output(['otool','-L',str(binaries/name)],text=True) for name in programs}
    for name,output in dependencies.items():
        for line in output.splitlines()[1:]:
            library=line.strip().split(' (',1)[0]
            if library.startswith('/') and not library.startswith(('/System/Library/','/usr/lib/')):
                raise ValueError('Packaged program still uses a developer-machine library: '+name+' -> '+library)
    if sign:
        subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
        subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
    launcher=destination/'start-getdaq.command'
    launcher.write_text('#!/bin/bash\nset -e\ngetdaq_dir="$(cd "$(dirname "$0")" && pwd)"\nexec open "$getdaq_dir/GetDAQ.app" --args "$@"\n');launcher.chmod(0o755)
    result={'schema':'getdaq.macos-app:1','status':'complete','architecture':architecture,
            'app':'GetDAQ.app','programs':{name:hashlib.sha256((binaries/name).read_bytes()).hexdigest() for name in programs},
            'qt_deployment':'macdeployqt','signature':'ad-hoc' if sign else 'not-signed',
            'notarized':False,'dependencies':dependencies}
    (destination/'macos-package-receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    incomplete.unlink();return app

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt',type=Path,required=True);parser.add_argument('--destination',type=Path,required=True)
    parser.add_argument('--macdeployqt',default=shutil.which('macdeployqt'))
    parser.add_argument('--no-sign',action='store_true')
    args=parser.parse_args()
    if sys.platform!='darwin':parser.error('Run this packager on macOS with native Mach-O build outputs')
    if not args.macdeployqt:parser.error('macdeployqt from the selected Qt 5 kit is required')
    try:print('Packaged '+str(package(args.receipt.resolve(),args.destination.resolve(),args.macdeployqt,not args.no_sign)))
    except (OSError,ValueError,subprocess.CalledProcessError) as error:sys.exit(str(error))
