#include "eccserviceprocesssupervisor.h"
#include "configurationsha256.h"
#include "eccprotocol.h"
#include "scanexecutionjournal.h"
#include "scanexecutionjournalstore.h"
#include "scanmanagedserviceretirement.h"
#include "scanmanagedservicestart.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace GetDAQ;

void Check(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

template <typename Action>
void Rejects(Action action, const char *message)
{
    try { action(); }
    catch (const std::exception &) { return; }
    throw std::runtime_error(message);
}

std::string ReplaceOnce(std::string bytes, const std::string &from,
                        const std::string &to)
{
    const std::size_t position = bytes.find(from);
    Check(position != std::string::npos, "journal mutation target missing");
    bytes.replace(position, from.size(), to);
    return bytes;
}

ScanExecutionJournal BoundJournal(const std::string &executionId,
                                 const std::string &snapshotSha,
                                 const std::string &runtimeIdentitySha,
                                 const std::string &instanceId,
                                 const std::string &serviceSnapshotSha)
{
    ScanExecutionJournal journal(executionId, ConfigurationSha256("source-plan"),
                                 snapshotSha, 2);
    journal = journal.WithTransition(ScanPointState::Planned, ScanPointState::Reviewed);
    const struct Step { ScanPointState from, to; const char *operation; } steps[] = {
        {ScanPointState::Reviewed, ScanPointState::EccStarted, "start-ecc"},
        {ScanPointState::EccStarted, ScanPointState::Described, "describe"},
        {ScanPointState::Described, ScanPointState::Prepared, "prepare"},
        {ScanPointState::Prepared, ScanPointState::Configured, "configure"},
        {ScanPointState::Configured, ScanPointState::RunStarted, "run-start"},
        {ScanPointState::RunStarted, ScanPointState::RunStopped, "run-stop"},
        {ScanPointState::RunStopped, ScanPointState::ReceiverFinished, "receiver-finish"},
        {ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified, "catalog-verify"},
        {ScanPointState::CatalogVerified, ScanPointState::Bound, "bind"}
    };
    unsigned int sequence = 0;
    ScanEventCountBaseline baseline;
    baseline.instanceId = instanceId;
    baseline.snapshotSha256 = serviceSnapshotSha;
    baseline.dataBindingSha256 = ConfigurationSha256("data-binding");
    baseline.agentId = "agent-1";
    baseline.planSha256 = ConfigurationSha256("receiver-plan");
    baseline.receiverInstanceId = ConfigurationSha256("receiver-instance");
    baseline.sourceId = "source-1";
    baseline.sessionId = ConfigurationSha256("receiver-session");
    baseline.pid = 1234;
    baseline.available = true;
    baseline.acquisitionEvents = 0;
    baseline.observedAtUnixMs = 1000;
    for (const Step &step : steps) {
        if (std::string(step.operation) == "run-start")
            journal = journal.WithEventCountBaseline(baseline);
        const std::string operationId = "op-" + std::to_string(sequence++);
        journal = journal.WithOperationIntent(operationId, step.operation,
                                               ConfigurationSha256(operationId));
        journal = journal.WithOperationComplete(operationId,
                                                 ConfigurationSha256(step.operation), step.to);
        if (std::string(step.operation) == "start-ecc")
            journal = journal.WithRuntimeIdentityBinding(runtimeIdentitySha);
    }
    return journal;
}

void PersistBoundJournal(ScanExecutionJournalStore &store,
                         const std::string &runtimeIdentitySha,
                         const std::string &instanceId,
                         const std::string &serviceSnapshotSha)
{
    store.Persist(store.Current().WithTransition(ScanPointState::Planned,
                                                  ScanPointState::Reviewed));
    ScanEventCountBaseline baseline;
    baseline.instanceId = instanceId;
    baseline.snapshotSha256 = serviceSnapshotSha;
    baseline.dataBindingSha256 = ConfigurationSha256("data-binding");
    baseline.agentId = "agent-1";
    baseline.planSha256 = ConfigurationSha256("receiver-plan");
    baseline.receiverInstanceId = ConfigurationSha256("receiver-instance");
    baseline.sourceId = "source-1";
    baseline.sessionId = ConfigurationSha256("receiver-session");
    baseline.pid = 1234;
    baseline.available = true;
    baseline.observedAtUnixMs = 1000;
    const struct Step { ScanPointState from, to; const char *operation; } steps[] = {
        {ScanPointState::Reviewed, ScanPointState::EccStarted, "start-ecc"},
        {ScanPointState::EccStarted, ScanPointState::Described, "describe"},
        {ScanPointState::Described, ScanPointState::Prepared, "prepare"},
        {ScanPointState::Prepared, ScanPointState::Configured, "configure"},
        {ScanPointState::Configured, ScanPointState::RunStarted, "run-start"},
        {ScanPointState::RunStarted, ScanPointState::RunStopped, "run-stop"},
        {ScanPointState::RunStopped, ScanPointState::ReceiverFinished, "receiver-finish"},
        {ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified, "catalog-verify"},
        {ScanPointState::CatalogVerified, ScanPointState::Bound, "bind"}
    };
    unsigned int sequence = 0;
    for (const Step &step : steps) {
        if (std::string(step.operation) == "run-start")
            store.Persist(store.Current().WithEventCountBaseline(baseline));
        const std::string operationId = "persist-op-" + std::to_string(sequence++);
        store.Persist(store.Current().WithOperationIntent(
            operationId, step.operation, ConfigurationSha256(operationId)));
        store.Persist(store.Current().WithOperationComplete(
            operationId, ConfigurationSha256(step.operation), step.to));
        if (std::string(step.operation) == "start-ecc")
            store.Persist(store.Current().WithRuntimeIdentityBinding(runtimeIdentitySha));
    }
}

void WriteFile(const std::string &path, const std::string &bytes, mode_t mode)
{
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    Check(fd >= 0, "cannot create test file");
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        Check(count > 0, "cannot write test file");
        offset += static_cast<std::size_t>(count);
    }
    Check(::close(fd) == 0, "cannot close test file");
}

