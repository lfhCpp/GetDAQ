#ifndef GETDAQ_SCANEXECUTIONJOURNALSTORE_WINDOWS_SUPPORT_P_H
#define GETDAQ_SCANEXECUTIONJOURNALSTORE_WINDOWS_SUPPORT_P_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <vector>

namespace GetDAQ {
namespace ScanExecutionJournalWindows {

class Handle {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : fValue(value) {}
    ~Handle() { if (fValue != INVALID_HANDLE_VALUE) ::CloseHandle(fValue); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&other) noexcept : fValue(other.Release()) {}
    Handle &operator=(Handle &&other) noexcept
    {
        if (this != &other) {
            if (fValue != INVALID_HANDLE_VALUE) ::CloseHandle(fValue);
            fValue = other.Release();
        }
        return *this;
    }
    HANDLE Get() const { return fValue; }
    HANDLE Release() { const HANDLE value = fValue; fValue = INVALID_HANDLE_VALUE; return value; }
private:
    HANDLE fValue;
};

struct Identity {
    DWORD volume;
    DWORD indexHigh;
    DWORD indexLow;
};

Identity FileIdentity(HANDLE handle);
bool Same(const Identity &left,const Identity &right);
std::vector<unsigned char> CurrentUserSid();
void CheckPrivateAcl(HANDLE handle,const std::vector<unsigned char> &userSid);
std::wstring Utf8ToWide(const std::string &value);
std::string WideToUtf8(const std::wstring &value);
std::wstring FullPath(const std::wstring &path);
std::wstring LongPath(const std::wstring &path);
std::wstring FinalPath(HANDLE handle);
bool EqualPath(std::wstring left,std::wstring right);
void CheckRegularFile(HANDLE handle,const char *message);
void WriteAll(HANDLE handle,const std::string &bytes);

} // namespace ScanExecutionJournalWindows
} // namespace GetDAQ

#endif
