#include "iceingress.h"
#include "iceingress_helpers_p.h"

#include <sstream>
#include <unistd.h>

namespace GetDAQ {
std::string IceIngressRecorder::Status() const
{
    std::lock_guard<std::mutex> lock(fErrorMutex);
    std::ostringstream out;
    out << "{\"service\":\"getdaq-ice-receiver\""
        << ",\"input_protocol\":\"ICE\""
        << ",\"storage_mode\":" << IceIngressInternal::Quote(fConfig.storageMode)
        << ",\"pid\":" << ::getpid()
        << ",\"control_protocol\":1"
        << ",\"once\":true"
        << ",\"durable\":true"
        << ",\"file_naming\":" << IceIngressInternal::Quote(fConfig.fileNaming)
        << ",\"data_bind\":" << IceIngressInternal::Quote(fConfig.dataBind)
        << ",\"data_port\":" << fConfig.dataPort
        << ",\"status_port\":" << fConfig.statusPort
        << ",\"sender_id\":" << IceIngressInternal::Quote(fConfig.senderId)
        << ",\"configuration_sha256\":" << IceIngressInternal::Quote(fConfig.configurationSha256)
        << ",\"artifact_sha256\":" << IceIngressInternal::Quote(fConfig.artifactSha256)
        << ",\"connections\":" << fConnections
        << ",\"connected\":" << (fConnections && !fInputClosed ? "true" : "false")
        << ",\"minimum_free_bytes\":" << fConfig.minimumFreeBytes
        << ",\"queued_blocks\":" << fQueue.Pending()
        << ",\"sealed_bytes\":" << fSynced
        << ",\"stream_sealed\":" << (fReceipt ? "true" : "false")
        << ",\"session_id\":" << IceIngressInternal::Quote(fConfig.sessionId)
        << ",\"output_directory\":" << IceIngressInternal::Quote(fConfig.output)
        << ",\"sender_eof_confirmed\":false"
        << ",\"sender_end_confirmed\":" << (fEnd ? "true" : "false")
        << ",\"writer_done\":" << (fWriterDone ? "true" : "false")
        << ",\"completion_receipt_published\":" << (fReceipt ? "true" : "false")
        << ",\"received_bytes\":" << fReceived
        << ",\"accepted_file_bytes\":" << fAccepted
        << ",\"synced_bytes\":" << fSynced
        << ",\"frames\":" << fFrames
        << ",\"files\":" << fFiles
        << ",\"pool_bytes\":" << fConfig.blocks * fConfig.blockBytes
        << ",\"export_clients\":" << fExporter.Clients()
        << ",\"export_disconnections\":" << fExporter.Disconnected()
        << ",\"export_bytes\":" << fExporter.Bytes()
        << ",\"export_failed\":" << (fExporter.Failed() ? "true" : "false")
        << ",\"error\":" << IceIngressInternal::Quote(fError)
        << "}\n";
    return out.str();
}
}