std::string ReadRequest(int fd)
{
    std::string bytes;
    char buffer[1024];
    std::size_t expected = std::string::npos;
    for (;;) {
        const ssize_t count = ::recv(fd, buffer, sizeof(buffer), 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        bytes.append(buffer, static_cast<std::size_t>(count));
        const auto split = bytes.find("\r\n\r\n");
        if (split != std::string::npos && expected == std::string::npos) {
            const auto header = bytes.find("Content-Length: ");
            Check(header != std::string::npos && header < split, "missing content length");
            const auto begin = header + std::strlen("Content-Length: ");
            const auto end = bytes.find("\r\n", begin);
            const auto length = static_cast<std::size_t>(std::stoul(bytes.substr(begin, end - begin)));
            expected = split + 4 + length;
        }
        if (expected != std::string::npos && bytes.size() >= expected) break;
        Check(bytes.size() < 8192, "request too large");
    }
    return bytes;
}

void Run()
{
    char temporary[] = "/tmp/getdaq-service-supervisor-XXXXXX";
    char *created = ::mkdtemp(temporary);
    Check(created != nullptr, "cannot create temporary directory");
    const std::string root(created);
    const std::string executable = root + "/ecc-child";
    const std::string configuration = root + "/service.json";
    const std::string credential = root + "/credential";
    const std::string marker = root + "/shutdown-accepted";
    const std::string instance(64, 'a'), snapshot(64, 'b'), operation("handoff-test-1");
    const std::string token(64, 'c');
    const std::string configBytes = "{\"fixed\":true}\n";
    WriteFile(configuration, configBytes, 0600);
    WriteFile(credential, token + "\n", 0600);
    WriteFile(executable, "#!/bin/sh\nwhile [ ! -f \"$GETDAQ_TEST_SHUTDOWN_MARKER\" ]; do sleep 0.01; done\nexit 0\n", 0700);
    Check(::setenv("GETDAQ_TEST_SHUTDOWN_MARKER", marker.c_str(), 1) == 0, "cannot set test marker");

    const int listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    Check(listener >= 0, "cannot create test listener");
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    Check(::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
          ::listen(listener, 1) == 0, "cannot bind test listener");
    socklen_t addressBytes = sizeof(address);
    Check(::getsockname(listener, reinterpret_cast<sockaddr *>(&address), &addressBytes) == 0,
          "cannot inspect test listener");
    const unsigned int port = ntohs(address.sin_port);

    const pid_t server = ::fork();
    Check(server >= 0, "cannot fork fake ECC endpoint");
    if (server == 0) {
        const int client = ::accept(listener, nullptr, nullptr);
        if (client < 0) _exit(10);
        const std::string request = ReadRequest(client);
        const std::string body = "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown\",\"instance_id\":" +
            EncodeJsonString(instance) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshot) +
            ",\"handoff_operation_id\":" + EncodeJsonString(operation) + "}\n";
        const std::string prefix = "POST /v1/shutdown HTTP/1.1\r\n";
        const std::string auth = "Authorization: Bearer " + token + "\r\n";
        const bool valid = request.compare(0, prefix.size(), prefix) == 0 &&
            request.find(auth) != std::string::npos &&
            request.substr(request.find("\r\n\r\n") + 4) == body;
        const std::string accepted = "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown-accepted\",\"instance_id\":" +
            EncodeJsonString(instance) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshot) +
            ",\"handoff_operation_id\":" + EncodeJsonString(operation) + "}\n";
        const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " +
            std::to_string(accepted.size()) + "\r\nConnection: close\r\n\r\n" + accepted;
        const ssize_t sent = ::send(client, response.data(), response.size(), MSG_NOSIGNAL);
        ::close(client);
        ::close(listener);
        if (sent != static_cast<ssize_t>(response.size()) || !valid) _exit(11);
        const int markerFd = ::open(marker.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (markerFd < 0) _exit(12);
        ::close(markerFd);
        _exit(0);
    }
    ::close(listener);

    EccServiceProcessSupervisor supervisor;
    supervisor.Start(executable, configuration);
    const int childPid = supervisor.ProcessId();
    Check(childPid > 0 && supervisor.Running(), "managed ECC process did not start");
    const std::string executionId = "scan-001";
    const std::string executionSnapshotSha = ConfigurationSha256("scan-snapshot");
    const std::string runtimeIdentitySha = ConfigurationSha256("runtime-identity");
    const std::string retirementOperationId = "retire-001";
    const std::string journalPath = root + "/scan.json";
    std::unique_ptr<ScanExecutionJournalStore> store = ScanExecutionJournalStore::Create(
        journalPath, ScanExecutionJournal(executionId, ConfigurationSha256("source-plan"),
                                          executionSnapshotSha, 2));
    PersistBoundJournal(*store, runtimeIdentitySha, instance, snapshot);
    const std::string serviceConfigurationSha = ConfigurationSha256(configBytes);
    const auto evidence = RetireManagedEccServiceForScanPoint(
        *store, supervisor, "127.0.0.1", port, credential,
        serviceConfigurationSha, instance, snapshot, operation,
        retirementOperationId, 5000);
    Check(store->Current().State() == ScanPointState::Retired,
          "accepted shutdown and normal exit did not retire the bound scan point");
    const ScanExecutionJournal retired = store->Current();
    const std::size_t retiredEventCount = retired.EventCount();
    const auto uncompletedIntent = BoundJournal(executionId, executionSnapshotSha,
        runtimeIdentitySha, instance, snapshot).WithManagedServiceShutdownIntent(
            retirementOperationId, childPid, serviceConfigurationSha,
            instance, snapshot, operation);
    const ScanExecutionJournal recoveredIntent = ScanExecutionJournal::Decode(
        uncompletedIntent.Encode());
    Check(recoveredIntent.State() == ScanPointState::NeedsReconciliation,
          "restart did not quarantine a pending managed service shutdown intent");
    Check(evidence.ProcessId() == childPid && evidence.ExitCode() == 0,
          "exit evidence does not identify the managed normal exit");
    Check(evidence.ServiceConfigurationSha256() == ConfigurationSha256(configBytes),
          "exit evidence lost the frozen service configuration digest");
    Check(evidence.InstanceId() == instance && evidence.ServiceSnapshotSha256() == snapshot &&
          evidence.HandoffOperationId() == operation &&
          evidence.RetirementOperationId() == retirementOperationId,
          "exit evidence identity mismatch");
    Check(evidence.ShutdownRequestSha256() == ConfigurationSha256(
          "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown\",\"instance_id\":" +
          EncodeJsonString(instance) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshot) +
          ",\"handoff_operation_id\":" + EncodeJsonString(operation) + "}\n"),
          "shutdown request digest mismatch");
    Check(evidence.ShutdownResponseSha256() == ConfigurationSha256(
          "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown-accepted\",\"instance_id\":" +
          EncodeJsonString(instance) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshot) +
          ",\"handoff_operation_id\":" + EncodeJsonString(operation) + "}\n"),
          "shutdown response digest mismatch");
    Check(evidence.ExecutionId() == executionId &&
          evidence.ExecutionSnapshotSha256() == executionSnapshotSha &&
          evidence.PointIndex() == 0 && evidence.RuntimeIdentitySha256() == runtimeIdentitySha,
          "exit evidence is not bound to the selected scan point identity");

    Rejects([&] {
        ScanExecutionJournal::Decode(ReplaceOnce(
            retired.Encode(), evidence.ShutdownResponseSha256(), std::string(64, '0')));
    }, "journal accepted a shutdown response digest inconsistent with its exact identity");
    store.reset();
    store = ScanExecutionJournalStore::Open(journalPath);
    const ScanExecutionJournal replayed = store->Current();
    Check(replayed.State() == ScanPointState::Retired &&
          replayed.EventCount() == retiredEventCount,
          "managed process retirement did not survive journal replay");
    Check(replayed.WithNextPoint().PointIndex() == 1,
          "evidence-retired point could not advance to its next scan point");
    store.reset();
    Rejects([&] {
        BoundJournal("other-scan", executionSnapshotSha, runtimeIdentitySha,
                     instance, snapshot)
            .WithManagedServiceShutdownIntent(retirementOperationId, evidence.ProcessId(),
                evidence.ServiceConfigurationSha256(), instance, snapshot, operation)
            .WithManagedServiceRetirement(evidence);
    }, "retirement evidence was accepted for a different execution ID");
    Rejects([&] {
        BoundJournal(executionId, executionSnapshotSha,
                     ConfigurationSha256("other-runtime"), instance, snapshot)
            .WithManagedServiceShutdownIntent(retirementOperationId, evidence.ProcessId(),
                evidence.ServiceConfigurationSha256(), instance, snapshot, operation)
            .WithManagedServiceRetirement(evidence);
    }, "retirement evidence was accepted for a different runtime identity");

    int serverStatus = 0;
    Check(::waitpid(server, &serverStatus, 0) == server && WIFEXITED(serverStatus) &&
          WEXITSTATUS(serverStatus) == 0, "fake ECC endpoint rejected the shutdown request");
    ::unlink(journalPath.c_str()); ::unlink((journalPath + ".lock").c_str());
    ::unlink(marker.c_str()); ::unlink(credential.c_str()); ::unlink(configuration.c_str());
    ::unlink(executable.c_str()); ::rmdir(root.c_str());
}

