#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>

#include "../apps/ecc/scanexecutionjournalstore.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

std::vector<unsigned char> UserSid()
{
    HANDLE token = nullptr;
    Require(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) != 0,
            "Cannot inspect test user token");
    DWORD bytes = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    Require(::GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytes > 0,
            "Cannot size test user token");
    std::vector<unsigned char> buffer(bytes);
    Require(::GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes) != 0,
            "Cannot read test user token");
    ::CloseHandle(token);
    const TOKEN_USER *user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
    const DWORD sidSize = ::GetLengthSid(user->User.Sid);
    std::vector<unsigned char> sid(sidSize);
    Require(::CopySid(sidSize, sid.data(), user->User.Sid) != 0,
            "Cannot copy test user SID");
    return sid;
}

void ProtectDirectory(const std::wstring &path)
{
    const auto sid = UserSid();
    EXPLICIT_ACCESSW access = {};
    access.grfAccessPermissions = GENERIC_ALL;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
    ::BuildTrusteeWithSidW(&access.Trustee, const_cast<unsigned char *>(sid.data()));
    PACL dacl = nullptr;
    Require(::SetEntriesInAclW(1, &access, nullptr, &dacl) == ERROR_SUCCESS,
            "Cannot create private test directory ACL");
    const DWORD status = ::SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()),
        SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, dacl, nullptr);
    ::LocalFree(dacl);
    Require(status == ERROR_SUCCESS, "Cannot protect private test directory");
}

std::string Utf8(const std::wstring &value)
{
    const int count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    Require(count > 0, "Cannot size UTF-8 test path");
    std::string result(static_cast<std::size_t>(count), '\0');
    Require(::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), &result[0], count, nullptr, nullptr) == count,
        "Cannot convert test path to UTF-8");
    return result;
}

struct TemporaryDirectory {
    std::wstring path;
    explicit TemporaryDirectory()
    {
        wchar_t temp[MAX_PATH] = {};
        const DWORD length = ::GetTempPathW(MAX_PATH, temp);
        Require(length > 0 && length < MAX_PATH, "Cannot locate Windows temporary directory");
        wchar_t candidate[MAX_PATH] = {};
        Require(::GetTempFileNameW(temp, L"gds", 0, candidate) != 0,
                "Cannot allocate private test directory name");
        Require(::DeleteFileW(candidate) != 0 && ::CreateDirectoryW(candidate, nullptr) != 0,
                "Cannot create scan journal test directory");
        path = candidate;
        ProtectDirectory(path);
    }
    ~TemporaryDirectory()
    {
        WIN32_FIND_DATAW data;
        const std::wstring pattern = path + L"\\*";
        HANDLE search = ::FindFirstFileW(pattern.c_str(), &data);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if (std::wstring(data.cFileName) == L"." ||
                    std::wstring(data.cFileName) == L"..") continue;
                ::DeleteFileW((path + L"\\" + data.cFileName).c_str());
            } while (::FindNextFileW(search, &data));
            ::FindClose(search);
        }
        ::RemoveDirectoryW(path.c_str());
    }
    std::wstring File(const wchar_t *name) const { return path + L"\\" + name; }
};

}

int main()
{
    try {
        TemporaryDirectory directory;
        const std::string journalPath = Utf8(directory.File(L"scan.json"));
        auto store = GetDAQ::ScanExecutionJournalStore::Create(journalPath,
            GetDAQ::ScanExecutionJournal("scan-windows", std::string(64, 'a'), std::string(64, 'f'), 1));
        store->Persist(store->Current().WithTransition(GetDAQ::ScanPointState::Planned,
                                                       GetDAQ::ScanPointState::Reviewed));

        bool secondWriterRejected = false;
        try { GetDAQ::ScanExecutionJournalStore::Open(journalPath); }
        catch (const std::exception &) { secondWriterRejected = true; }
        Require(secondWriterRejected, "Second Windows journal writer was not excluded");

        store->Persist(store->Current().WithOperationIntent(
            "windows-operation-1", "start-ecc", std::string(64, 'b')));
        store.reset();
        auto reopened = GetDAQ::ScanExecutionJournalStore::Open(journalPath);
        Require(reopened->Current().State() == GetDAQ::ScanPointState::NeedsReconciliation &&
                reopened->Current().PendingOperationUnknown(),
                "Windows restart did not quarantine an incomplete operation");
        const auto reconciled = reopened->Current().WithOperationReconciled(
            "windows-operation-1", "operator-1",
            GetDAQ::ScanOperationDisposition::ConfirmedNotSent, std::string(64, 'c'));
        reopened->Persist(reconciled);
        reopened.reset();

        const std::wstring linkedPath = directory.File(L"scan-linked.json");
        Require(::CreateHardLinkW(linkedPath.c_str(), directory.File(L"scan.json").c_str(), nullptr) != 0,
                "Cannot create Windows hard-link rejection fixture");
        bool hardLinkRejected = false;
        try { GetDAQ::ScanExecutionJournalStore::Open(Utf8(linkedPath)); }
        catch (const std::exception &) { hardLinkRejected = true; }
        Require(hardLinkRejected, "Windows journal accepted a multiply linked file");
        ::DeleteFileW(linkedPath.c_str());

        auto tamperStore = GetDAQ::ScanExecutionJournalStore::Open(journalPath);
        HANDLE tamper = ::CreateFileW(directory.File(L"scan.json").c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(tamper != INVALID_HANDLE_VALUE, "Cannot open journal tamper fixture");
        DWORD written = 0;
        const char byte = ' ';
        const BOOL wrote = ::WriteFile(tamper, &byte, 1, &written, nullptr);
        ::CloseHandle(tamper);
        Require(wrote && written == 1, "Cannot mutate journal tamper fixture");
        bool tamperRejected = false;
        try {
            tamperStore->Persist(tamperStore->Current().WithOperationIntent(
                "windows-operation-2", "start-ecc", std::string(64, 'd')));
        } catch (const std::exception &) { tamperRejected = true; }
        Require(tamperRejected, "Windows journal overwrote an external modification");
        tamperStore.reset();
        std::cout << "PASS Windows scan journal private ACL, exclusive writer, restart quarantine, hard-link rejection, and tamper detection\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
