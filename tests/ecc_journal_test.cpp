#include "eccjournal.h"
#include "../apps/ecc/scanexecutionjournalstore.h"
#include "../apps/ecc/scanruntimeidentitybinding.h"
#include "../apps/ecc/scanruntimeidentitycoordinator.h"
#include "../apps/ecc/scaneventcountbaselinecoordinator.h"
#include "../apps/ecc/scanoperationrunner.h"
#include "../common/experiment/configurationsha256.h"
#include "../common/experiment/pulseramplitudescan.h"
#include "../common/experiment/scanexecutionpolicy.h"
#include "../common/experiment/scanexecutionsnapshot.h"
#include "../common/experiment/systemscanplan.h"
#include "../tests/cobo_configure_fixture.h"
#include "../common/text/utf8.h"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <future>
#include <chrono>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace GetDAQ;
namespace {
const std::string Scope = "test-CoBo-0-configuration-v1";
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
struct RuntimeSnapshotFixture {
    ScanExecutionSnapshot snapshot;
    std::string configurationSha256;
    std::string artifactSha256;
    RuntimeSnapshotFixture(const ScanExecutionSnapshot &value,
                           const std::string &configuration,
                           const std::string &artifact)
        : snapshot(value), configurationSha256(configuration), artifactSha256(artifact) {}
};

SystemExperimentArtifact RuntimeArtifact()
{
    SystemExperimentArtifact artifact;
    artifact.profileSha256 = ConfigurationSha256("scan-journal-test-profile");
    ExperimentConfigurationSource source;
    source.path = "scan-journal-test.xcfg";
    source.sha256 = ConfigurationSha256("scan-journal-test-source");
    artifact.sources.push_back(source);
    SystemExperimentCoBo cobo;
    cobo.id = "C0";
    cobo.configure = GetDAQTest::Configure(9);
    cobo.prepare.inputs.asadMask = cobo.prepare.clock.asadMask =
        cobo.prepare.monitoring.asadMask = cobo.prepare.calibration.asadMask = 9;
    cobo.prepare.clock.allowNominalDifference = true;
    for (unsigned int id : {0U, 3U}) {
        AsAdInputBoardSettings input; input.asadId = id;
        input.positions[0].active = true; cobo.prepare.inputs.boards.push_back(input);
        AsAdBoardClockSettings clock; clock.asadId = id; clock.delayPs = -5335;
        cobo.prepare.clock.boards.push_back(clock);
        AsAdMonitoringBoardSettings monitor; monitor.asadId = id;
        monitor.externalTemperatureLower = -10; cobo.prepare.monitoring.boards.push_back(monitor);
        AsAdCalibrationSettings calibration; calibration.asadId = id;
        cobo.prepare.calibration.boards.push_back(calibration);
        cobo.prepare.calibration.chipVersions[4 * id] = 0x202;
    }
    cobo.configure.readout.aget.allowReg2BankResetDifference = true;
    artifact.settings.cobos.push_back(cobo);
    SystemExperimentCoBo disabled; disabled.id = "C1"; disabled.enabled = false;
    artifact.settings.cobos.push_back(disabled);
    SystemExperimentMutant mutant; mutant.id = "M0";
    mutant.settings.timestamp10ns = (UINT64_C(1) << 48) - 1;
    mutant.settings.eventNumber = UINT32_MAX;
    mutant.settings.twoProtonCounterCommand = false;
    mutant.settings.inspection[3] = 255;
    artifact.settings.mutants.push_back(mutant);
    return artifact;
}

RuntimeSnapshotFixture MakeRuntimeSnapshot(const std::string &executionId = "scan-store-1")
{
    const SystemExperimentArtifact baseline = RuntimeArtifact();
    const std::vector<PulserAmplitudeScanPoint> scan =
        BuildPulserAmplitudeScan(baseline, "C0", -1000, -999, 1);
    Require(scan.size() == 2, "Runtime snapshot fixture requires two scan points");
    std::map<std::string, std::string> files;
    std::ostringstream plan;
    plan << "{\"schema_version\":1,\"kind\":\"getdaq.pulser-amplitude-scan\","
         << "\"target_cobo\":\"C0\",\"start_mv\":-1000,\"stop_mv\":-999,\"step_mv\":1,"
         << "\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,"
         << "\"acquisition_event_count_unspecified\":true,\"points\":[";
    for (std::size_t i = 0; i < scan.size(); ++i) {
        const std::string name = "point-" + std::to_string(i) + ".json";
        const std::string bytes = EncodeSystemExperimentArtifact(scan[i].artifact);
        files[name] = bytes;
        if (i) plan << ',';
        plan << "{\"index\":" << i << ",\"voltage_mv\":" << scan[i].voltageMv
             << ",\"artifact\":" << EncodeJsonString(name)
             << ",\"configuration_sha256\":"
             << EncodeJsonString(scan[i].artifact.configurationSha256)
             << ",\"artifact_sha256\":" << EncodeJsonString(ConfigurationSha256(bytes)) << "}";
    }
    plan << "]}";
    const std::string planBytes = plan.str();
    const auto loader = [&files](const std::string &name) { return files.at(name); };
    const SystemScanPlanSummary summary = ValidateSystemScanPlan(planBytes, loader);
    const ScanExecutionPolicy policy = ScanExecutionPolicy::ForEventCount(
        planBytes, summary, 10, 1000);
    const ScanExecutionSnapshot snapshot = ScanExecutionSnapshot::Create(
        executionId, planBytes, policy.Encode(), loader);
    return RuntimeSnapshotFixture(snapshot, snapshot.Points()[0].configurationSha256,
                                  snapshot.Points()[0].artifactSha256);
}

template<class Action> void Reject(Action action, const std::string &part = "") {
    try { action(); } catch (const std::exception &e) {
        Require(std::string(e.what()).find(part) != std::string::npos, e.what()); return;
    }
    throw std::runtime_error("Expected rejection: " + part);
}
std::string Read(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    Require(bool(input), "Cannot read test file");
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
std::string InspectScanJournalWithTool(const std::string &path) {
    int output[2];
    Require(::pipe(output) == 0, "Cannot create scan journal tool output pipe");
    const pid_t child = ::fork();
    Require(child >= 0, "Cannot fork scan journal inspection tool");
    if (child == 0) {
        ::close(output[0]);
        if (::dup2(output[1], STDOUT_FILENO) < 0) ::_exit(125);
        ::close(output[1]);
        ::execl(GETDAQ_ECC_JOURNAL_TOOL, GETDAQ_ECC_JOURNAL_TOOL,
                "inspect-scan", path.c_str(), static_cast<char *>(nullptr));
        ::_exit(127);
    }
    ::close(output[1]);
    std::string result;
    char buffer[1024];
    for (;;) {
        const ssize_t count = ::read(output[0], buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        Require(count >= 0, "Cannot read scan journal tool output");
        if (count == 0) break;
        result.append(buffer, static_cast<std::size_t>(count));
        Require(result.size() <= 64U * 1024U, "Scan journal tool output exceeded its bound");
    }
    ::close(output[0]);
    int status = 0;
    Require(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0, "Scan journal inspection command failed");
    return result;
}
void Write(const std::string &path, const std::string &bytes) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    Require(fd >= 0, "Cannot create test file");
    const auto size = ::write(fd, bytes.data(), bytes.size()); ::close(fd);
    Require(size == static_cast<ssize_t>(bytes.size()), "Cannot write test file");
}
struct Directory {
    std::string path; std::set<std::string> files;
    Directory() { char buffer[] = "/tmp/getdaq-ecc-journal-XXXXXX"; Require(::mkdtemp(buffer), "mkdtemp failed"); path = buffer; }
    std::string File(const std::string &name) { const auto file = path + "/" + name; files.insert(file); return file; }
    ~Directory() { for (const auto &file : files) ::unlink(file.c_str()); ::rmdir(path.c_str()); }
};
class Backend : public EccBackend {
public:
    bool badConfig = false, failApply = false, recoveryAllowed = false;
    int applies = 0, verifies = 0, exitAtApply = 0;
    bool servePending = false;
    bool replaceAfterApply = false;
    EccState hardware = EccState::Idle;
    std::string path, effect;
    std::string errorMessage = "Hardware reply lost / 中文";
    void Validate(EccCommand, EccState, EccState) override {
        if (badConfig) throw std::runtime_error("Invalid configuration");
    }
    void Apply(EccCommand, EccState, EccState to) override {
        ++applies;
        if (!path.empty()) {
            const auto records = Read(path);
            Require(records.find("\"phase\":\"intent\"") != std::string::npos, "Hardware ran before intent was recorded");
            Require(records.back() == '\n', "Hardware ran after an incomplete intent");
        }
        if (exitAtApply == 77) ::_exit(77); // 意图已落盘，但硬件还未执行。
        hardware = to;
        if (!effect.empty()) Write(effect, std::to_string(static_cast<int>(to)));
        if (replaceAfterApply) { Require(::rename(path.c_str(), (path + ".old").c_str()) == 0, "rename failed"); Write(path, "replacement"); }
        if (exitAtApply == 78) ::_exit(78); // 硬件已经执行，完成记录尚未写入。
        if (servePending) { std::cout << "ready" << std::endl; std::string stop; std::getline(std::cin, stop); }
        if (failApply) throw std::runtime_error(errorMessage);
    }
    void VerifyRecovery(EccState state, const EccStatus &status, const std::string &) override {
        ++verifies;
        Require(status.outcomeUnknown || status.needsReconciliation, "Recovery without a recovery need");
        if (!recoveryAllowed || state != hardware) throw std::runtime_error("Hardware verification failed");
    }
};
class FaultStore : public EccPersistence {
public:
    int fault = 0, unknownCalls = 0, heldCalls = 0;
    EccStatus loaded;
    EccStatus Load() const override { return loaded; }
    void CheckNewId(const std::string &, const std::string &) const override {}
    void Begin(const std::string &, EccCommand, EccState, EccState, const std::string &) override {
        if (fault == 1) throw std::runtime_error("Intent fsync failed");
    }
    void Complete(const std::string &) override { if (fault == 2) throw std::runtime_error("Completion fsync failed"); }
    void Unknown(const std::string &, const std::string &) override { ++unknownCalls; if (fault == 3) throw std::runtime_error("Unknown fsync failed"); }
    void Reconciled(const std::string &, EccState, const std::string &) override { if (fault == 4) throw std::runtime_error("Recovery fsync failed"); }
    void Held(const std::string &, const std::string &) override {
        ++heldCalls;
        if (fault == 5) throw std::runtime_error("Hold fsync failed");
    }
};
class UnverifiedBackend : public EccBackend {
public:
    int applies = 0;
    void Validate(EccCommand, EccState, EccState) override {}
    void Apply(EccCommand, EccState, EccState) override { ++applies; }
};
class BlockingStore : public FaultStore {
public:
    int stage = 1;
    std::promise<void> entered;
    std::shared_future<void> release;
    void Begin(const std::string &id, EccCommand command, EccState from, EccState to, const std::string &key) override {
        if (stage == 1) { entered.set_value(); release.wait(); }
        FaultStore::Begin(id, command, from, to, key);
    }
    void Complete(const std::string &id) override {
        if (stage == 2) { entered.set_value(); release.wait(); }
        FaultStore::Complete(id);
    }
};
}
int main(int argc, char **argv) {
    try {
        if (argc == 3 && (std::string(argv[1]) == "--fixture" || std::string(argv[1]) == "--serve-pending")) {
            EccJournal journal(argv[2], Scope); Backend backend; backend.path = argv[2]; EccStateMachine machine(backend, journal);
            backend.servePending = std::string(argv[1]) == "--serve-pending";
            machine.Execute(EccCommand::Describe, "describe-1", "immutable-description-v1");
            if (backend.servePending) return 0;
            machine.Execute(EccCommand::Prepare, "prepare-1", "immutable-prepare-v1");
            machine.Execute(EccCommand::Configure, "configure-1", "immutable-physics-v1");
            machine.Execute(EccCommand::Start, "start-1", "run-1");
            backend.failApply = true;
            Reject([&]() { machine.Execute(EccCommand::Pause, "pause-1", "run-1"); });
            std::cout << "fixture-created" << std::endl; return 0;
        }
        Directory directory;
        const RuntimeSnapshotFixture runtimeSnapshot = MakeRuntimeSnapshot();
        const std::string executionSnapshotSha256 =
            ConfigurationSha256(runtimeSnapshot.snapshot.Encode());
        // New maintenance commands must remain readable after a process restart.
        const auto pulsePath = directory.File("led-pulse.jsonl");
        {
            EccJournal journal(pulsePath, Scope); Backend backend; EccStateMachine machine(backend, journal);
            machine.Execute(EccCommand::Describe, "pulse-describe", "description");
            machine.Execute(EccCommand::Prepare, "pulse-prepare", "prepare");
            machine.Execute(EccCommand::PulseCoBoLed, "pulse-led-10", "C0:10:725");
            Require(machine.Status().state == EccState::Prepared && journal.Operation("pulse-led-10").command == static_cast<int>(EccCommand::PulseCoBoLed),
                    "LED pulse intent/completion was not persisted");
        }
        {
            EccJournal journal(pulsePath, Scope, EccJournalMode::ReadOnly);
            Require(journal.Load().state == EccState::Prepared && journal.Load().needsReconciliation &&
                    journal.Operation("pulse-led-10").phase == "complete",
                    "Reopened journal rejected or lost the LED pulse operation");
        }
        const auto configureCoBoPath=directory.File("configure-cobo.jsonl");
        {
            EccJournal journal(configureCoBoPath,Scope);Backend backend;EccStateMachine machine(backend,journal);
            machine.Execute(EccCommand::Describe,"configure-cobo-describe","description");
            machine.Execute(EccCommand::Prepare,"configure-cobo-prepare","prepare");
            machine.Execute(EccCommand::ConfigureCoBo,"configure-cobo-one","fixed-target-C0");
            Require(machine.Status().state==EccState::Prepared && journal.Operation("configure-cobo-one").command==17,
                    "Single-CoBo Configure was not persisted as Prepared-to-Prepared");
        }
        {
            EccJournal journal(configureCoBoPath,Scope,EccJournalMode::ReadOnly);
            Require(journal.Load().state==EccState::Prepared && journal.Load().needsReconciliation &&
                    journal.Operation("configure-cobo-one").command==17 && journal.Operation("configure-cobo-one").phase=="complete",
                    "Reopened journal rejected or lost single-CoBo Configure");
        }
        const auto heldPath = directory.File("external-hold.jsonl");
        {
            EccJournal journal(heldPath, Scope); Backend backend; EccStateMachine machine(backend, journal);
            machine.Execute(EccCommand::Describe, "held-describe", "description");
            machine.Execute(EccCommand::Prepare, "held-prepare", "prepare");
            machine.Execute(EccCommand::Configure, "held-configure", "configure");
            machine.Execute(EccCommand::Start, "held-start", "run");
            const auto before = Read(heldPath);
            Reject([&]() { machine.HoldForReconciliation("another-start", "receiver failed"); }, "same persisted");
            Require(Read(heldPath) == before && !machine.Status().outcomeUnknown,
                    "Unrelated external hold changed the original result");
            machine.HoldForReconciliation("held-start", "original receiver failed after Start completed");
            Require(machine.Status().outcomeUnknown && machine.Status().needsReconciliation &&
                    machine.Status().operationId == "held-start" && backend.applies == 4,
                    "External hold lost binding or invoked hardware");
            Require(journal.Operation("held-start").phase == "complete" && journal.Load().outcomeUnknown,
                    "External hold rewrote successful Start or was not persisted");
            Reject([&]() { machine.Execute(EccCommand::Stop, "held-stop", "run"); });
            Require(backend.applies == 4, "Positive transition escaped the persistent external hold");
        }
        {
            EccJournal journal(heldPath, Scope); Backend backend; EccStateMachine machine(backend, journal);
            Require(machine.Status().outcomeUnknown && machine.Status().needsReconciliation &&
                    machine.Status().operationId == "held-start" && journal.Operation("held-start").phase == "complete",
                    "Reopened journal lost the hold or changed successful Start history");
            Reject([&]() { machine.Reconcile("held-recovery-denied", EccState::Described, "unverified cleanup"); });
            Require(machine.Status().outcomeUnknown, "Failed read-only verification cleared the external hold");
            backend.hardware = EccState::Described; backend.recoveryAllowed = true;
            machine.Reconcile("held-recovery", EccState::Described, "verified original cleanup");
            Require(!machine.Status().outcomeUnknown && !machine.Status().needsReconciliation &&
                    machine.Status().state == EccState::Described && backend.applies == 0 &&
                    journal.Operation("held-start").phase == "complete",
                    "Read-only reconciliation replayed hardware or rewrote Start history");
        }
        {
            EccJournal journal(heldPath, Scope, EccJournalMode::ReadOnly);
            Require(journal.Load().state == EccState::Described && journal.Load().needsReconciliation &&
                    !journal.Load().outcomeUnknown && journal.Operation("held-start").phase == "complete" &&
                    journal.Operation("held-recovery").phase == "reconciled",
                    "Cold reopening invented live hardware verification");
        }
        {
            FaultStore store; store.fault = 5; store.loaded.operationId = "held-start";
            store.loaded.state = EccState::Running; Backend backend; EccStateMachine machine(backend, store);
            Reject([&]() { machine.HoldForReconciliation("held-start", "receiver failed"); }, "Hold fsync failed");
            Require(store.heldCalls == 1 && machine.Status().outcomeUnknown && machine.Status().needsReconciliation,
                    "Failed hold persistence did not retain the local fence");
            Reject([&]() { machine.Execute(EccCommand::Stop, "unsafe-stop", "run"); });
            Require(backend.applies == 0, "Failed hold persistence permitted a hardware transition");
        }
        const auto main = directory.File("main.jsonl");
        {
            EccJournal journal(main, Scope); Backend backend; backend.path = main; EccStateMachine machine(backend, journal);
            Require(!machine.Status().needsReconciliation && machine.Status().state == EccState::Idle, "Fresh journal state failed");
            const auto before = Read(main);
            Reject([&]() { EccJournal competing(main, Scope); }, "locked");
            Reject([&]() { EccJournal observer(main, Scope, EccJournalMode::ReadOnly); }, "locked");
            Reject([&]() { machine.Execute(EccCommand::Describe); }, "operation ID");
            for (const auto &id : {"", "id with space", "id\nnewline", "中文"})
                Reject([&]() { machine.Execute(EccCommand::Describe, id, "config-v1"); });
            for (const auto &key : {std::string(""), std::string("\0", 1), std::string("\xc0\xaf"), std::string(4097, 'x')})
                Reject([&]() { machine.Execute(EccCommand::Describe, "bad-key", key); });
            Require(Read(main) == before && backend.applies == 0 && !machine.Status().needsReconciliation,
                    "Invalid input changed journal or invoked hardware");
            backend.badConfig = true;
            Reject([&]() { machine.Execute(EccCommand::Describe, "describe-1", "描述-v1\n包含来源"); }, "configuration");
            Require(journal.Operations().empty(), "Preflight failure consumed operation ID");
            backend.badConfig = false;
            machine.Execute(EccCommand::Describe, "describe-1", "描述-v1\n包含来源");
            Require(journal.Operation("describe-1").phase == "complete", "Completed operation lookup failed");
            machine.Execute(EccCommand::Prepare, "prepare-1", "prepare-v1");
            machine.Execute(EccCommand::Configure, "configure-1", "config-v1");
            Require(machine.Status().state == EccState::Ready && journal.Load().state == EccState::Ready, "Commit did not update both states");
            Reject([&]() { journal.CheckNewId("describe-1", "different-request"); }, "already recorded");
            Reject([&]() { journal.Complete("configure-1"); }, "pending");
            Require(backend.applies == 3, "Duplicate operation reached backend");
        }
        const auto completeBytes = Read(main);
        Reject([&]() { EccJournal other(main, "another-board"); }, "scope differs");
        Require(Read(main) == completeBytes, "Scope mismatch appended a session");
        {
            EccJournal journal(main, Scope); Backend backend; backend.hardware = EccState::Ready; backend.recoveryAllowed = true;
            EccStateMachine machine(backend, journal);
            Require(machine.Status().state == EccState::Ready && machine.Status().needsReconciliation && !machine.Status().outcomeUnknown,
                    "Restart treated historical Ready as fresh hardware state");
            Reject([&]() { machine.Execute(EccCommand::Start, "start-1", "run-1"); }, "reconciliation");
            Reject([&]() { machine.Reconcile("recover-bad", EccState::Prepared, "wrong state evidence"); }, "verification");
            Require(machine.Status().needsReconciliation && backend.applies == 0, "Failed recovery cleared the fence");
            machine.Reconcile("recover-1", EccState::Ready, "读回和部署版本一致 / board-generation-1");
            Require(!machine.Status().needsReconciliation && journal.Operation("recover-1").phase == "reconciled", "Recovery not committed");
            machine.Execute(EccCommand::Start, "start-1", "run-1");
            backend.failApply = true;
            Reject([&]() { machine.Execute(EccCommand::Pause, "pause-1", "run-1"); }, "reply lost");
            Require(machine.Status().state == EccState::Running && machine.Status().outcomeUnknown && journal.Operation("pause-1").phase == "unknown",
                    "Partial operation reported as complete");
            Reject([&]() { machine.Execute(EccCommand::Stop, "stop-1", "run-1"); }, "UNKNOWN");
            machine.Reconcile("recover-2", EccState::Paused, "核对暂停状态和已执行命令");
            Require(journal.Operation("pause-1").phase == "unknown", "Recovery rewrote the old unknown outcome");
            backend.failApply = false;
            machine.Execute(EccCommand::Stop, "stop-1", "run-1");
        }
        const auto saved = Read(main);
        {
            EccJournal readOnly(main, Scope, EccJournalMode::ReadOnly);
            Require(readOnly.Load().state == EccState::Ready && readOnly.Load().needsReconciliation, "Offline inspector state wrong");
            Require(readOnly.Operations().size() == 8, "Operation history count mismatch");
            Reject([&]() { readOnly.Begin("x", EccCommand::Start, EccState::Ready, EccState::Running, "x"); }, "read-only");
        }
        Require(Read(main) == saved, "Read-only inspection changed journal");
        {
            EccJournal exclusive(main, Scope, EccJournalMode::ExclusiveReadOnly);
            Require(exclusive.Load().state == EccState::Ready && exclusive.Load().needsReconciliation,
                "Exclusive read-only mode cleared historical reconciliation");
            Reject([&]() { EccJournal writer(main, Scope); }, "locked");
            Reject([&]() { EccJournal reader(main, Scope, EccJournalMode::ReadOnly); }, "locked");
            Reject([&]() { exclusive.Begin("x", EccCommand::Start, EccState::Ready, EccState::Running, "x"); }, "read-only");
            Reject([&]() { exclusive.Reconciled("x", EccState::Ready, "not-authorized"); }, "read-only");
        }
        Require(Read(main) == saved, "Exclusive read-only reservation changed journal");
        {
            EccJournal reader(main, Scope, EccJournalMode::ReadOnly);
            Reject([&]() { EccJournal exclusive(main, Scope, EccJournalMode::ExclusiveReadOnly); }, "locked");
        }
        // 各阶段真实子进程退出：父进程重新打开同一文件，不能重放意图或清空 unknown。
        for (int phase : {77, 78, 79}) {
            const auto file = directory.File("crash-" + std::to_string(phase));
            const auto effect = directory.File("effect-" + std::to_string(phase));
            const auto child = ::fork(); Require(child >= 0, "fork failed");
            if (child == 0) {
                try {
                    EccJournal journal(file, Scope); Backend backend; backend.path = file; backend.effect = effect; backend.exitAtApply = phase;
                    EccStateMachine machine(backend, journal); machine.Execute(EccCommand::Describe, "crashed", "description-v1");
                    ::_exit(79);
                } catch (...) { ::_exit(100); }
            }
            int status = 0; Require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == phase, "Child crash stage mismatch");
            Require((::access(effect.c_str(), F_OK) == 0) == (phase != 77), "Hardware marker does not match crash stage");
            EccJournal journal(file, Scope); Backend backend; EccStateMachine machine(backend, journal);
            Require(machine.Status().needsReconciliation && machine.Status().outcomeUnknown == (phase != 79), "Crash recovery misclassified completion");
            Require(machine.Status().state == (phase == 79 ? EccState::Described : EccState::Idle), "Unconfirmed state committed after crash");
            Reject([&]() { machine.Execute(EccCommand::Describe, "crashed", "description-v1"); });
            Reject([&]() { journal.Complete("crashed"); });
            Require(backend.applies == 0, "Restart replayed a hardware command");
        }
        // 真实文件写失败：在子进程限制文件大小，让意图写入产生残尾；硬件不得执行。
        const auto limited = directory.File("limited.jsonl");
        const auto limitedChild = ::fork(); Require(limitedChild >= 0, "fork failed");
        if (limitedChild == 0) {
            try {
                EccJournal journal(limited, Scope); Backend backend; EccStateMachine machine(backend, journal);
                struct stat info; Require(::stat(limited.c_str(), &info) == 0, "stat failed");
                struct rlimit size; size.rlim_cur = size.rlim_max = static_cast<rlim_t>(info.st_size + 40);
                Require(::setrlimit(RLIMIT_FSIZE, &size) == 0, "setrlimit failed");
                std::signal(SIGXFSZ, SIG_IGN);
                Reject([&]() { machine.Execute(EccCommand::Describe, "disk-failed", "description-v1"); }, "append");
                Require(backend.applies == 0 && machine.Status().needsReconciliation && !machine.Status().outcomeUnknown,
                        "Incomplete intent reached hardware");
                ::_exit(80);
            } catch (...) { ::_exit(100); }
        }
        int limitedStatus = 0;
        Require(::waitpid(limitedChild, &limitedStatus, 0) == limitedChild && WIFEXITED(limitedStatus) && WEXITSTATUS(limitedStatus) == 80,
                "Real storage-failure test failed");
        const auto limitedBytes = Read(limited);
        Reject([&]() { EccJournal damaged(limited, Scope); }, "truncated");
        Require(Read(limited) == limitedBytes, "Failed storage tail was modified");
        // 截断、序号跳跃、重复键/ID、非法转换、未来 schema 均不能被自动修复。
        std::vector<std::string> corrupt;
        const auto header=saved.substr(0,saved.find('\n')+1);
        const auto replaceHeader=[&](const std::string &from,const std::string &to) {
            auto text=header;const auto at=text.find(from);Require(at!=std::string::npos,"Header fixture token missing");text.replace(at,from.size(),to);return text;
        };
        corrupt.push_back(replaceHeader("\"schema_version\":1","\"schema_version\":\"1\""));
        corrupt.push_back(replaceHeader("\"seq\":\"0\"","\"seq\":0"));
        corrupt.push_back(replaceHeader("\"state\":1","\"state\":1.0"));
        corrupt.push_back(replaceHeader("\"phase\":\"header\"","\"phase\":true"));
        corrupt.push_back(replaceHeader("\"state\":1","\"state\":1 /*comment*/"));
        corrupt.push_back(replaceHeader("\"state\":1","\"state\":1,\"future\":false"));
        corrupt.push_back(saved.substr(0, saved.size() - 1));
        corrupt.push_back(saved.substr(0, saved.find('\n') + 1) + "{\"seq\":\"1\",\"phase\":\"intent\"");
        auto future = saved; future.replace(future.find("\"schema_version\":1"), 18, "\"schema_version\":2"); corrupt.push_back(future);
        auto duplicate = saved; duplicate.insert(duplicate.find("\"state\":1"), "\"state\":1,"); corrupt.push_back(duplicate);
        auto gap = saved; gap.replace(gap.find("\"seq\":\"1\""), 9, "\"seq\":\"9\""); corrupt.push_back(gap);
        auto wrong = saved; wrong.replace(wrong.find("\"to\":2"), 6, "\"to\":5"); corrupt.push_back(wrong);
        auto extra = saved; extra.insert(extra.find("\"state\":1"), "\"extra\":true,"); corrupt.push_back(extra);
        corrupt.push_back(saved + "{\"seq\":\"" + std::to_string(std::count(saved.begin(), saved.end(), '\n')) +
            "\",\"phase\":\"intent\",\"id\":\"describe-1\",\"command\":4,\"from\":4,\"to\":5,\"request_key\":\"new-request\"}\n");
        int number = 0;
        for (const auto &bytes : corrupt) {
            const auto file = directory.File("corrupt-" + std::to_string(++number)); Write(file, bytes);
            Reject([&]() { EccJournal bad(file, Scope); });
            Require(Read(file) == bytes, "Corrupt journal was truncated or modified");
        }
        const auto absent = directory.File("absent");
        Reject([&]() { EccJournal readOnly(absent, Scope, EccJournalMode::ReadOnly); }, "Cannot open");
        Require(::access(absent.c_str(), F_OK) != 0, "Read-only inspector created a file");
        const auto symlink = directory.File("symlink"); Require(::symlink(main.c_str(), symlink.c_str()) == 0, "symlink failed");
        Reject([&]() { EccJournal alias(symlink, Scope); }, "Cannot open");
        for (bool afterApply : {false, true}) {
            const auto file = directory.File(afterApply ? "replace-after" : "replace-before");
            directory.File((afterApply ? "replace-after" : "replace-before") + std::string(".old"));
            {
                EccJournal journal(file, Scope); Backend backend; backend.path = file; backend.replaceAfterApply = afterApply;
                EccStateMachine machine(backend, journal);
                if (!afterApply) { Require(::rename(file.c_str(), (file + ".old").c_str()) == 0, "rename failed"); Write(file, "replacement"); }
                Reject([&]() { machine.Execute(EccCommand::Describe, "replaced", "description-v1"); }, "modified/replaced/deleted");
                Require(machine.Status().needsReconciliation && machine.Status().outcomeUnknown == afterApply,
                        "File replacement fence misclassified hardware outcome");
                Require(backend.applies == (afterApply ? 1 : 0) && Read(file) == "replacement", "Replaced journal was overwritten or hardware replayed");
            }
            EccJournal old(file + ".old", Scope, EccJournalMode::ReadOnly);
            Require(old.Load().outcomeUnknown == afterApply, "Original journal lost intent after replacement");
        }
        // 持久化异常不能靠新内存状态掩盖。Begin 失败时不进入 Apply，Complete 失败时不重做 Apply。
        for (int fault = 1; fault <= 3; ++fault) {
            FaultStore store; store.fault = fault; Backend backend; backend.failApply = fault == 3;
            EccStateMachine machine(backend, store);
            Reject([&]() { machine.Execute(EccCommand::Describe, "storage-fault", "description-v1"); });
            Require(machine.Status().needsReconciliation && machine.Status().outcomeUnknown == (fault != 1), "Storage failure fence wrong");
            Require(backend.applies == (fault == 1 ? 0 : 1) && store.unknownCalls == (fault == 3 ? 1 : 0), "Storage failure replayed or invented outcome");
            Reject([&]() { machine.Execute(EccCommand::Describe, "try-again", "description-v1"); });
        }
        FaultStore failedRecovery; failedRecovery.fault = 4; failedRecovery.loaded.needsReconciliation = true;
        Backend recovery; recovery.recoveryAllowed = true; EccStateMachine recovering(recovery, failedRecovery);
        Reject([&]() { recovering.Reconcile("recover", EccState::Idle, "verified idle"); });
        Require(recovering.Status().needsReconciliation && recovery.applies == 0, "Failed recovery persistence cleared fence");
        FaultStore unverifiedStore; unverifiedStore.loaded.needsReconciliation = true;
        UnverifiedBackend unverified; EccStateMachine unavailable(unverified, unverifiedStore);
        Reject([&]() { unavailable.Reconcile("recover", EccState::Idle, "a note is not a hardware check"); }, "not implemented");
        Require(unavailable.Status().needsReconciliation && unverified.applies == 0, "Missing recovery backend cleared fence");
        int errorIndex = 0;
        for (const auto &error : {std::string(""), std::string("\xc0\xaf"), std::string(3999, 'a') + "中文" + std::string(100, 'x')}) {
            const auto file = directory.File("error-text-" + std::to_string(++errorIndex));
            {
                EccJournal journal(file, Scope); Backend backend; backend.failApply = true; backend.errorMessage = error;
                EccStateMachine machine(backend, journal);
                Reject([&]() { machine.Execute(EccCommand::Describe, "error-text", "description-v1"); });
                Require(machine.Status().outcomeUnknown && !machine.Status().needsReconciliation, "Error text prevented unknown persistence");
                const auto message = journal.Operation("error-text").message;
                Require(!message.empty() && message.size() <= 4096 && ValidUtf8(message), "Invalid persisted error text");
                Require(message == machine.Status().message, "Error normalization was not idempotent");
            }
            EccJournal check(file, Scope, EccJournalMode::ReadOnly);
            Require(check.Operation("error-text").phase == "unknown", "Bounded error record cannot be reloaded");
        }
        for (int stage : {1, 2}) {
            BlockingStore store; store.stage = stage; std::promise<void> release;
            store.release = release.get_future().share(); Backend backend; EccStateMachine controller(backend, store);
            auto entered = store.entered.get_future();
            auto action = std::async(std::launch::async, [&]() { controller.Execute(EccCommand::Describe, "blocked-storage", "description-v1"); });
            const bool reached = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
            auto query = std::async(std::launch::async, [&]() { return controller.Status(); });
            const bool responsive = query.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
            EccStatus busy;
            if (responsive) busy = query.get();
            auto duplicate = std::async(std::launch::async, [&]() {
                try { controller.Execute(EccCommand::Describe, "duplicate", "description-v1"); } catch (...) { return true; }
                return false;
            });
            const bool refusedImmediately = duplicate.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
            release.set_value(); action.get();
            if (!responsive) query.get();
            const bool refused = duplicate.get();
            Require(reached && responsive && refusedImmediately && refused && busy.state == EccState::Idle && busy.transition == 1,
                    "Storage latency blocked status, queued a command, or committed state too early");
            Require(backend.applies == 1 && controller.Status().state == EccState::Described, "Storage latency caused a replay");
        }
        const auto scanPath = directory.File("scan-execution.json");
        directory.File("scan-execution.json.lock");
        auto scanStore = ScanExecutionJournalStore::Create(
            scanPath, ScanExecutionJournal(runtimeSnapshot.snapshot.ExecutionId(),
                runtimeSnapshot.snapshot.SourcePlanSha256(), executionSnapshotSha256,
                runtimeSnapshot.snapshot.PointCount()));
        struct stat scanInfo;
        Require(::lstat(scanPath.c_str(), &scanInfo) == 0 && S_ISREG(scanInfo.st_mode) &&
                (scanInfo.st_mode & 0777) == 0600 && scanInfo.st_nlink == 1,
                "Scan execution journal file is not private and regular");
        Reject([&]() { ScanExecutionJournalStore::Open(scanPath); }, "already owned");
        const auto reviewed = scanStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed);
        scanStore->Persist(reviewed);
        const auto intent = scanStore->Current().WithOperationIntent(
            "start-1", "start-ecc", std::string(64, 'b'));
        scanStore->Persist(intent);
        Require(scanStore->Current().State() == ScanPointState::Reviewed &&
                scanStore->Current().HasPendingOperation() &&
                !scanStore->Current().PendingOperationUnknown(),
                "Active store did not retain its pending live operation");
        scanStore.reset();
        const std::string retainedIntent = Read(scanPath);
        Reject([&]() {
            ScanExecutionJournalStore::Create(
                scanPath, ScanExecutionJournal("replacement", std::string(64, '9'), std::string(64, 'f'), 1));
        }, "without replacing");
        Require(Read(scanPath) == retainedIntent,
                "Creating an existing scan journal replaced its durable intent");
        const std::string scanInspection = InspectScanJournalWithTool(scanPath);
        Require(scanInspection.find("\"kind\":\"getdaq.scan-execution-status\"") != std::string::npos &&
                scanInspection.find("\"state\":\"NeedsReconciliation\"") != std::string::npos &&
                scanInspection.find("\"pending_operation_unknown\":true") != std::string::npos &&
                scanInspection.find("\"pending_operation_id\":\"start-1\"") != std::string::npos,
                "Scan journal CLI did not report the durable unresolved operation");
        auto recoveredScan = ScanExecutionJournalStore::Open(scanPath);
        Require(recoveredScan->Current().State() == ScanPointState::NeedsReconciliation &&
                recoveredScan->Current().PendingOperationUnknown() &&
                recoveredScan->Current().PendingOperationId() == "start-1",
                "Reopened pending scan operation was not quarantined");
        Reject([&]() {
            recoveredScan->Current().WithOperationComplete(
                "start-1", std::string(64, 'c'), ScanPointState::EccStarted);
        }, "does not match");
        const auto reconciledScan = recoveredScan->Current().WithOperationReconciled(
            "start-1", "operator-1", ScanOperationDisposition::ConfirmedApplied,
            std::string(64, 'c'));
        recoveredScan->Persist(reconciledScan);
        recoveredScan.reset();
        auto reconciledScanReopened = ScanExecutionJournalStore::Open(scanPath);
        Require(reconciledScanReopened->Current().State() == ScanPointState::EccStarted &&
                !reconciledScanReopened->Current().HasPendingOperation(),
                "Persisted operator reconciliation did not survive store reopen");
        ScanRuntimeIdentityDeployment runtimeDeployment;
        runtimeDeployment.serviceId = "scan-point-0";
        runtimeDeployment.clientId = "console";
        runtimeDeployment.instanceId = std::string(64, 'a');
        runtimeDeployment.snapshotSha256 = std::string(64, 'f');
        runtimeDeployment.inputBindingSha256 = std::string(64, 'c');
        runtimeDeployment.dataBindingSha256 = std::string(64, '1');
        runtimeDeployment.dataScopeSha256 = std::string(64, '2');
        runtimeDeployment.dataEpoch = std::string(64, '3');
        runtimeDeployment.dataNodes.push_back({"receiver-0", std::string(64, '4'), std::string(64, '5')});
        const ScanRuntimeIdentityExpectation runtimeExpected =
            BindScanRuntimeIdentityExpectation(runtimeSnapshot.snapshot, 0, runtimeDeployment);
        Require(runtimeExpected.executionSnapshotSha256 == executionSnapshotSha256 &&
                runtimeExpected.configurationSha256 == runtimeSnapshot.configurationSha256 &&
                runtimeExpected.artifactSha256 == runtimeSnapshot.artifactSha256,
                "Runtime identity builder did not derive immutable hashes from its snapshot point");
        const ScanRuntimeIdentityExpectation nextPointExpected =
            BindScanRuntimeIdentityExpectation(runtimeSnapshot.snapshot, 1, runtimeDeployment);
        Require(nextPointExpected.configurationSha256 != runtimeExpected.configurationSha256 ||
                nextPointExpected.artifactSha256 != runtimeExpected.artifactSha256,
                "Adjacent scan point unexpectedly reused both configuration and artifact identities");
        Reject([&]() { BindScanRuntimeIdentityExpectation(runtimeSnapshot.snapshot, 2, runtimeDeployment); },
               "outside the frozen execution snapshot");
        const std::string runtimeIdentity =
            "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":\"scan-point-0\",\"client_id\":\"console\",\"instance_id\":\""+std::string(64, 'a')+
            "\",\"snapshot_sha256\":\""+std::string(64, 'f')+"\",\"input_binding_sha256\":\""+std::string(64, 'c')+
            "\",\"configuration_sha256\":\""+runtimeSnapshot.configurationSha256+"\",\"artifact_sha256\":\""+runtimeSnapshot.artifactSha256+
            "\",\"data_binding_sha256\":\""+std::string(64, '1')+"\"}";
        const std::string runtimeNode =
            "{\"agent_id\":\"receiver-0\",\"instance_id\":\""+std::string(64, '4')+"\",\"plan_sha256\":\""+std::string(64, '5')+
            "\",\"lifecycle\":{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+std::string(64, '5')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]}}";
        const std::string runtimeStatus =
            "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\""+std::string(64, 'a')+
            "\",\"snapshot_sha256\":\""+std::string(64, 'f')+"\",\"operation_active\":false,\"operation_id\":\"\",\"client_epoch\":\"\",\"control_needs_reconciliation\":false,\"message\":\"\","
            "\"controller\":{\"state\":1,\"transition\":0,\"error\":0,\"outcome_unknown\":false,\"needs_reconciliation\":false,\"operation_id\":\"\",\"message\":\"\"},"
            "\"data\":{\"needs_reconciliation\":false,\"controller_scope_sha256\":\""+std::string(64, '2')+"\",\"controller_epoch\":\""+std::string(64, '3')+
            "\",\"message\":\"\",\"nodes\":["+runtimeNode+"]}}";
        const std::string runtimeReceivers =
            "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\""+std::string(64, 'a')+
            "\",\"snapshot_sha256\":\""+std::string(64, 'f')+"\",\"data_binding_sha256\":\""+std::string(64, '1')+"\",\"nodes\":["
            "{\"agent_id\":\"receiver-0\",\"plan_sha256\":\""+std::string(64, '5')+"\",\"instance_id\":\""+std::string(64, '4')+
            "\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1000\",\"lifecycle\":"
            "{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+std::string(64, '5')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]},\"receivers\":[]}}]}";
        ScanRuntimePointIdentityResponse runtimeResponse;
        runtimeResponse.identityJson = runtimeIdentity;
        runtimeResponse.statusJson = runtimeStatus;
        runtimeResponse.receiversJson = runtimeReceivers;
        runtimeResponse.expectation = runtimeExpected;
        const std::string runtimeResponseBytes =
            EncodeScanRuntimePointIdentityResponse(runtimeResponse);
        const std::string bytesBeforeBadBinding = Read(scanPath);
        const auto notStartedPath = directory.File("scan-not-started.json");
        directory.File("scan-not-started.json.lock");
        auto notStartedStore = ScanExecutionJournalStore::Create(notStartedPath,
            ScanExecutionJournal(runtimeSnapshot.snapshot.ExecutionId(),
                runtimeSnapshot.snapshot.SourcePlanSha256(), executionSnapshotSha256,
                runtimeSnapshot.snapshot.PointCount()));
        bool notStartedQueryRejected = false;
        try {
            ScanRuntimeIdentityCoordinator coordinator(*notStartedStore,
                runtimeSnapshot.snapshot, runtimeExpected);
        } catch (const std::exception &error) {
            notStartedQueryRejected = std::string(error.what()).find("durably completed start-ecc") != std::string::npos;
        }
        Require(notStartedQueryRejected,
                "Identity query coordinator allowed a request before start-ecc completed");
        notStartedStore.reset();
        const auto adoptionPath = directory.File("scan-adopt-ecc.json");
        directory.File("scan-adopt-ecc.json.lock");
        auto adoptionStore = ScanExecutionJournalStore::Create(adoptionPath,
            ScanExecutionJournal(runtimeSnapshot.snapshot.ExecutionId(),
                runtimeSnapshot.snapshot.SourcePlanSha256(),
                runtimeSnapshot.snapshot.PointCount()));
        adoptionStore->Persist(adoptionStore->Current().WithSnapshotBinding(
            executionSnapshotSha256));
        adoptionStore->Persist(adoptionStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanRuntimeIdentityCoordinator adoptionCoordinator(*adoptionStore,
            runtimeSnapshot.snapshot, runtimeExpected, false, true);
        const ScanRuntimePointIdentityRequest adoptionRequest =
            adoptionCoordinator.PrepareAdoptionQuery("adopt-current-ecc-0");
        Require(adoptionRequest.pointIndex == 0 &&
                adoptionStore->Current().State() == ScanPointState::Reviewed &&
                adoptionStore->Current().PendingOperation() == "adopt-ecc" &&
                adoptionStore->Current().Encode().find("\"schema_version\":5") != std::string::npos,
                "ECC adoption did not persist a schema-5 intent before identity verification");
        const ScanExecutionJournal adoptedEcc = adoptionCoordinator.BindResponse(
            runtimeResponseBytes);
        Require(adoptedEcc.State() == ScanPointState::EccStarted &&
                !adoptedEcc.HasPendingOperation() &&
                adoptedEcc.RuntimeIdentitySha256() == ObserveScanRuntimeIdentity(
                    runtimeIdentity, runtimeStatus, runtimeReceivers,
                    runtimeExpected).sha256,
                "Matching external ECC identity was not durably adopted and bound");
        adoptionStore.reset();
        auto reopenedAdoptionStore = ScanExecutionJournalStore::Open(adoptionPath);
        Require(reopenedAdoptionStore->Current().State() == ScanPointState::EccStarted &&
                reopenedAdoptionStore->Current().RuntimeIdentitySha256() ==
                    adoptedEcc.RuntimeIdentitySha256(),
                "ECC adoption and identity binding did not survive journal reopen");
        reopenedAdoptionStore.reset();

        const auto rejectedAdoptionPath = directory.File("scan-rejected-adoption.json");
        directory.File("scan-rejected-adoption.json.lock");
        auto rejectedAdoptionStore = ScanExecutionJournalStore::Create(rejectedAdoptionPath,
            ScanExecutionJournal(runtimeSnapshot.snapshot.ExecutionId(),
                runtimeSnapshot.snapshot.SourcePlanSha256(),
                runtimeSnapshot.snapshot.PointCount()));
        rejectedAdoptionStore->Persist(rejectedAdoptionStore->Current().WithSnapshotBinding(
            executionSnapshotSha256));
        rejectedAdoptionStore->Persist(rejectedAdoptionStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanRuntimeIdentityCoordinator rejectedAdoption(*rejectedAdoptionStore,
            runtimeSnapshot.snapshot, runtimeExpected, false, true);
        bool adoptionMismatchRejected = false;
        try {
            (void)rejectedAdoption.PrepareAdoptionQuery("adopt-rejected-ecc-0");
            ScanRuntimePointIdentityResponse wrongAdoptionResponse = runtimeResponse;
            wrongAdoptionResponse.expectation.clientId = "other-client";
            (void)rejectedAdoption.BindResponse(
                EncodeScanRuntimePointIdentityResponse(wrongAdoptionResponse));
        } catch (const std::exception &error) {
            adoptionMismatchRejected = std::string(error.what()).find("identity differs") !=
                                       std::string::npos;
        }
        Require(adoptionMismatchRejected &&
                rejectedAdoptionStore->Current().State() == ScanPointState::Reviewed &&
                rejectedAdoptionStore->Current().HasPendingOperation() &&
                rejectedAdoptionStore->Current().RuntimeIdentitySha256().empty(),
                "Mismatched ECC identity advanced or bound a reviewed scan point");
        rejectedAdoptionStore.reset();

        const RuntimeSnapshotFixture otherRuntimeSnapshot = MakeRuntimeSnapshot("scan-store-other");
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            otherRuntimeSnapshot.snapshot, runtimeExpected, runtimeIdentity, runtimeStatus, runtimeReceivers); },
            "frozen execution input");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Snapshot object from another execution changed the durable journal");
        auto wrongExpectedRuntime = runtimeExpected;
        wrongExpectedRuntime.artifactSha256 = std::string(64, '9');
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            runtimeSnapshot.snapshot, wrongExpectedRuntime, runtimeIdentity, runtimeStatus, runtimeReceivers); }, "frozen scan point");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Rejected runtime identity observation changed the durable journal");
        wrongExpectedRuntime = runtimeExpected;
        wrongExpectedRuntime.executionSnapshotSha256 = std::string(64, '8');
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            runtimeSnapshot.snapshot, wrongExpectedRuntime, runtimeIdentity, runtimeStatus, runtimeReceivers); }, "different scan execution snapshot");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Wrong-snapshot runtime observation changed the durable journal");
        wrongExpectedRuntime = runtimeExpected;
        wrongExpectedRuntime.pointIndex = 1;
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            runtimeSnapshot.snapshot, wrongExpectedRuntime, runtimeIdentity, runtimeStatus, runtimeReceivers); }, "different scan point");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Wrong-point runtime observation changed the durable journal");
        wrongExpectedRuntime = runtimeExpected;
        wrongExpectedRuntime.clientId = "different-client";
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            runtimeSnapshot.snapshot, wrongExpectedRuntime, runtimeResponseBytes); },
            "different deployment or scan point");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Response envelope from a different deployment changed the durable journal");
        std::string staleRuntimeReceivers=runtimeReceivers;
        const std::string freshState="\"state\":\"fresh\"";
        const std::size_t freshStateAt=staleRuntimeReceivers.find(freshState);
        Require(freshStateAt!=std::string::npos,"Fresh receiver fixture state missing");
        staleRuntimeReceivers.replace(freshStateAt,freshState.size(),"\"state\":\"ready\"");
        Reject([&]() { PersistScanRuntimeIdentityBinding(*reconciledScanReopened,
            runtimeSnapshot.snapshot, runtimeExpected, runtimeIdentity, runtimeStatus, staleRuntimeReceivers); }, "stale");
        Require(Read(scanPath) == bytesBeforeBadBinding,
                "Stale receiver observation changed the durable journal");
        ScanRuntimeIdentityCoordinator identityCoordinator(*reconciledScanReopened,
            runtimeSnapshot.snapshot, runtimeExpected);
        const ScanRuntimePointIdentityRequest preparedIdentityRequest =
            identityCoordinator.PrepareQuery();
        Require(preparedIdentityRequest.executionSnapshotSha256 == executionSnapshotSha256 &&
                preparedIdentityRequest.pointIndex == 0 &&
                preparedIdentityRequest.configurationSha256 == runtimeSnapshot.configurationSha256 &&
                preparedIdentityRequest.artifactSha256 == runtimeSnapshot.artifactSha256,
                "Identity query coordinator did not derive its request from the current frozen journal point");
        const auto validatedBinding = identityCoordinator.BindResponse(runtimeResponseBytes);
        Require(validatedBinding.RuntimeIdentitySha256() == ObserveScanRuntimeIdentity(
            runtimeIdentity, runtimeStatus, runtimeReceivers, runtimeExpected).sha256,
            "Validated runtime observation digest differs from the journal binding");
        ScanRuntimeIdentityCoordinator identityRevalidator(
            *reconciledScanReopened, runtimeSnapshot.snapshot, runtimeExpected, true);
        const ScanRuntimePointIdentityRequest revalidationRequest =
            identityRevalidator.PrepareRevalidationQuery();
        Require(revalidationRequest.pointIndex == 0 &&
                revalidationRequest.executionSnapshotSha256 == executionSnapshotSha256 &&
                revalidationRequest.mode == "revalidate" &&
                revalidationRequest.expectedControllerState == 1U,
                "Identity recovery query did not retain the frozen point binding");
        const std::string journalBeforeIdentityRevalidation = Read(scanPath);
        ScanRuntimePointIdentityResponse eccStartedRevalidationResponse = runtimeResponse;
        eccStartedRevalidationResponse.mode = "revalidate";
        eccStartedRevalidationResponse.observedControllerState = 1U;
        const std::string eccStartedRevalidationBytes =
            EncodeScanRuntimePointIdentityResponse(eccStartedRevalidationResponse);
        const ScanExecutionJournal revalidatedBinding =
            identityRevalidator.BindResponse(eccStartedRevalidationBytes);
        Require(revalidatedBinding.RuntimeIdentitySha256() ==
                    validatedBinding.RuntimeIdentitySha256() &&
                Read(scanPath) == journalBeforeIdentityRevalidation,
                "Fresh identity revalidation changed the persisted journal binding");
        ScanRuntimePointIdentityResponse wrongRuntimeIdentity = runtimeResponse;
        const std::string expectedInstance = "\"instance_id\":\"" + std::string(64, 'a') + "\"";
        const std::size_t expectedInstanceAt = wrongRuntimeIdentity.identityJson.find(expectedInstance);
        Require(expectedInstanceAt != std::string::npos,
                "Runtime identity fixture instance is missing");
        wrongRuntimeIdentity.identityJson.replace(expectedInstanceAt, expectedInstance.size(),
            "\"instance_id\":\"" + std::string(64, 'b') + "\"");
        Reject([&]() { identityRevalidator.BindResponse(
            EncodeScanRuntimePointIdentityResponse(wrongRuntimeIdentity)); },
            "ECC identity differs from the frozen scan deployment");
        Require(Read(scanPath) == journalBeforeIdentityRevalidation,
                "Mismatched identity revalidation changed the scan journal");
        bool duplicateQueryRejected = false;
        try { (void)identityCoordinator.PrepareQuery(); }
        catch (const std::exception &error) {
            duplicateQueryRejected = std::string(error.what()).find("already bound") != std::string::npos;
        }
        Require(duplicateQueryRejected,
                "Identity query coordinator allowed the same scan point to be rebound");
        reconciledScanReopened.reset();
        auto runtimeBoundScan = ScanExecutionJournalStore::Open(scanPath);
        Require(runtimeBoundScan->Current().RuntimeIdentitySha256() == validatedBinding.RuntimeIdentitySha256(),
                "Persisted validated runtime identity binding did not survive store reopen");
        const auto describeAfterRuntimeBind = runtimeBoundScan->Current().WithOperationIntent(
            "describe-after-runtime-bind", "describe", std::string(64, '9'));
        Require(describeAfterRuntimeBind.PendingOperationId() == "describe-after-runtime-bind",
                "Describe intent was not enabled by the persisted runtime identity binding");
        const auto completeScanStep = [&](const std::string &id, const std::string &operation,
                                          ScanPointState target, char requestDigest, char responseDigest) {
            try {
                runtimeBoundScan->Persist(runtimeBoundScan->Current().WithOperationIntent(
                    id, operation, std::string(64, requestDigest)));
            } catch (const std::exception &error) {
                throw std::runtime_error(id + " intent failed: " + error.what());
            }
            try {
                runtimeBoundScan->Persist(runtimeBoundScan->Current().WithOperationComplete(
                    id, std::string(64, responseDigest), target));
            } catch (const std::exception &error) {
                throw std::runtime_error(id + " completion failed: " + error.what());
            }
        };
        completeScanStep("describe-for-baseline", "describe", ScanPointState::Described, 'a', 'b');
        completeScanStep("prepare-for-baseline", "prepare", ScanPointState::Prepared, 'c', 'd');
        completeScanStep("configure-for-baseline", "configure", ScanPointState::Configured, 'e', 'f');
        ScanRuntimeIdentityCoordinator configuredIdentityRevalidator(
            *runtimeBoundScan, runtimeSnapshot.snapshot, runtimeExpected, true);
        ScanRuntimePointIdentityResponse configuredIdentityResponse = runtimeResponse;
        configuredIdentityResponse.mode = "revalidate";
        configuredIdentityResponse.observedControllerState = 4U;
        const std::string readySession(64, '6');
        const std::string readyNode =
            "{\"agent_id\":\"receiver-0\",\"instance_id\":\"" + std::string(64, '4') +
            "\",\"plan_sha256\":\"" + std::string(64, '5') +
            "\",\"lifecycle\":{\"schema\":1,\"state\":\"ready\",\"needs_reconciliation\":false,\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"session_id\":\"" + readySession +
            "\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\"" +
            readySession + "\",\"output_directory\":\"/tmp/getdaq-scan-test\",\"pid\":1234,\"exit_code\":-1,\"reaped\":false,\"seal_verified\":false,\"received_bytes\":\"0\",\"frames\":\"0\",\"files\":\"0\"}]}}";
        configuredIdentityResponse.statusJson =
            "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\"" +
            std::string(64, 'a') + "\",\"snapshot_sha256\":\"" + std::string(64, 'f') +
            "\",\"operation_active\":false,\"operation_id\":\"configure-for-baseline\",\"client_epoch\":\"\",\"control_needs_reconciliation\":false,\"message\":\"\",\"controller\":{\"state\":4,\"transition\":0,\"error\":0,\"outcome_unknown\":false,\"needs_reconciliation\":false,\"operation_id\":\"configure-for-baseline\",\"message\":\"\"},\"data\":{\"needs_reconciliation\":false,\"controller_scope_sha256\":\"" +
            std::string(64, '2') + "\",\"controller_epoch\":\"" + std::string(64, '3') +
            "\",\"message\":\"\",\"nodes\":[" + readyNode + "]}}";
        configuredIdentityResponse.receiversJson =
            "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\"" +
            std::string(64, 'a') + "\",\"snapshot_sha256\":\"" + std::string(64, 'f') +
            "\",\"data_binding_sha256\":\"" + std::string(64, '1') +
            "\",\"nodes\":[{\"agent_id\":\"receiver-0\",\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"instance_id\":\"" + std::string(64, '4') +
            "\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1234\",\"lifecycle\":{\"schema\":1,\"state\":\"ready\",\"needs_reconciliation\":false,\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"session_id\":\"" + readySession +
            "\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\"" +
            readySession + "\",\"output_directory\":\"/tmp/getdaq-scan-test\",\"pid\":1234,\"exit_code\":-1,\"reaped\":false,\"seal_verified\":false,\"received_bytes\":\"0\",\"frames\":\"0\",\"files\":\"0\"}]},\"receivers\":[{\"id\":\"C0\",\"pid\":1234,\"state\":\"running\",\"exit_code\":-1,\"signal\":0,\"message\":\"\"}]}}]}";
        const std::string configuredIdentityResponseBytes =
            EncodeScanRuntimePointIdentityResponse(configuredIdentityResponse);
        const std::string journalBeforeConfiguredIdentityRevalidation =
            runtimeBoundScan->Current().Encode();
        const ScanExecutionJournal configuredIdentity =
            configuredIdentityRevalidator.BindResponse(configuredIdentityResponseBytes);
        Require(configuredIdentity.State() == ScanPointState::Configured &&
                configuredIdentity.RuntimeIdentitySha256() ==
                    validatedBinding.RuntimeIdentitySha256() &&
                runtimeBoundScan->Current().Encode() ==
                    journalBeforeConfiguredIdentityRevalidation,
                "Configured point identity recovery changed journal state or binding");
        ScanEventCountBaselineCoordinator eventBaselineCoordinator(
            *runtimeBoundScan, runtimeSnapshot.snapshot, runtimeExpected);
        Require(eventBaselineCoordinator.PrepareQuery() == "C0",
                "Event-count baseline query did not select the frozen CoBo source");
        const std::string receiverEvents =
            "{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-events\",\"instance_id\":\"" +
            std::string(64, 'a') + "\",\"snapshot_sha256\":\"" + std::string(64, 'f') +
            "\",\"data_binding_sha256\":\"" + std::string(64, '1') +
            "\",\"events\":{\"agent_id\":\"receiver-0\",\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"instance_id\":\"" + std::string(64, '4') +
            "\",\"source_id\":\"C0\",\"session_id\":\"" + std::string(64, '6') +
            "\",\"pid\":1234,\"available\":true,\"acquisition_events\":\"9007199254740993\",\"observed_at_unix_ms\":\"1234\"}}";
        const std::string runningLifecycle =
            "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\"" +
            std::string(64, 'a') + "\",\"snapshot_sha256\":\"" + std::string(64, 'f') +
            "\",\"data_binding_sha256\":\"" + std::string(64, '1') +
            "\",\"nodes\":[{\"agent_id\":\"receiver-0\",\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"instance_id\":\"" + std::string(64, '4') +
            "\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1234\",\"lifecycle\":{\"schema\":1,\"state\":\"ready\",\"needs_reconciliation\":false,\"plan_sha256\":\"" +
            std::string(64, '5') + "\",\"session_id\":\"" + std::string(64, '6') +
            "\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\"" +
            std::string(64, '6') + "\",\"output_directory\":\"/tmp/getdaq-scan-test\",\"pid\":1234,\"exit_code\":-1,\"reaped\":false,\"seal_verified\":false,\"received_bytes\":\"0\",\"frames\":\"0\",\"files\":\"0\"}]},\"receivers\":[{\"id\":\"C0\",\"pid\":1234,\"state\":\"running\",\"exit_code\":-1,\"signal\":0,\"message\":\"\"}]}}]}";
        const EccControlReceiverObservation runningReceiverObservation =
            DecodeEccControlReceiverObservation(runningLifecycle);
        const std::string eventJournalBeforeBadResponse = runtimeBoundScan->Current().Encode();
        std::string wrongEventSource = receiverEvents;
        const std::size_t sourceAt = wrongEventSource.find("\"source_id\":\"C0\"");
        Require(sourceAt != std::string::npos, "Receiver event source fixture is missing");
        wrongEventSource.replace(sourceAt, std::string("\"source_id\":\"C0\"").size(),
                                 "\"source_id\":\"C1\"");
        Reject([&]() { eventBaselineCoordinator.BindResponse(wrongEventSource, runningReceiverObservation); },
               "selected scan source");
        Require(runtimeBoundScan->Current().Encode() == eventJournalBeforeBadResponse,
                "Wrong-source event response changed the scan journal");
        EccControlReceiverObservation staleSessionObservation = runningReceiverObservation;
        staleSessionObservation.nodes[0].lifecycle.sessionId = std::string(64, '7');
        Reject([&]() { eventBaselineCoordinator.BindResponse(receiverEvents, staleSessionObservation); },
               "session differs");
        EccControlReceiverObservation wrongPidObservation = runningReceiverObservation;
        wrongPidObservation.nodes[0].receivers[0].pid = 4321;
        Reject([&]() { eventBaselineCoordinator.BindResponse(receiverEvents, wrongPidObservation); },
               "PID differs");
        EccControlReceiverObservation stoppedObservation = runningReceiverObservation;
        stoppedObservation.nodes[0].receivers[0].state = "exited";
        Reject([&]() { eventBaselineCoordinator.BindResponse(receiverEvents, stoppedObservation); },
               "PID differs");
        EccControlReceiverObservation startingLifecycleObservation = runningReceiverObservation;
        startingLifecycleObservation.nodes[0].lifecycle.state = "starting";
        Reject([&]() { eventBaselineCoordinator.BindResponse(receiverEvents, startingLifecycleObservation); },
               "session differs");
        EccControlReceiverObservation wrongDeploymentObservation = runningReceiverObservation;
        wrongDeploymentObservation.dataBindingSha256 = std::string(64, '9');
        Reject([&]() { eventBaselineCoordinator.BindResponse(receiverEvents, wrongDeploymentObservation); },
               "fixed ECC deployment");
        Require(runtimeBoundScan->Current().Encode() == eventJournalBeforeBadResponse,
                "Rejected receiver lifecycle/event mismatch changed the scan journal");
        const ScanExecutionJournal baselineBound =
            eventBaselineCoordinator.BindResponse(receiverEvents, runningReceiverObservation);
        Require(baselineBound.HasEventCountBaseline() &&
                baselineBound.EventCountBaseline().sourceId == "C0" &&
                baselineBound.EventCountBaseline().acquisitionEvents == UINT64_C(9007199254740993) &&
                baselineBound.EventCountBaseline().sessionId == std::string(64, '6'),
                "Validated receiver event baseline lost exact count or receiver identity");
        Require(runtimeBoundScan->Current().HasEventCountBaseline(),
                "Event-count baseline was not durably persisted");
        Reject([&]() { eventBaselineCoordinator.PrepareQuery(); },
               "no prior baseline");
        runtimeBoundScan.reset();
        const std::string runtimeInspection = InspectScanJournalWithTool(scanPath);
        Require(runtimeInspection.find("\"runtime_identity_sha256\":\"" +
                    validatedBinding.RuntimeIdentitySha256() + "\"") != std::string::npos,
                "Scan journal inspect tool omitted the persisted runtime identity digest");

        const auto completedPath = directory.File("scan-complete.json");
        directory.File("scan-complete.json.lock");
        {
            auto writer = ScanExecutionJournalStore::Create(
                completedPath, ScanExecutionJournal("scan-store-2", std::string(64, 'd'), std::string(64, 'f'), 1));
            writer->Persist(writer->Current().WithTransition(
                ScanPointState::Planned, ScanPointState::Reviewed));
            writer->Persist(writer->Current().WithOperationIntent(
                "start-2", "start-ecc", std::string(64, 'e')));
            writer->Persist(writer->Current().WithOperationComplete(
                "start-2", std::string(64, 'f'), ScanPointState::EccStarted));
        }
        auto completedScan = ScanExecutionJournalStore::Open(completedPath);
        Require(completedScan->Current().State() == ScanPointState::EccStarted &&
                !completedScan->Current().HasPendingOperation(),
                "Completed scan operation did not survive store reopen");
        completedScan.reset();

        const auto runnerSuccessPath = directory.File("scan-runner-success.json");
        directory.File("scan-runner-success.json.lock");
        auto runnerSuccessStore = ScanExecutionJournalStore::Create(
            runnerSuccessPath, ScanExecutionJournal("scan-runner-success", std::string(64, '2'), std::string(64, 'f'), 1));
        runnerSuccessStore->Persist(runnerSuccessStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanOperationRunner runner(*runnerSuccessStore);
        int successfulCalls = 0;
        const auto success = runner.Execute("runner-op-1", "start-ecc", "exact request",
            [&]() {
                ++successfulCalls;
                const auto visible = runnerSuccessStore->Current();
                Require(visible.HasPendingOperation() &&
                        visible.PendingOperationId() == "runner-op-1" &&
                        !visible.PendingOperationUnknown(),
                        "External operation ran before its durable intent");
                return std::string("confirmed response");
            });
        Require(successfulCalls == 1 && !success.outcomeUnknown &&
                success.journal.State() == ScanPointState::EccStarted &&
                !success.journal.HasPendingOperation(),
                "Successful scan operation was not recorded exactly once");
        runnerSuccessStore.reset();
        auto runnerSuccessReopened = ScanExecutionJournalStore::Open(runnerSuccessPath);
        Require(runnerSuccessReopened->Current().State() == ScanPointState::EccStarted,
                "Scan operation runner completion did not persist");
        runnerSuccessReopened.reset();

        const auto runnerPhasedPath = directory.File("scan-runner-phased.json");
        directory.File("scan-runner-phased.json.lock");
        auto runnerPhasedStore = ScanExecutionJournalStore::Create(
            runnerPhasedPath, ScanExecutionJournal("scan-runner-phased", std::string(64, '5'), std::string(64, 'f'), 1));
        runnerPhasedStore->Persist(runnerPhasedStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanOperationRunner phasedRunner(*runnerPhasedStore);
        phasedRunner.PersistIntent("runner-op-phased", "start-ecc", "async request bytes");
        Require(runnerPhasedStore->Current().HasPendingOperation() &&
                runnerPhasedStore->Current().PendingOperationId() == "runner-op-phased" &&
                !runnerPhasedStore->Current().PendingOperationUnknown(),
                "Phased runner did not durably expose intent before async submission");
        const auto phasedSuccess = phasedRunner.Complete("runner-op-phased", "async response bytes");
        Require(!phasedSuccess.outcomeUnknown &&
                phasedSuccess.journal.State() == ScanPointState::EccStarted &&
                !phasedSuccess.journal.HasPendingOperation(),
                "Phased async operation completion did not advance journal");
        runnerPhasedStore.reset();
        auto runnerPhasedReopened = ScanExecutionJournalStore::Open(runnerPhasedPath);
        Require(runnerPhasedReopened->Current().State() == ScanPointState::EccStarted,
                "Phased async runner completion did not survive reopen");
        runnerPhasedReopened.reset();

        const auto runnerUnknownPath = directory.File("scan-runner-unknown.json");
        directory.File("scan-runner-unknown.json.lock");
        auto runnerUnknownStore = ScanExecutionJournalStore::Create(
            runnerUnknownPath, ScanExecutionJournal("scan-runner-unknown", std::string(64, '3'), std::string(64, 'f'), 1));
        runnerUnknownStore->Persist(runnerUnknownStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanOperationRunner unknownRunner(*runnerUnknownStore);
        int unknownCalls = 0;
        const auto unknownResult = unknownRunner.Execute(
            "runner-op-2", "start-ecc", "exact request",
            [&]() -> std::string {
                ++unknownCalls; // 模拟外部已受理但响应丢失。
                throw std::runtime_error("reply lost");
            });
        Require(unknownCalls == 1 && unknownResult.outcomeUnknown &&
                unknownResult.journal.State() == ScanPointState::NeedsReconciliation &&
                unknownResult.journal.PendingOperationUnknown() &&
                unknownResult.journal.PendingOperationId() == "runner-op-2",
                "Ambiguous scan operation was not quarantined without replay");
        runnerUnknownStore.reset();
        auto runnerUnknownReopened = ScanExecutionJournalStore::Open(runnerUnknownPath);
        Require(runnerUnknownReopened->Current().State() == ScanPointState::NeedsReconciliation &&
                runnerUnknownReopened->Current().PendingOperationId() == "runner-op-2",
                "Unknown scan operation did not survive restart quarantine");
        Reject([&]() {
            ScanOperationRunner recovered(*runnerUnknownReopened);
            recovered.CompleteObserved("runner-op-2", "response");
        }, "limited to whole-system lifecycle");
        runnerUnknownReopened.reset();

        const auto observedRecoveryPath = directory.File("scan-runner-observed-recovery.json");
        directory.File("scan-runner-observed-recovery.json.lock");
        auto observedRecoveryStore = ScanExecutionJournalStore::Create(
            observedRecoveryPath, ScanExecutionJournal("scan-runner-observed-recovery",
                std::string(64, '6'), 1));
        observedRecoveryStore->Persist(observedRecoveryStore->Current().WithSnapshotBinding(
            std::string(64, '7')));
        observedRecoveryStore->Persist(observedRecoveryStore->Current().WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed));
        ScanOperationRunner observedRecoveryRunner(*observedRecoveryStore);
        observedRecoveryRunner.PersistIntent("observed-describe-1", "start-ecc",
                                              "start-ecc request");
        observedRecoveryRunner.Complete("observed-describe-1", "start-ecc response");
        observedRecoveryStore->Persist(observedRecoveryStore->Current().WithRuntimeIdentityBinding(
            ConfigurationSha256("verified scan identity")));
        observedRecoveryRunner.PersistIntent("observed-describe-2", "describe",
                                              "exact saved describe request");
        observedRecoveryStore.reset();
        auto observedRecoveryReopened = ScanExecutionJournalStore::Open(observedRecoveryPath);
        Require(observedRecoveryReopened->Current().PendingOperationUnknown() &&
                observedRecoveryReopened->Current().PendingRequestSha256() ==
                    ConfigurationSha256("exact saved describe request"),
                "Restart quarantine did not preserve the exact lifecycle request digest");
        ScanOperationRunner observedRecovery(*observedRecoveryReopened);
        const auto observedComplete = observedRecovery.CompleteObserved(
            "observed-describe-2", "validated exact ECC response");
        Require(!observedComplete.outcomeUnknown &&
                observedComplete.journal.State() == ScanPointState::Described &&
                !observedComplete.journal.HasPendingOperation() &&
                observedComplete.journal.Encode().find("operation-observed-complete") !=
                    std::string::npos,
                "Read-only observed lifecycle recovery did not resolve the exact quarantined intent");
        observedRecoveryReopened.reset();
        auto observedRecoveryRoundTrip = ScanExecutionJournalStore::Open(observedRecoveryPath);
        Require(observedRecoveryRoundTrip->Current().State() == ScanPointState::Described &&
                !observedRecoveryRoundTrip->Current().HasPendingOperation(),
                "Observed lifecycle recovery did not survive journal reopen");
        observedRecoveryRoundTrip.reset();

        const auto runnerCompletionFailurePath = directory.File("scan-runner-completion-failure.json");
        directory.File("scan-runner-completion-failure.json.lock");
        auto runnerCompletionFailureStore = ScanExecutionJournalStore::Create(
            runnerCompletionFailurePath, ScanExecutionJournal("scan-runner-completion-failure", std::string(64, '4'), std::string(64, 'f'), 1));
        runnerCompletionFailureStore->Persist(
            runnerCompletionFailureStore->Current().WithTransition(
                ScanPointState::Planned, ScanPointState::Reviewed));
        ScanOperationRunner completionFailureRunner(*runnerCompletionFailureStore);
        int completionFailureCalls = 0;
        Reject([&]() {
            completionFailureRunner.Execute("runner-op-3", "start-ecc", "exact request",
                [&]() {
                    ++completionFailureCalls;
                    // Simulate the external action succeeding while durable completion fails.
                    const int fd = ::open(runnerCompletionFailurePath.c_str(),
                                          O_WRONLY | O_APPEND | O_CLOEXEC);
                    Require(fd >= 0 && ::write(fd, " ", 1) == 1,
                            "Cannot inject completion persistence failure");
                    ::close(fd);
                    return std::string("confirmed response");
                });
        }, "changed outside");
        Require(completionFailureCalls == 1,
                "Completion persistence failure caused an external operation retry");
        runnerCompletionFailureStore.reset();

        const auto tamperedPath = directory.File("scan-tampered.json");
        directory.File("scan-tampered.json.lock");
        auto tamperedStore = ScanExecutionJournalStore::Create(
            tamperedPath, ScanExecutionJournal("scan-store-3", std::string(64, '1'), std::string(64, 'f'), 1));
        const int tamperFd = ::open(tamperedPath.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        Require(tamperFd >= 0 && ::write(tamperFd, " ", 1) == 1,
                "Cannot prepare external journal mutation");
        ::close(tamperFd);
        Reject([&]() {
            tamperedStore->Persist(tamperedStore->Current().WithTransition(
                ScanPointState::Planned, ScanPointState::Reviewed));
        }, "changed outside");
        Require(Read(tamperedPath).back() == ' ', "Store overwrote external journal mutation");
        tamperedStore.reset();

        const auto linkedPath = directory.File("scan-linked.json");
        directory.File("scan-linked.json.lock");
        Require(::symlink(scanPath.c_str(), linkedPath.c_str()) == 0,
                "Cannot prepare scan journal symlink test");
        Reject([&]() { ScanExecutionJournalStore::Open(linkedPath); }, "without following links");
        std::cout << "PASS ECC and scan journals: intent ordering, private atomic store, restart quarantine, tamper and link refusal\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
