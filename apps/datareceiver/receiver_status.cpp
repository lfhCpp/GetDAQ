#include "receiver.h"
#include "receiver_internal.h"
#include "netutil.h"
#include <chrono>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
std::string Receiver::Status()
{
    std::lock_guard<std::mutex> lock(fErrorMutex);
    std::ostringstream out;
    const uint64_t controls = fControlBytes.load();
    const uint64_t wire = fReceived.load();
    const uint64_t received = wire - controls;
    out << "{\"service\":\"getdaq-receiver\",\"protocol\":"
        << ReceiverInternal::QuoteJson(fConfig.storageMode == "byte-storage" ? "raw bytes over plain TCP" :
                 (fConfig.inputProtocol == "FDT" ? "MFM over FDT TCP" : "MFM over plain TCP")) << ",\"storage_mode\":"
        << ReceiverInternal::QuoteJson(fConfig.storageMode) << ",\"control_protocol\":1,\"pid\":" << ::getpid()
        << ",\"session_id\":" << ReceiverInternal::QuoteJson(fConfig.sessionId) << ",\"sender_id\":" << ReceiverInternal::QuoteJson(fConfig.senderId)
        << ",\"configuration_sha256\":" << ReceiverInternal::QuoteJson(fConfig.configurationSha256) << ",\"artifact_sha256\":" << ReceiverInternal::QuoteJson(fConfig.artifactSha256)
        << ",\"output_directory\":" << ReceiverInternal::QuoteJson(fConfig.output) << ",\"data_bind\":" << ReceiverInternal::QuoteJson(fConfig.bind)
        << ",\"data_port\":" << fConfig.port << ",\"status_port\":" << fConfig.statusPort << ",\"once\":" << (fConfig.once ? "true" : "false")
        << ",\"sender_eof_confirmed\":" << (fConnections>0 && fEofs==fConnections ? "true" : "false")
        << ",\"connected\":" << (fConnected ? "true" : "false")
        << ",\"received_bytes\":" << received << ",\"accepted_file_bytes\":" << fAccepted << ",\"sealed_bytes\":" << fSynced
        << ",\"frames\":" << fFrames << ",\"acquisition_event_count_available\":" << (fConfig.storageMode == "frame-storage" ? "true" : "false")
        << ",\"acquisition_events\":" << fAcquisitionEvents << ",\"files\":" << fFiles << ",\"connections\":" << fConnections
        << ",\"minimum_free_bytes\":" << fConfig.minimumFreeBytes
        << ",\"queued_blocks\":" << fQueue.Pending() << ",\"pool_bytes\":" << fConfig.blocks * fConfig.blockBytes
        << ",\"durable\":" << (fConfig.durable ? "true" : "false")
        << ",\"stream_sealed\":" << (fError.empty() && !fConnected && fSealed && fQueue.Pending() == 0 && fSynced == received &&
            (!fConfig.cycleControlStdin || (fCycleFinish && !fCycleControlLost && !fCycleUnused)) ? "true" : "false")
        << ",\"archive_requires_exit\":true"
        << ",\"export_clients\":" << fExporter.Clients() << ",\"export_disconnections\":" << fExporter.Disconnected()
        << ",\"export_bytes\":" << fExporter.Bytes() << ",\"export_failed\":" << (fExporter.Failed() ? "true" : "false")
        << ",\"error\":" << ReceiverInternal::QuoteJson(fError);
    if (fConfig.inputProtocol == "FDT")
        out << ",\"wire_received_bytes\":" << wire << ",\"fdt_control_bytes\":" << controls
            << ",\"fdt_heartbeats\":" << fHeartbeats << ",\"fdt_goodbyes\":" << fGoodbyes;
    if (fConfig.fileNaming != "native") out << ",\"file_naming\":" << ReceiverInternal::QuoteJson(fConfig.fileNaming);
    if (fConfig.inputProtocol != "TCP") out << ",\"input_protocol\":" << ReceiverInternal::QuoteJson(fConfig.inputProtocol);
    if (fConfig.cycleControlStdin) out << ",\"cycle_control\":\"stdin-socket-v1\",\"cycle_finish_requested\":" << (fCycleFinish ? "true" : "false")
        << ",\"cycle_control_connected\":" << (!fCycleControlLost ? "true" : "false")
        << ",\"cycle_startup_aborted\":" << (fCycleAborted ? "true" : "false") << ",\"cycle_abort_refused\":" << (fCycleAbortRefused ? "true" : "false")
        << ",\"cycle_unused_refused\":" << (fCycleUnusedRefused ? "true" : "false")
        << ",\"source_unused_confirmed\":" << (fCycleUnused && fWriterDone && fError.empty() && !fConnections && !wire && !fFrames && !fFiles ? "true" : "false");
    out << "}\n";
    return out.str();
}

void Receiver::ServeStatus()
{
    if (fStatus < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); return; }
    if (!Readable(fStatus, 50)) return;
    const int fd = ::accept(fStatus, nullptr, nullptr);
    if (fd < 0) return;
    try {
        Nonblock(fd);
        ConfigureNoSigpipe(fd);
        std::string request;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (request.size() <= 4096 && request.find("\r\n\r\n") == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            if (!Readable(fd, 50)) continue;
            char buffer[1024]; const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            request.append(buffer, std::size_t(n));
        }
        const bool valid = request.size() <= 4096 && request.find("\r\n\r\n") != std::string::npos &&
            (request.find("GET /status HTTP/1.1\r\n") == 0 || request.find("GET /status HTTP/1.0\r\n") == 0);
        const std::string body = valid ? Status() : "{\"error\":\"GET /status only\"}\n";
        const std::string response = std::string(valid ? "HTTP/1.1 200 OK" : "HTTP/1.1 400 Bad Request") +
            "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        std::size_t offset = 0;
        while (offset < response.size() && std::chrono::steady_clock::now() < deadline) {
            const ssize_t n = SendNoSigpipe(fd, response.data() + offset, response.size() - offset);
            if (n > 0) offset += std::size_t(n);
            else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                pollfd p = {fd, POLLOUT, 0}; ::poll(&p, 1, 20);
            } else break;
        }
    } catch (const std::exception &e) { std::cerr << "Status client: " << e.what() << '\n'; }
    ::close(fd);
}
}
