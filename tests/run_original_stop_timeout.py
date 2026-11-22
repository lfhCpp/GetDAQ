#!/usr/bin/env python3
"""Run the original CoBo stop timeout fault probe after building bundled backends."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--original-build',type=Path,required=True,help='Directory containing build/liboriginal-get.a')
p.add_argument('--ice-build',type=Path,required=True)
a=p.parse_args();build=a.original_build.resolve()/'build';ice=a.ice_build.resolve()
flags=(build/'CMakeFiles/original-get.dir/flags.make').read_text();arguments=[]
for name in ['CXX_FLAGS','CXX_DEFINES','CXX_INCLUDES']:
    arguments+=shlex.split(next(line.split(' = ',1)[1] for line in flags.splitlines() if line.startswith(name+' = ')))
libs=[build/'liboriginal-get.a']+[ice/('libcompat-'+name+'.a') for name in ['GET','Ice','IceUtil','bzip']]
with tempfile.TemporaryDirectory(prefix='getdaq-stop-timeout-') as directory:
    binary=Path(directory)/'probe'
    subprocess.run(['c++',*arguments,str(Path(__file__).with_name('original_stop_timeout_probe.cpp')),*map(str,libs),'-lpthread','-ldl','-lrt','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=45)
