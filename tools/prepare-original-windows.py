"""Windows-only build copies for original GET socket/platform code.

No experiment settings, register semantics or upstream files are rewritten.
"""
from pathlib import Path
import re

def prepare(vendor, output, sources):
    changes={}
    def edit(name, change):
        path=vendor/name;original=path.read_text(encoding='utf-8');text=change(original)
        if text==original:raise ValueError('Windows adaptation did not match '+name)
        changes[name]=text
    edit('mdaq/src/mdaq/mt/SysClock.cpp',lambda s:s.replace('#include <ctime>', '#include <ctime>\n#include <thread>\n#include <chrono>').replace('Sleep(microSecs / 1000);','std::this_thread::sleep_for(std::chrono::microseconds(microSecs));'))
    edit('compound/src/CCfg/Io/Url.cpp',lambda s:s.replace('url.setPath(PathNormalizer::escape(path));', "std::string normalized=path; std::replace(normalized.begin(),normalized.end(),'\\\\','/');\n    url.setPath(PathNormalizer::escape(normalized));"))
    edit('getbench/src/get/soap/EccSoapServer.cpp',lambda s:s.replace('CCfg::Io::Url repoFullUrl(url);', 'if(url.find("://")==std::string::npos){backEnd_.setConfigManager(url);return;}\n    CCfg::Io::Url repoFullUrl(url);'))
    edit('getbench/src/get/rc/BackEnd.cpp',lambda s:s.replace('CCfg::Io::Url url(repoUrl);', 'CCfg::Io::Url url = repoUrl.find("://")==std::string::npos ? CCfg::Io::Url::fromPath(repoUrl) : CCfg::Io::Url(repoUrl);'))
    edit('getbench/src/get/daq/FrameStorage.cpp',lambda s:s.replace('fileTimestamp = get::utl::buildTimeStamp();','fileTimestamp = get::utl::buildTimeStamp();\n        std::replace(fileTimestamp.begin(),fileTimestamp.end(),\':\',\'-\');'))
    edit('getbench/src/get/daq/DataRouter.cpp',lambda s:s.replace('#ifndef __APPLE__', '#if !defined(__APPLE__) && !defined(_WIN32)'))
    edit('getbench/src/get/rc/ConfigFileManager.cpp',lambda s:s.replace('FileStorage outFile(configPath.c_str(),', 'FileStorage outFile(configPath.string().c_str(),').replace('CCfg::Io::Url(configPath.string())','CCfg::Io::Url::fromPath(configPath.generic_string())').replace('doc.loadFromFile(cfgUrl.format(), false)','doc.loadFromFile(cfgUrl.toPath(), false)'))
    edit('utilities/src/utl/net/IpAddress.h',lambda s:re.sub(r'struct in_addr_t : in_addr\n\{.*?\n\};', 'typedef u_long in_addr_t;', s, flags=re.S))
    edit('mdaq/src/mdaq/utl/net/IpDetect.h',lambda s:s.replace('#include <netinet/in.h>', '#include <utl/net/IpAddress.h>'))
    edit('mdaq/src/mdaq/utl/net/Socket.h',lambda s:s.replace('#if defined(VXWORKS)\ntypedef int SocklenType;', '#if defined(VXWORKS) || defined(WIN32)\ntypedef int SocklenType;').replace('int a_bsd_socket','SOCKET a_bsd_socket'))
    edit('mdaq/src/mdaq/utl/net/Socket.cpp',lambda s:s.replace('int a_bsd_socket','SOCKET a_bsd_socket').replace('void Socket::open()\n{','void Socket::open()\n{\n    GetDAQWindows::Winsock();').replace('struct timespec startTime, stopTime;\n\t\t\tclock_gettime(CLOCK_REALTIME, &startTime);','ULONGLONG startTime=GetTickCount64();').replace('clock_gettime(CLOCK_REALTIME, &stopTime);\n\t\t\tremaining_ms -= (stopTime.tv_sec - startTime.tv_sec)*1000 + (stopTime.tv_nsec - startTime.tv_nsec) /1000000;','remaining_ms -= static_cast<Milliseconds>(GetTickCount64()-startTime);').replace(': excep_errno(errno)',': excep_errno(WSAGetLastError())').replace('strerror(excep_errno)','GetDAQWindows::SocketError(excep_errno).c_str()'))
    edit('mdaq/src/mdaq/utl/net/TcpConnection.h',lambda s:s.replace('int a_bsd_socket','SOCKET a_bsd_socket'))
    edit('mdaq/src/mdaq/utl/net/TcpConnection.cpp',lambda s:s.replace('#include <sys/socket.h>','#include <winsock2.h>').replace('int a_bsd_socket','SOCKET a_bsd_socket'))
    edit('mdaq/src/mdaq/utl/net/UdpTransceiver.cpp',lambda s:s.replace('#ifdef VXWORKS\n', '#if defined(VXWORKS) || defined(WIN32)\n'))
    edit('mdaq/src/mdaq/utl/net/TcpAcceptor.cpp',lambda s:s.replace('int bsd_socket_client = ::accept','SOCKET bsd_socket_client = ::accept').replace('if (bsd_socket_client < 0)','if (bsd_socket_client == INVALID_SOCKET)'))
    path='mdaq/src/mdaq/utl/net/IpDetect.cpp'
    original=(vendor/path).read_text(encoding='utf-8')
    changes[path]=original[:original.index('#include "IpDetect.h"')]+'''#include "IpDetect.h"
#include <iphlpapi.h>
#include <vector>
#include <stdexcept>
namespace mdaq { namespace utl { namespace net {
IpDetect::IpDetect() {
    GetDAQWindows::Winsock();
    ULONG length=16384;
    std::vector<unsigned char> storage(length);
    ULONG result=ERROR_BUFFER_OVERFLOW;
    for(int attempt=0;attempt<3 && result==ERROR_BUFFER_OVERFLOW;++attempt){
        storage.resize(length);
        result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES *>(storage.data()),&length);
    }
    if(result!=NO_ERROR)throw std::runtime_error("Cannot enumerate Windows IPv4 adapters: "+std::to_string(result));
    for(auto adapter=reinterpret_cast<IP_ADAPTER_ADDRESSES *>(storage.data());adapter;adapter=adapter->Next)
        for(auto address=adapter->FirstUnicastAddress;address;address=address->Next)
            if(address->Address.lpSockaddr && address->Address.lpSockaddr->sa_family==AF_INET)
                push_back(reinterpret_cast<sockaddr_in *>(address->Address.lpSockaddr)->sin_addr.s_addr);
}
}}}
'''
    output.mkdir(parents=True,exist_ok=True)
    def write(path,data):
        path.parent.mkdir(parents=True,exist_ok=True)
        if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    # Preserve relative ../ header includes in each original source tree.
    for package in ['compound','utilities','mdaq','getbench']:
        for header in (vendor/package/'src').rglob('*'):
            if header.is_file() and header.suffix in ['.h','.hpp']:
                relative=header.relative_to(vendor)
                if relative.as_posix() not in changes:write(output/relative,header.read_bytes())

    for name,text in changes.items():
        target=output/name;target.parent.mkdir(parents=True,exist_ok=True)
        # Retain adjacent unchanged headers for original quoted local includes.
        for header in (vendor/name).parent.iterdir():
            if not header.is_file() or header.suffix not in ['.h','.hpp','.cpp']:continue
            dest=target.parent/header.name
            if header.relative_to(vendor).as_posix() not in changes:write(dest,header.read_bytes())
        write(target,text.encode('utf-8'))
    prepared=[]
    for path in sources:
        try:relative=path.relative_to(vendor)
        except ValueError:prepared.append(path);continue
        candidate=output/relative
        prepared.append(candidate if candidate.is_file() else path)
    return prepared

def entry(text, ecc=False):
    """Make Windows console Ctrl+Break follow the existing graceful exit path."""
    if ecc:
        text=text.replace('static bool askedForShutdown = false;','static volatile std::sig_atomic_t askedForShutdown = 0;')
        return text.replace('signal(SIGINT, sigint_handler);','signal(SIGINT, sigint_handler);\n        signal(SIGBREAK, sigint_handler);')
    return text.replace('std::signal(SIGINT,requestStop);','std::signal(SIGINT,requestStop);std::signal(SIGBREAK,requestStop);')
