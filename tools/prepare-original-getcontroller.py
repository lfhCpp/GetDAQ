#!/usr/bin/env python3
"""Prepare original GetController and its offline libraries in an isolated build tree."""
import argparse
import hashlib
import importlib.util
import json
import platform
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'third_party/legacy-get'


def prepare(destination, ice_build, target_system=None):
    target_system=target_system or platform.system()
    if target_system not in ("Linux", "Darwin"):raise ValueError("Unsupported original GetController platform")
    module = importlib.util.spec_from_file_location('original_cconfig', ROOT / 'tools/prepare-original-cconfig.py')
    cconfig = importlib.util.module_from_spec(module);module.loader.exec_module(cconfig)
    cconfig.prepare(destination)
    inputs = json.loads((destination / 'original-inputs.json').read_text())
    packages = {
        'controller': 'controller',
        'getbench': 'getbench',
        'mdaq': 'mdaq',
        'multiframe': 'multiframe',
        'utilityfull': 'utilities',
    }
    for name, directory in packages.items():
        original = VENDOR / directory
        for path in original.rglob('*'):
            if not path.is_file() or '.svn' in path.parts:
                continue
            relative = path.relative_to(original)
            if relative.parts[0] not in ('src','res') and path.name not in ('COPYING','Makefile.am'):
                continue
            data = path.read_bytes();inputs[str(path.relative_to(ROOT))] = hashlib.sha256(data).hexdigest()
            if name == 'controller' and path.suffix in ('.cpp','.h'):
                text = data.decode('utf-8')
                text = re.sub(r'["<]get/(\w+)\.ui\.h[">]',r'"ui_\1.h"',text)
                data = text.encode('utf-8')
            target = destination / name / relative
            target.parent.mkdir(parents=True,exist_ok=True)
            if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    qwt=destination/'qwt-6.1.4'
    for path in (VENDOR/'qwt-6.1.4').rglob('*'):
        if not path.is_file():continue
        data=path.read_bytes();inputs[str(path.relative_to(ROOT))]=hashlib.sha256(data).hexdigest()
        target=qwt/path.relative_to(VENDOR/'qwt-6.1.4');target.parent.mkdir(parents=True,exist_ok=True)
        if path.suffix=='.h' and b'QPainterPath' in data and b'#include <QPainterPath>' not in data:
            data=b'// GetDAQ build compatibility: explicit type include for Qt 5.15.\n#include <QPainterPath>\n'+data
        if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    boost=destination/'boost_1_55_0'
    ice=next((ice_build/'vendor').glob('ice-*/cpp/include')).parent.parent
    generated=destination/'generated/IceStorm';generated.mkdir(parents=True,exist_ok=True)
    for interface in (ice/'slice/IceStorm').glob('*.ice'):
        if not (generated/(interface.stem+'.cpp')).exists():
            subprocess.run([str(ice_build/'slice2cpp'),'--ice','-I'+str(ice/'slice'),'--include-dir','IceStorm','--output-dir',str(generated),str(interface)],check=True)
    sources=[]
    # Reuse exactly the configuration-editor sources, excluding its executable entry point.
    text=(destination/'original.pri').read_text()
    sources += [Path(p) for p in re.findall(r'^SOURCES \+= "([^"]+)"',text,re.M) if not p.endswith('/widgets/src/main.cpp')]
    src=destination/'controller/src'
    text=(src/'Makefile.am').read_text().split('\nsources = ',1)[1].split('\nmoc_sources',1)[0]
    sources += [src/p for p in re.findall(r'[\w/]+\.cpp',text)]
    mdaq=destination/'mdaq/src';get=destination/'getbench/src';utils=destination/'utilityfull/src'
    sources += list((destination/'multiframe/src/mfm').glob('*.cpp'))
    sources += [mdaq/p for p in [
        'mdaq/utl/CmdLineArgs.cpp','mdaq/utl/Server.cpp','mdaq/utl/ConsoleLoggingBackend.cpp',
        'mdaq/utl/IceStormLoggingBackend.cpp','mdaq/utl/IceStormLoggingSubscriber.cpp',
        'mdaq/utl/net/IpDetect.cpp','mdaq/utl/net/Socket.cpp','mdaq/utl/net/TcpAcceptor.cpp',
        'mdaq/utl/net/TcpConnection.cpp','mdaq/utl/net/UdpTransceiver.cpp','mdaq/utl/net/SocketAddress.cpp']]
    sources += list((mdaq/'mdaq/mt').glob('*.cpp'))
    sources += [mdaq/('mdaq/daq/'+p+'.cpp') for p in ['CircularBuffer','DataProcessorCore','DataReceiver','TcpDataReceiver','IceDataReceiver','NetPerfProcessor','DataSender','IceDataSender']]
    sources += [get/p for p in ['get/asad/utl.cpp','get/rc/SM.cpp','get/mt/AlarmLogger.cpp','get/utl/Time.cpp']]
    sources += [get/('get/daq/'+p+'.cpp') for p in ['DataRouter','FrameStorage','ByteStorage','FdtDataReceiver','FrameCounter']]
    sources += [utils/p for p in ['utl/BinIO.cpp','utl/EnumStringMapper.cpp','utl/Exception.cpp','utl/net/IpAddress.cpp','utl/net/SocketAddress.cpp']]
    sources += list((boost/'libs/program_options/src').glob('*.cpp'))
    sources += list((qwt/'src').glob('*.cpp'))
    sources += list(generated.glob('*.cpp'))
    sources += [get/'get/rc/CoBoConfigParser.cpp',get/'get/rc/MutantConfigParser.cpp']
    sources += [p for p in (boost/'libs/filesystem/src').glob('*.cpp') if p.name!='windows_file_codecvt.cpp']
    sources += list((boost/'libs/system/src').glob('*.cpp'))
    if target_system=='Darwin':
        # Same exclusions as the original upstream Apple build, which lacks POSIX timer_create.
        excluded={'mdaq/mt/Timer.cpp','mdaq/mt/SigAction.cpp','mdaq/daq/NetPerfProcessor.cpp','get/daq/FrameCounter.cpp'}
        sources=[p for p in sources if not any(p.as_posix().endswith('/'+name) for name in excluded)]
    assert len(sources)==len(set(sources)) and all(p.is_file() for p in sources)
    def q(p):return '"'+str(p).replace('\\','/')+'"'
    # Unique wrappers avoid collisions between multiple Exception.cpp/SocketAddress.cpp/etc.
    units=destination/'units';units.mkdir(exist_ok=True)
    lines=[]
    for index,source in enumerate(sources):
        unit=units/('unit_%03d.cpp'%index)
        # Qt 5.15 no longer supplies QPainterPath through QPainter's includes.
        prefix='#include <QPainterPath>\n' if source.parent==qwt/'src' and 'QPainterPath' in source.read_text() else ''
        data=prefix+'#include '+q(source)+'\n'
        if not unit.exists() or unit.read_text()!=data:unit.write_text(data)
        lines.append('SOURCES += '+q(unit))
        if source==destination/'controller/src/get/main.cpp':lines.append('GETDAQ_CONTROLLER_MAIN = '+q(unit))
    includes=[destination/'controller/src',destination/'generated',ice_build/'generated',ice/'cpp/include']
    includes += [destination/p for p in ['widgets/src','compound/src','tinyxml/src','utilityfull/src','mdaq/src','getbench/src','multiframe/src','boost_1_55_0','qwt-6.1.4/src']]
    lines.append('INCLUDEPATH += '+' '.join(q(p) for p in includes))
    for directory in [destination/'widgets/src',destination/'controller/src',qwt/'src']:
        headers=list(directory.rglob('*.h'))
        if directory==destination/'controller/src':
            manifest=(directory/'Makefile.am').read_text().split('headers =',1)[1].split('\nsources =',1)[0]
            headers=sorted({directory/p for p in re.findall(r'[\w/]+\.h',manifest)})
        lines += ['HEADERS += '+q(p) for p in headers]
        lines += ['FORMS += '+q(p) for p in directory.rglob('*.ui')]
    lines += ['RESOURCES += '+q(destination/'widgets/src/ConfigWidgets.qrc'), 'RESOURCES += '+q(destination/'controller/src/GetController.qrc')]
    lines.append('LIBS += '+' '.join(q(ice_build/('libcompat-'+p+'.a')) for p in ['GET','Ice','IceUtil','bzip'])+(' -lpthread' if target_system=='Darwin' else ' -ldl -lpthread -lrt'))
    (destination/'getcontroller.pri').write_text('\n'.join(lines)+'\n')
    (destination/'original-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--destination',type=Path,required=True);parser.add_argument('--ice-build',type=Path,required=True);args=parser.parse_args()
    prepare(args.destination.resolve(),args.ice_build.resolve())
