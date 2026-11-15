#include "scanexecutionjournalstore.h"

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string Utf8(const std::wstring &value)
{
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("Invalid Unicode command line");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), &result[0], size, nullptr, nullptr) != size)
        throw std::runtime_error("Cannot encode command line as UTF-8");
    return result;
}

std::string Quote(const std::string &value)
{
    static const char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 0x20) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}

}

int main()
{
    int argc = 0;
    LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) { std::cerr << "Cannot read Unicode command line\n"; return 2; }
    int result = 0;
    try {
        if (argc == 2 && std::wstring(argv[1]) == L"--help") {
            std::cout << "Usage: getdaq-scan-journal inspect FILE\n"
                         "Offline scan journal summary; no hardware connection.\n";
        } else if (argc == 3 && std::wstring(argv[1]) == L"inspect") {
            const std::string path = Utf8(argv[2]);
            const auto store = GetDAQ::ScanExecutionJournalStore::Open(path);
            const auto journal = store->Current();
            std::cout << "{\"schema_version\":1,\"kind\":\"getdaq.scan-execution-status\",\"execution_id\":"
                << Quote(journal.ExecutionId()) << ",\"plan_sha256\":"
                << Quote(journal.SourcePlanSha256()) << ",\"snapshot_sha256\":"
                << Quote(journal.SourceSnapshotSha256()) << ",\"runtime_identity_sha256\":"
                << Quote(journal.RuntimeIdentitySha256()) << ",\"point_count\":"
                << journal.PointCount() << ",\"point_index\":" << journal.PointIndex()
                << ",\"state\":" << Quote(GetDAQ::ScanPointStateName(journal.State()))
                << ",\"event_count\":" << journal.EventCount()
                << ",\"has_pending_operation\":"
                << (journal.HasPendingOperation() ? "true" : "false")
                << ",\"pending_operation_unknown\":"
                << (journal.PendingOperationUnknown() ? "true" : "false")
                << ",\"pending_operation_id\":" << Quote(journal.PendingOperationId())
                << "}\n";
        } else {
            std::cerr << "Usage: getdaq-scan-journal inspect FILE\n";
            result = 2;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    ::LocalFree(argv);
    return result;
}
