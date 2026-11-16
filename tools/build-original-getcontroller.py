#!/usr/bin/env python3
"""Build the original GetController offline without installing legacy system libraries."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--ice-build',type=Path)
    parser.add_argument('--destination',type=Path,default=ROOT/'bin')
    parser.add_argument('--qmake',default='qmake');parser.add_argument('--make',default='make')
    parser.add_argument('--cmake',default='cmake');parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--svg-headers',type=Path,help='Optional existing Qt SVG header directory when the Qt SVG qmake module is unavailable')
    parser.add_argument('--configure-only',action='store_true')
    args=parser.parse_args()
    if sys.platform!='linux':parser.error('This original GetController integration is currently validated on Linux only')
    if args.jobs<1:parser.error('--jobs must be positive')
    build=args.build_dir.resolve();build.mkdir(parents=True,exist_ok=True)
    ice=args.ice_build.resolve() if args.ice_build else build/'ice'
    if not (ice/'libcompat-GET.a').exists():
        subprocess.run([args.cmake,'-S',str(ROOT/'third_party/ice-compat'),'-B',str(ice),'-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release','-DGETDAQ_ICE_LIBRARIES_ONLY=ON'],check=True)
        subprocess.run([args.cmake,'--build',str(ice),'--parallel',str(args.jobs),'--target','compat-GET'],check=True)
    destination=args.destination.resolve();destination.mkdir(parents=True,exist_ok=True)
    command=[args.qmake,str(ROOT/'apps/getcontroller/getcontroller.pro'),'GETDAQ_ICE_BUILD='+str(ice),'GETDAQ_DESTDIR='+str(destination),'GETDAQ_PYTHON='+sys.executable]
    if args.svg_headers:command.append('GETDAQ_SVG_HEADERS='+str(args.svg_headers.resolve()))
    subprocess.run(command,cwd=build,check=True)
    if args.configure_only:return
    subprocess.run([args.make,'-j'+str(args.jobs)],cwd=build,check=True)
    share=destination.parent/'share'
    formats=share/'get-bench/format';formats.mkdir(parents=True,exist_ok=True)
    for source in (ROOT/'third_party/legacy-get/getbench/data/format').glob('*.xcfg'):shutil.copy2(source,formats/source.name)
    licenses=share/'licenses/getcontroller';licenses.mkdir(parents=True,exist_ok=True)
    for source in (ROOT/'apps/getcontroller/licenses').iterdir():shutil.copy2(source,licenses/source.name)
    print('Built '+str(destination/'getdaq-getcontroller'))

if __name__=='__main__':main()
