#include "scanexecutionjournalstore_windows_support_p.h"
#include <aclapi.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace GetDAQ {
namespace ScanExecutionJournalWindows {
namespace {

void Require(bool okay, const char *message)
{
    if (!okay) throw std::runtime_error(message);
}

} // namespace

Identity FileIdentity(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION info;
    Require(::GetFileInformationByHandle(handle, &info) != 0,
            "Cannot inspect scan journal file identity");
    return {info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow};
}

bool Same(const Identity &left, const Identity &right)
{
    return left.volume == right.volume && left.indexHigh == right.indexHigh &&
           left.indexLow == right.indexLow;
}

std::vector<unsigned char> CurrentUserSid()
{
    HANDLE rawToken = nullptr;
    Require(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken) != 0,
            "Cannot inspect current Windows user");
    Handle token(rawToken);
    DWORD bytes = 0;
    ::GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &bytes);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0)
        throw std::runtime_error("Cannot size current Windows user identity");
    std::vector<unsigned char> buffer(bytes);
    Require(::GetTokenInformation(token.Get(), TokenUser, buffer.data(), bytes, &bytes) != 0,
            "Cannot read current Windows user identity");
    const TOKEN_USER *user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
    const DWORD sidBytes = ::GetLengthSid(user->User.Sid);
    std::vector<unsigned char> sid(sidBytes);
    Require(::CopySid(sidBytes, sid.data(), user->User.Sid) != 0,
            "Cannot copy current Windows user identity");
    return sid;
}

void CheckPrivateAcl(HANDLE handle, const std::vector<unsigned char> &userSid)
{
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD result = ::GetSecurityInfo(handle, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &dacl, nullptr, &descriptor);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Cannot inspect scan journal access permissions");
    const auto release = [&]() { if (descriptor) ::LocalFree(descriptor); };
    if (!owner || !::EqualSid(owner, const_cast<unsigned char *>(userSid.data())) || !dacl) {
        release();
        throw std::runtime_error("Scan journal path must be owned by the current user with a private ACL");
    }
    for (DWORD index = 0; index < dacl->AceCount; ++index) {
        void *rawAce = nullptr;
        if (!::GetAce(dacl, index, &rawAce)) {
            release();
            throw std::runtime_error("Cannot inspect scan journal access entry");
        }
        const ACE_HEADER *header = static_cast<const ACE_HEADER *>(rawAce);
        if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            const ACCESS_ALLOWED_ACE *ace = static_cast<const ACCESS_ALLOWED_ACE *>(rawAce);
            PSID sid = const_cast<DWORD *>(&ace->SidStart);
            if (!::EqualSid(sid, const_cast<unsigned char *>(userSid.data()))) {
                release();
                throw std::runtime_error("Scan journal ACL grants access to another user");
            }
        } else if (header->AceType != ACCESS_DENIED_ACE_TYPE) {
            release();
            throw std::runtime_error("Scan journal ACL contains an unsupported access entry");
        }
    }
    release();
}

std::wstring Utf8ToWide(const std::string &value)
{
    if (value.empty() || value.find('\0') != std::string::npos)
        throw std::invalid_argument("Scan journal path is empty or contains a NUL byte");
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) throw std::invalid_argument("Scan journal path is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), &result[0], count) != count)
        throw std::invalid_argument("Cannot convert scan journal path to Windows form");
    return result;
}

std::string WideToUtf8(const std::wstring &value)
{
    const int count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) throw std::runtime_error("Cannot encode canonical scan journal path");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), &result[0], count, nullptr, nullptr) != count)
        throw std::runtime_error("Cannot encode canonical scan journal path");
    return result;
}

std::wstring FullPath(const std::wstring &path)
{
    const DWORD needed = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!needed) throw std::runtime_error("Cannot resolve scan journal path");
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1U);
    const DWORD length = ::GetFullPathNameW(path.c_str(),
        static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (!length || length >= buffer.size())
        throw std::runtime_error("Cannot resolve scan journal path");
    return std::wstring(buffer.data(), length);
}

std::wstring LongPath(const std::wstring &path)
{
    const DWORD needed = ::GetLongPathNameW(path.c_str(), nullptr, 0);
    if (!needed) throw std::runtime_error("Cannot resolve long scan journal directory path");
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1U);
    const DWORD length = ::GetLongPathNameW(path.c_str(), buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size())
        throw std::runtime_error("Cannot resolve long scan journal directory path");
    return std::wstring(buffer.data(), length);
}

std::wstring FinalPath(HANDLE handle)
{
    const DWORD needed = ::GetFinalPathNameByHandleW(handle, nullptr, 0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!needed) throw std::runtime_error("Cannot verify canonical scan journal directory");
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1U);
    const DWORD length = ::GetFinalPathNameByHandleW(handle, buffer.data(),
        static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!length || length >= buffer.size())
        throw std::runtime_error("Cannot verify canonical scan journal directory");
    std::wstring result(buffer.data(), length);
    if (result.compare(0, 4, L"\\\\?\\") == 0) result.erase(0, 4);
    return result;
}

bool EqualPath(std::wstring left, std::wstring right)
{
    std::replace(left.begin(), left.end(), L'/', L'\\');
    std::replace(right.begin(), right.end(), L'/', L'\\');
    if (left.size() > 3 && left.back() == L'\\') left.pop_back();
    if (right.size() > 3 && right.back() == L'\\') right.pop_back();
    return ::CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()),
        right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

void CheckRegularFile(HANDLE handle, const char *message)
{
    BY_HANDLE_FILE_INFORMATION info;
    Require(::GetFileType(handle) == FILE_TYPE_DISK &&
            ::GetFileInformationByHandle(handle, &info) != 0 &&
            !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            info.nNumberOfLinks == 1,
            message);
}

void WriteAll(HANDLE handle, const std::string &bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            bytes.size() - offset, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0;
        if (!::WriteFile(handle, bytes.data() + offset, request, &written, nullptr) || !written)
            throw std::runtime_error("Cannot write scan execution journal");
        offset += written;
    }
    Require(::FlushFileBuffers(handle) != 0,
            "Cannot flush scan execution journal temporary file");
}

} // namespace ScanExecutionJournalWindows
} // namespace GetDAQ
