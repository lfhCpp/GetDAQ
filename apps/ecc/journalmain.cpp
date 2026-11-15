#include "eccjournal.h"
#include "scanexecutionjournalstore.h"
#include <iostream>
#include <stdexcept>

namespace {
// 输出交给本地查看工具；控制字节和引号必须转义。
std::string Quote(const std::string &text) {
    static const char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}
void Operation(const GetDAQ::EccOperation &op) {
    std::cout << "{\"id\":" << Quote(op.id) << ",\"phase\":" << Quote(op.phase) << ",\"command\":" << op.command
        << ",\"from\":" << static_cast<int>(op.from) << ",\"to\":" << static_cast<int>(op.to)
        << ",\"request_key\":" << Quote(op.requestKey) << ",\"evidence\":" << Quote(op.evidence) << ",\"message\":" << Quote(op.message) << '}';
}
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: getdaq-ecc-journal inspect FILE SCOPE [--operation ID]\n"
            << "       getdaq-ecc-journal inspect-scan FILE\n"
            << "Offline inspection; no hardware connection. ECC inspect requires exact target/deployment scope.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "inspect-scan") {
        try {
            auto store = GetDAQ::ScanExecutionJournalStore::Open(argv[2]);
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
            return 0;
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    if ((argc != 4 && argc != 6) || std::string(argv[1]) != "inspect" ||
        (argc == 6 && std::string(argv[4]) != "--operation")) {
        std::cerr << "Usage: getdaq-ecc-journal inspect FILE SCOPE [--operation ID]\n"
            << "       getdaq-ecc-journal inspect-scan FILE\n"; return 2;
    }
    try {
        GetDAQ::EccJournal journal(argv[2], argv[3], GetDAQ::EccJournalMode::ReadOnly);
        if (argc == 6) { Operation(journal.Operation(argv[5])); std::cout << '\n'; }
        else {
            const auto status = journal.Load();
            std::cout << "{\"schema_version\":1,\"scope\":" << Quote(argv[3]) << ",\"state\":" << static_cast<int>(status.state)
                << ",\"transition\":" << status.transition << ",\"error\":" << status.error
                << ",\"outcome_unknown\":" << (status.outcomeUnknown ? "true" : "false")
                << ",\"needs_reconciliation\":" << (status.needsReconciliation ? "true" : "false")
                << ",\"operation_id\":" << Quote(status.operationId) << ",\"message\":" << Quote(status.message) << ",\"operations\":[";
            const auto operations = journal.Operations();
            for (std::size_t i = 0; i < operations.size(); ++i) { if (i) std::cout << ','; Operation(operations[i]); }
            std::cout << "]}\n";
        }
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
