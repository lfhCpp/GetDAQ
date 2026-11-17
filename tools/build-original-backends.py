#!/usr/bin/env python3
"""Build original SOAP ECC and ATTPC DataExporter from bundled sources, offline."""
import argparse,hashlib,json,os,re,shutil,subprocess,tarfile,importlib.util
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args,**kw):subprocess.run([str(a) for a in args],check=True,**kw)
def prepare(build,ice):
    run([os.sys.executable,ROOT/'tools/verify-original-backends.py'])
    v=ROOT/'third_party/legacy-get'; build.mkdir(parents=True,exist_ok=True)
    gsoap=build/'vendor/gsoap-2.8.124/gsoap'
    archive=ROOT/'third_party/archives/gsoap-2.8.124.orig.tar.gz'
    if hashlib.sha256(archive.read_bytes()).hexdigest()!='1f6727ccf0e84a5dc7c86a554d0b0e2adb12f66b6e601b21d081de9041443800':raise ValueError('Bundled gSOAP checksum mismatch')
    if not gsoap.is_dir():
        (build/'vendor').mkdir(exist_ok=True)
        with tarfile.open(archive) as t:
            base=(build/'vendor').resolve()
            for member in t.getmembers():
                target=(base/member.name).resolve()
                if (target!=base and base not in target.parents) or member.issym() or member.islnk():raise ValueError('Unsafe gSOAP archive member')
            t.extractall(base)
    generator=build/'soapcpp2'
    if not generator.exists():
        src=gsoap/'src';lex=gsoap/'VisualStudio2005/soapcpp2/soapcpp2/lex.soapcpp2_lex.c'
        run([os.environ.get('CC','cc'),'-O2','-DWITH_YACC','-DLINUX','-I'+str(src),'-I'+str(gsoap),src/'soapcpp2_yacc.tab.c',lex,*[src/(n+'.c') for n in ['symbol2','error2','init2','soapcpp2']],'-o',generator])
    spec=importlib.util.spec_from_file_location('entry_patches',ROOT/'tools/original-backend-patches.py');patcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(patcher)
    main=build/'getEccSoapServer.cpp'
    main.write_text(patcher.apply_patch((v/'getbench/src/get/soap/getEccSoapServer.cpp').read_text(),(ROOT/'apps/original-backends/ecc-entry.patch').read_text()))
    router_main=build/'dataRouter.cpp';router_main.write_text(patcher.apply_patch((v/'getbench/src/main/dataRouter.cpp').read_text(),(ROOT/'apps/original-backends/datarouter-entry.patch').read_text()))
    bindings=build/'bindings';bindings.mkdir(exist_ok=True)
    run([generator,'-z1','-L','-x','-pecc','-n','-d'+str(bindings),v/'getbench/src/get/soap/ecc.hh'])
    # Only interface/compiler adaptation; hardware control sources remain upstream.
    (build/'config.h').write_text('/* Optional database/logging backends disabled. */\n')
    compat=build/'compat.h';compat.write_text('''#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#define ICE_EDITION Ice
#define QUOTE_IT(x) #x
#define ICEUTIL_HEADER(header) QUOTE_IT(IceUtil/header)
#define ICE_HEADER(header) QUOTE_IT(Ice/header)
#define CFG_DB_SUPPORT_ENABLED 0
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define WORDS_LITTLE_ENDIAN 1
#else
#define WORDS_BIG_ENDIAN 1
#endif
''')
    compound=v/'compound/src';text=(compound/'Makefile.am').read_text();blocks=[text.split('\nsources = ',1)[1].split('\n\nAM_CXXFLAGS',1)[0]]
    for sec in ['BUILD_CCFG_FMT_XML','BUILD_CCFG_FMT_BINARY']:blocks.append(text.split('if '+sec,1)[1].split('endif',1)[0].split('sources +=',1)[1])
    sources=[compound/p for p in re.findall(r'[\w/]+\.cpp','\n'.join(blocks))]
    sources += [v/'tinyxml/src'/p for p in ['tinyxml.cpp','tinystr.cpp','tinyxmlparser.cpp','tinyxmlerror.cpp']]
    sources += list((v/'multiframe/src/mfm').glob('*.cpp'))
    sources += [v/'utilities/src/utl'/p for p in ['Logging.cpp','BinIO.cpp','EnumStringMapper.cpp','Exception.cpp','net/IpAddress.cpp','net/SocketAddress.cpp']]
    sources += [v/'mdaq/src/mdaq'/p for p in ['utl/CmdLineArgs.cpp','utl/Server.cpp','utl/ConsoleLoggingBackend.cpp','utl/net/IpDetect.cpp','utl/net/Socket.cpp','utl/net/TcpAcceptor.cpp','utl/net/TcpConnection.cpp','utl/net/UdpTransceiver.cpp','utl/net/SocketAddress.cpp','EccBackEnd.cpp','rc/HardwareNode.cpp','hw/RegisterProcessor.cpp','hw/RemoteRegisterProcessor.cpp','cfg/NodeAtTop.cpp']]
    sources += list((v/'mdaq/src/mdaq/mt').glob('*.cpp'))
    sources += [v/'mdaq/src/mdaq/daq'/(n+'.cpp') for n in ['CircularBuffer','DataProcessorCore','DataReceiver','TcpDataReceiver','IceDataReceiver','NetPerfProcessor','DataSender','IceDataSender']]
    sources += [v/'getbench/src/get'/p for p in ['asad/utl.cpp','mt/AlarmLogger.cpp','utl/Time.cpp','daq/FrameStorage.cpp','daq/ByteStorage.cpp','daq/FdtDataReceiver.cpp','daq/FrameCounter.cpp']]
    sources += [p for p in (v/'getbench/src/get/rc').glob('*.cpp') if p.name!='ConfigDatabaseManager.cpp']
    sources += list((v/'statemachine/src/dhsm').glob('*.cpp'))
    boost=v/'boost_1_55_0'
    sources += list((boost/'libs/program_options/src').glob('*.cpp'))
    sources += [p for p in (boost/'libs/filesystem/src').glob('*.cpp') if p.name!='windows_file_codecvt.cpp']
    sources += list((boost/'libs/system/src').glob('*.cpp'))
    sources += list((boost/'libs/thread/src/pthread').glob('*.cpp'))
    sources += [boost/'libs/thread/src/future.cpp',boost/'libs/thread/src/tss_null.cpp']
    if os.uname().sysname=='Darwin':
        sources=[p for p in sources if p.name not in ['Timer.cpp','SigAction.cpp','NetPerfProcessor.cpp','FrameCounter.cpp']]
    includes=[v/p for p in ['compound/src','tinyxml/src','utilities/src','mdaq/src','getbench/src','getbench/src/get','multiframe/src','statemachine/src','boost_1_55_0']]
    includes += [v/'getbench/src/get/soap',build,ice/'generated',next((ice/'vendor').glob('ice-*/cpp/include')),bindings,gsoap]
    def q(p):return '"'+str(p)+'"'
    cmake=['cmake_minimum_required(VERSION 3.12)','project(GetDAQOriginalBackends LANGUAGES C CXX)','set(CMAKE_CXX_STANDARD 11)','set(CMAKE_CXX_STANDARD_REQUIRED ON)','find_package(Threads REQUIRED)', 'add_compile_definitions(ICE_STATIC_LIBS BOOST_ALL_NO_LIB ASIO_STANDALONE BUILD_CCFG_FMT_XML=1 BUILD_CCFG_FMT_BINARY=1 TIXML_USE_STL)', 'add_compile_options(-include '+q(compat)+' -Wno-deprecated-declarations)', 'include_directories('+ ' '.join(q(p) for p in includes)+')', 'add_library(original-get STATIC '+ ' '.join(q(p) for p in sources)+')']
    libs=' '.join(q(ice/('libcompat-'+n+'.a')) for n in ['GET','Ice','IceUtil','bzip'])
    cmake+=['target_link_libraries(original-get PUBLIC '+libs+' Threads::Threads ${CMAKE_DL_LIBS})','if(UNIX AND NOT APPLE)\n target_link_libraries(original-get PUBLIC rt)\nendif()']
    cmake+=['add_executable(getEccSoapServer '+ ' '.join(q(p) for p in [main,v/'getbench/src/get/soap/EccSoapServer.cpp',bindings/'eccC.cpp',bindings/'eccServer.cpp',gsoap/'stdsoap2.cpp'])+')','target_link_libraries(getEccSoapServer PRIVATE original-get)']
    dex=ROOT/'third_party/DataExporter/src'
    dex_main=build/'DataExporter-main.cpp';dex_main.write_text(patcher.apply_patch((dex/'main.cpp').read_text(),(ROOT/'apps/original-backends/dataexporter-entry.patch').read_text()))
    cmake+=['add_executable(DataExporter '+q(dex_main)+' '+ ' '.join(q(dex/p) for p in ['DataRouter.cpp','Exporter.cpp','server/TCPServer.cpp'])+')','target_include_directories(DataExporter PRIVATE '+q(dex)+' '+q(dex/'vendor/asio/asio/include')+')','target_link_libraries(DataExporter PRIVATE original-get)']
    cmake+=['add_executable(dataRouter '+q(router_main)+' '+q(v/'getbench/src/get/daq/DataRouter.cpp')+')','target_link_libraries(dataRouter PRIVATE original-get)']
    (build/'CMakeLists.txt').write_text('\n'.join(cmake)+'\n')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--ice-build',type=Path,required=True);p.add_argument('--jobs',type=int,default=2);p.add_argument('--prepare-only',action='store_true');a=p.parse_args();b=a.build_dir.resolve();prepare(b,a.ice_build.resolve())
    if not a.prepare_only:run(['cmake','-S',b,'-B',b/'build','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',b/'build','--parallel',a.jobs])
if __name__=='__main__':main()
