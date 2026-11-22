#ifndef GETDAQ_ORIGINAL_WINDOWS_PLATFORM_H
#define GETDAQ_ORIGINAL_WINDOWS_PLATFORM_H
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifdef EV_BREAK
#undef EV_BREAK
#endif
#include <mutex>
#include <stdexcept>
#include <string>
namespace GetDAQWindows {
inline void Winsock(){
    static std::once_flag once;
    std::call_once(once,[]{WSADATA data;const int error=WSAStartup(MAKEWORD(2,2),&data);if(error)throw std::runtime_error("Cannot initialize Winsock: "+std::to_string(error));});
}
inline std::string SocketError(int error){
    char *message=nullptr;
    const DWORD count=FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,reinterpret_cast<char *>(&message),0,nullptr);
    std::string result=count?std::string(message,count):"Winsock error "+std::to_string(error);if(message)LocalFree(message);return result;
}
}
#endif
