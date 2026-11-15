#include "scanexecutionjournalstore_windows_io_p.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>

namespace GetDAQ {
namespace ScanExecutionJournalWindows {
namespace {

std::atomic<unsigned long> TemporarySequence(0);

void Require(bool okay, const char *message)
{
    if (!okay) throw std::runtime_error(message);
}

std::wstring TemporaryName(const std::wstring &directoryPath)
{
    return directoryPath + L"\\.scan-journal-" +
           std::to_wstring(::GetCurrentProcessId()) + L"-" +
           std::to_wstring(TemporarySequence.fetch_add(1)) + L".tmp";
}

void WriteTemporary(const std::wstring &path,
                    const std::vector<unsigned char> &userSid,
                    const std::string &bytes)
{
    Handle file(::CreateFileW(path.c_str(),
        GENERIC_WRITE | FILE_READ_ATTRIBUTES | READ_CONTROL,
        0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create private scan journal temporary file");
    CheckRegularFile(file.Get(), "Scan journal temporary path is not a regular file");
    CheckPrivateAcl(file.Get(), userSid);
    const Identity identity = FileIdentity(file.Get());
    WriteAll(file.Get(), bytes);
    LARGE_INTEGER size;
    Require(::GetFileSizeEx(file.Get(), &size) &&
            size.QuadPart == static_cast<LONGLONG>(bytes.size()) &&
            Same(identity, FileIdentity(file.Get())),
            "Scan journal temporary file changed before publication");
}

} // namespace

std::string ReadJournalFile(const std::wstring &directoryPath,
                            const std::wstring &leaf,
                            const std::vector<unsigned char> &userSid,
                            Identity *identity,
                            std::size_t maximumBytes)
{
    const std::wstring path = directoryPath + L"\\" + leaf;
    Handle file(::CreateFileW(path.c_str(), GENERIC_READ | READ_CONTROL,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT |
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open scan journal without following reparse points");
    CheckRegularFile(file.Get(), "Scan journal is not a regular local file");
    CheckPrivateAcl(file.Get(), userSid);
    const Identity before = FileIdentity(file.Get());
    LARGE_INTEGER size;
    Require(::GetFileSizeEx(file.Get(), &size) && size.QuadPart >= 1 &&
            size.QuadPart <= static_cast<LONGLONG>(maximumBytes),
            "Scan journal file size is invalid");
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            bytes.size() - offset, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD read = 0;
        if (!::ReadFile(file.Get(), &bytes[offset], request, &read, nullptr) || !read)
            throw std::runtime_error("Cannot read complete scan execution journal");
        offset += read;
    }
    LARGE_INTEGER afterSize;
    Require(Same(before, FileIdentity(file.Get())) &&
            ::GetFileSizeEx(file.Get(), &afterSize) && afterSize.QuadPart == size.QuadPart,
            "Scan journal changed while being read");
    *identity = before;
    return bytes;
}

void PublishNewJournalFile(const std::wstring &directoryPath,
                           const std::wstring &leaf,
                           const std::vector<unsigned char> &userSid,
                           const std::string &bytes)
{
    const std::wstring temporary = TemporaryName(directoryPath);
    try {
        WriteTemporary(temporary, userSid, bytes);
        if (!::MoveFileExW(temporary.c_str(),
            (directoryPath + L"\\" + leaf).c_str(), MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish new scan journal without replacing existing data");
    } catch (...) { ::DeleteFileW(temporary.c_str()); throw; }
}

void ReplaceJournalFile(const std::wstring &directoryPath,
                        const std::wstring &leaf,
                        const std::vector<unsigned char> &userSid,
                        const Identity &expectedIdentity,
                        const std::string &bytes)
{
    const std::wstring temporary = TemporaryName(directoryPath);
    try {
        WriteTemporary(temporary, userSid, bytes);
        const std::wstring target = directoryPath + L"\\" + leaf;
        Handle named(::CreateFileW(target.c_str(), FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        Require(named.Get() != INVALID_HANDLE_VALUE,
                "Scan journal disappeared before atomic replacement");
        CheckRegularFile(named.Get(), "Scan journal changed before atomic replacement");
        CheckPrivateAcl(named.Get(), userSid);
        Require(Same(FileIdentity(named.Get()), expectedIdentity),
                "Scan journal changed before atomic replacement");
        if (!::ReplaceFileW(target.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr))
            throw std::runtime_error("Cannot atomically replace scan journal (Win32 error " +
                std::to_string(::GetLastError()) + ")");
    } catch (...) { ::DeleteFileW(temporary.c_str()); throw; }
}

} // namespace ScanExecutionJournalWindows
} // namespace GetDAQ