void RunManagedStart()
{
    char temporary[] = "/tmp/getdaq-managed-start-XXXXXX";
    char *created = ::mkdtemp(temporary);
    Check(created != nullptr, "cannot create managed start test directory");
    const std::string root(created);
    const std::string executable = root + "/ecc-child";
    const std::string configuration = root + "/service.json";
    const std::string journalPath = root + "/scan.json";
    const std::string executableBytes = "#!/bin/sh\nexec sleep 30\n";
    const std::string configurationBytes = "{\"fixed\":true}\n";
    WriteFile(executable, executableBytes, 0700);
    WriteFile(configuration, configurationBytes, 0600);
    const std::string executableSha = ConfigurationSha256(executableBytes);
    const std::string configurationSha = ConfigurationSha256(configurationBytes);
    const std::string snapshotSha = ConfigurationSha256("frozen-snapshot");
    const std::string executionId = "managed-start-test";
    const std::string operationId = "start-point-0";
    std::unique_ptr<ScanExecutionJournalStore> store = ScanExecutionJournalStore::Create(
        journalPath, ScanExecutionJournal(executionId, ConfigurationSha256("plan"),
                                          snapshotSha, 1));
    store->Persist(store->Current().WithTransition(ScanPointState::Planned,
                                                   ScanPointState::Reviewed));

    EccServiceProcessSupervisor supervisor;
    const EccServiceProcessStartEvidence evidence = StartManagedEccServiceForScanPoint(
        *store, supervisor, executable, configuration, executableSha,
        configurationSha, operationId);
    Check(evidence.ProcessId() > 0 && evidence.OperationId() == operationId &&
          evidence.ExecutableSha256() == executableSha &&
          evidence.ServiceConfigurationSha256() == configurationSha &&
          evidence.ExecutionId() == executionId &&
          evidence.ExecutionSnapshotSha256() == snapshotSha && evidence.PointIndex() == 0,
          "managed start evidence lost its operation/deployment/point binding");
    Check(store->Current().State() == ScanPointState::EccStarted && supervisor.Running(),
          "managed start did not persist ECCStarted after observing the child");
    const ScanExecutionJournal started = store->Current();
    Check(ScanExecutionJournal::Decode(started.Encode()).State() == ScanPointState::EccStarted,
          "managed ECC startup did not survive journal replay");
    ScanExecutionJournal bound = started.WithRuntimeIdentityBinding(
        ConfigurationSha256("managed-start-runtime"));
    const struct Step { ScanPointState from, to; const char *operation; } steps[] = {
        {ScanPointState::EccStarted, ScanPointState::Described, "describe"},
        {ScanPointState::Described, ScanPointState::Prepared, "prepare"},
        {ScanPointState::Prepared, ScanPointState::Configured, "configure"},
        {ScanPointState::Configured, ScanPointState::RunStarted, "run-start"},
        {ScanPointState::RunStarted, ScanPointState::RunStopped, "run-stop"},
        {ScanPointState::RunStopped, ScanPointState::ReceiverFinished, "receiver-finish"},
        {ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified, "catalog-verify"},
        {ScanPointState::CatalogVerified, ScanPointState::Bound, "bind"}
    };
    ScanEventCountBaseline baseline;
    baseline.instanceId = ConfigurationSha256("instance");
    baseline.snapshotSha256 = ConfigurationSha256("service-snapshot");
    baseline.dataBindingSha256 = ConfigurationSha256("data-binding");
    baseline.agentId = "agent-1"; baseline.planSha256 = ConfigurationSha256("receiver-plan");
    baseline.receiverInstanceId = ConfigurationSha256("receiver-instance");
    baseline.sourceId = "source-1"; baseline.sessionId = ConfigurationSha256("session");
    baseline.pid = 321; baseline.available = true;
    baseline.observedAtUnixMs = 1000;
    unsigned int sequence = 0;
    for (const Step &step : steps) {
        if (std::string(step.operation) == "run-start")
            bound = bound.WithEventCountBaseline(baseline);
        const std::string op = "managed-op-" + std::to_string(sequence++);
        bound = bound.WithOperationIntent(op, step.operation, ConfigurationSha256(op));
        bound = bound.WithOperationComplete(op, ConfigurationSha256(step.operation), step.to);
    }
    const auto shutdownIntent = bound.WithManagedServiceShutdownIntent(
        "managed-retire", evidence.ProcessId(), configurationSha,
        baseline.instanceId, baseline.snapshotSha256, "handoff-1");
    Check(ScanExecutionJournal::Decode(shutdownIntent.Encode()).State() ==
          ScanPointState::NeedsReconciliation,
          "schema-8 managed startup did not preserve shutdown intent quarantine");
    Rejects([&] {
        ScanExecutionJournal::Decode(ReplaceOnce(started.Encode(), executableSha,
                                                 std::string(64, '0')));
    }, "journal accepted a managed ECC executable differing from its durable intent");

    const ScanExecutionJournal pending = ScanExecutionJournal(
        "pending-start", ConfigurationSha256("plan"), snapshotSha, 1)
        .WithTransition(ScanPointState::Planned, ScanPointState::Reviewed)
        .WithManagedServiceStartIntent("pending-op", executableSha, configurationSha);
    Check(ScanExecutionJournal::Decode(pending.Encode()).State() ==
          ScanPointState::NeedsReconciliation,
          "restart did not quarantine an incomplete managed ECC startup intent");

    std::unique_ptr<ScanExecutionJournalStore> failedStore = ScanExecutionJournalStore::Create(
        root + "/failed-scan.json", ScanExecutionJournal(
            "failed-start", ConfigurationSha256("plan"), snapshotSha, 1));
    failedStore->Persist(failedStore->Current().WithTransition(
        ScanPointState::Planned, ScanPointState::Reviewed));
    EccServiceProcessSupervisor unusedSupervisor;
    Rejects([&] {
        (void)StartManagedEccServiceForScanPoint(*failedStore, unusedSupervisor,
            executable, configuration, std::string(64, '0'), configurationSha,
            "mismatch-op");
    }, "managed ECC startup accepted changed deployment inputs");
    Check(unusedSupervisor.ProcessId() == 0 &&
          failedStore->Current().State() == ScanPointState::NeedsReconciliation,
          "preflight mismatch launched ECC or failed to quarantine its durable intent");

    Check(::kill(evidence.ProcessId(), SIGTERM) == 0,
          "cannot stop synthetic managed ECC child");
    int childStatus = 0;
    Check(::waitpid(evidence.ProcessId(), &childStatus, 0) == evidence.ProcessId() &&
          WIFSIGNALED(childStatus) && WTERMSIG(childStatus) == SIGTERM,
          "synthetic managed ECC child did not exit during test cleanup");
    failedStore.reset(); store.reset();
    ::unlink((journalPath + ".lock").c_str()); ::unlink(journalPath.c_str());
    ::unlink((root + "/failed-scan.json.lock").c_str());
    ::unlink((root + "/failed-scan.json").c_str());
    ::unlink(configuration.c_str()); ::unlink(executable.c_str()); ::rmdir(root.c_str());
}

}

int main()
{
    try { Run(); }
    catch (const std::exception &error) {
        std::fprintf(stderr, "ECC service process supervisor test failed: %s\n", error.what());
        return 1;
    }
    try { RunManagedStart(); }
    catch (const std::exception &error) {
        std::fprintf(stderr, "ECC managed startup test failed: %s\n", error.what());
        return 1;
    }
    std::cout << "ECC service process supervisor test passed\n";
    return 0;
}
