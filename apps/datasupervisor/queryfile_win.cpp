#include "queryfile.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace GetDAQ {
namespace {
std::wstring Wide(const std::string &value) {
    if (value.empty() || value.size()>32760 || value.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid supervisor request-file path");
    const int count=::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if (!count) throw std::invalid_argument("Supervisor request-file path must be UTF-8");
    std::wstring result(std::size_t(count),L'\0');
    if (!::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),&result[0],count))
        throw std::runtime_error("Cannot decode supervisor request-file path");
    return result;
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle() { if(value!=INVALID_HANDLE_VALUE) ::CloseHandle(value); }
};
BY_HANDLE_FILE_INFORMATION Information(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION info={};
    if (!::GetFileInformationByHandle(handle,&info)) throw std::runtime_error("Cannot inspect bounded supervisor request file");
    return info;
}
bool Same(const BY_HANDLE_FILE_INFORMATION &a,const BY_HANDLE_FILE_INFORMATION &b) {
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber && a.nFileIndexHigh==b.nFileIndexHigh && a.nFileIndexLow==b.nFileIndexLow;
}
}
std::string ReadSupervisorQueryFile(const std::string &path,std::size_t maximum) {
    const auto wide=Wide(path);
    Handle file;
    file.value=::CreateFileW(wide.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if (file.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open supervisor request file");
    const auto before=Information(file.value);
    const auto size=(static_cast<unsigned long long>(before.nFileSizeHigh)<<32)|before.nFileSizeLow;
    if ((before.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || before.nNumberOfLinks!=1 || size>maximum)
        throw std::runtime_error("Invalid bounded regular supervisor request file");
    std::string bytes(std::size_t(size),'\0'); std::size_t offset=0;
    while (offset<bytes.size()) {
        DWORD received=0; const DWORD wanted=DWORD(std::min<std::size_t>(bytes.size()-offset,0x7fffffffU));
        if (!::ReadFile(file.value,&bytes[offset],wanted,&received,nullptr) || !received) throw std::runtime_error("Cannot read complete supervisor request file");
        offset+=received;
    }
    char extra=0; DWORD received=0;
    if (!::ReadFile(file.value,&extra,1,&received,nullptr) || received) throw std::runtime_error("Supervisor request file changed while reading");
    const auto after=Information(file.value);
    if (!Same(before,after) || before.nFileSizeHigh!=after.nFileSizeHigh || before.nFileSizeLow!=after.nFileSizeLow ||
        ::CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)!=0)
        throw std::runtime_error("Supervisor request file changed while reading");
    return bytes;
}
}
