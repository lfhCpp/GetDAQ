#include "eccjournal.h"
#include "eccjournal_p.h"
#include "eccjournal_replay.h"
#include "../../common/text/utf8.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace EccJournalInternal;
namespace {
void Sync(int fd) {
    int result; do { result = ::fsync(fd); } while (result != 0 && errno == EINTR);
    if (result != 0) throw std::runtime_error("Cannot fsync ECC journal: " + std::string(std::strerror(errno)));
}
}
class EccJournal::Impl {
public:
    int fd = -1;
    bool readOnly = false, faulted = false;
    std::size_t bytes = 0, records = 0;
    unsigned long long sequence = 0;
    std::string path, scope, pending;
    dev_t device = 0;
    ino_t inode = 0;
    EccStatus status;
    std::map<std::string, EccOperation> operations;
    std::vector<std::string> order;
    mutable std::mutex mutex;
    ~Impl() { if (fd >= 0) ::close(fd); }
    void Writable() const {
        if (readOnly) throw std::runtime_error("ECC journal is read-only");
        if (faulted) throw std::runtime_error("ECC journal storage fault; preserve file, do not retry writes");
    }
    void NewId(const std::string &id, const std::string &details) const {
        IdLimit(id); TextLimit(details);
        if (operations.count(id)) throw std::runtime_error("Operation ID already recorded; query its outcome instead of replaying");
        if (operations.size() >= MaxOperations) throw std::runtime_error("ECC journal operation limit reached");
    }
    EccOperation &Pending(const std::string &id) {
        if (pending != id || !operations.count(id) || operations.at(id).phase != "intent")
            throw std::runtime_error("Journal completion does not match the pending intent");
        return operations.at(id);
    }
    template<class Action> void Update(Action action) {
        try { action(); } catch (...) { faulted = true; throw; }
    }
    void Identity(std::size_t expectedBytes) const {
        struct stat descriptor, name;
        if (::fstat(fd, &descriptor) != 0 || ::lstat(path.c_str(), &name) != 0 || !S_ISREG(name.st_mode) ||
            descriptor.st_dev != device || descriptor.st_ino != inode || name.st_dev != device || name.st_ino != inode ||
            descriptor.st_size != static_cast<off_t>(expectedBytes) || name.st_size != descriptor.st_size)
            throw std::runtime_error("ECC journal file was modified/replaced/deleted; preserve logs and stop control");
    }
    std::string Prefix(const char *phase) const {
        return "{\"seq\":" + Quote(std::to_string(sequence + 1)) + ",\"phase\":" + Quote(phase);
    }
    void Append(const std::string &record, bool header = false) {
        Writable();
        if (record.size() > MaxLine || bytes > MaxBytes - record.size() - 1 || records >= MaxRecords)
            throw std::runtime_error("ECC journal size/record limit reached");
        const std::string line = record + '\n';
        std::size_t done = 0;
        try {
            Identity(bytes);
            while (done < line.size()) {
                const auto count = ::write(fd, line.data() + done, line.size() - done);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0) throw std::runtime_error("Cannot append ECC journal: " + std::string(std::strerror(errno)));
                done += static_cast<std::size_t>(count);
            }
            Sync(fd);
            Identity(bytes + line.size());
        } catch (...) { faulted = true; throw; }
        bytes += line.size(); ++records;
        if (!header) ++sequence;
    }
};
EccJournal::EccJournal(const std::string &path, const std::string &scope, EccJournalMode mode) : fImpl(new Impl)
{
    TextLimit(scope);
    if (mode != EccJournalMode::Writable && mode != EccJournalMode::ReadOnly && mode != EccJournalMode::ExclusiveReadOnly)
        throw std::invalid_argument("Invalid ECC journal mode");
    if (path.empty() || path.find('\0') != std::string::npos) throw std::invalid_argument("Invalid ECC journal path");
    fImpl->path = path; fImpl->scope = scope; fImpl->readOnly = mode != EccJournalMode::Writable;
    bool created = false;
    const int options = O_CLOEXEC | O_NOFOLLOW;
    if (!fImpl->readOnly) {
        fImpl->fd = ::open(path.c_str(), O_RDWR | O_APPEND | O_CREAT | O_EXCL | options, 0600);
        if (fImpl->fd >= 0) created = true;
        else if (errno == EEXIST) fImpl->fd = ::open(path.c_str(), O_RDWR | O_APPEND | options);
    } else fImpl->fd = ::open(path.c_str(), O_RDONLY | options);
    if (fImpl->fd < 0) throw std::runtime_error("Cannot open ECC journal: " + std::string(std::strerror(errno)));
    struct stat info;
    if (::fstat(fImpl->fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > static_cast<off_t>(MaxBytes))
        throw std::runtime_error("ECC journal must be a regular file of at most 64 MiB");
    fImpl->device = info.st_dev; fImpl->inode = info.st_ino;
    if (::flock(fImpl->fd, (mode == EccJournalMode::ReadOnly ? LOCK_SH : LOCK_EX) | LOCK_NB) != 0)
        throw std::runtime_error("ECC journal is already locked by another controller/inspection");
    if (created) {
        const auto slash = path.find_last_of('/');
        const auto parent = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0, slash);
        const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0) throw std::runtime_error("Cannot open ECC journal directory");
        try { Sync(directory); } catch (...) { ::close(directory); throw; }
        ::close(directory);
        fImpl->Append("{\"schema_version\":1,\"kind\":\"getdaq.ecc-journal\",\"phase\":\"header\",\"seq\":\"0\",\"scope\":" +
                      Quote(scope) + ",\"state\":1}", true);
    } else {
        fImpl->Identity(static_cast<std::size_t>(info.st_size));
        auto replay = EccJournalInternal::Replay(fImpl->fd, scope);
        fImpl->bytes = replay.bytes;
        fImpl->records = replay.records;
        fImpl->sequence = replay.sequence;
        fImpl->pending = std::move(replay.pending);
        fImpl->status = std::move(replay.status);
        fImpl->operations = std::move(replay.operations);
        fImpl->order = std::move(replay.order);
        fImpl->Identity(fImpl->bytes);
        if (!fImpl->readOnly) fImpl->Append(fImpl->Prefix("opened") + ",\"state\":" + std::to_string(static_cast<int>(fImpl->status.state)) + "}");
    }
}
EccJournal::~EccJournal() {}
EccStatus EccJournal::Load() const {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    auto status = fImpl->status;
    if (fImpl->faulted) status.needsReconciliation = true;
    if (!fImpl->pending.empty()) {
        status.outcomeUnknown = true;
        status.error = fImpl->operations.at(fImpl->pending).command;
        if (status.message.empty()) status.message = "Operation has no confirmed completion; use the active controller status while it is running";
    }
    return status;
}
void EccJournal::CheckNewId(const std::string &id, const std::string &details) const {
    std::lock_guard<std::mutex> lock(fImpl->mutex); fImpl->Writable(); fImpl->NewId(id, details);
}
void EccJournal::Begin(const std::string &id, EccCommand command, EccState from, EccState to, const std::string &requestKey) {
    std::lock_guard<std::mutex> lock(fImpl->mutex); fImpl->Writable(); fImpl->NewId(id, requestKey);
    if (!fImpl->pending.empty() || fImpl->status.outcomeUnknown || fImpl->status.needsReconciliation ||
        fImpl->status.state != from || EccStateMachine::Target(from, command) != to)
        throw std::runtime_error("ECC journal cannot begin this transition");
    fImpl->Append(fImpl->Prefix("intent") + ",\"id\":" + Quote(id) + ",\"command\":" + std::to_string(static_cast<int>(command)) +
        ",\"from\":" + std::to_string(static_cast<int>(from)) + ",\"to\":" + std::to_string(static_cast<int>(to)) + ",\"request_key\":" + Quote(requestKey) + "}");
    fImpl->Update([&]() {
        EccOperation op; op.id = id; op.command = static_cast<int>(command); op.from = from; op.to = to; op.requestKey = requestKey; op.phase = "intent";
        fImpl->operations.emplace(id, op); fImpl->order.push_back(id); fImpl->pending = id; fImpl->status.operationId = id;
    });
}
void EccJournal::Complete(const std::string &id) {
    std::lock_guard<std::mutex> lock(fImpl->mutex); auto &op = fImpl->Pending(id);
    if (fImpl->status.needsReconciliation || fImpl->status.outcomeUnknown) throw std::runtime_error("Journal requires reconciliation; cannot invent completion");
    fImpl->Append(fImpl->Prefix("complete") + ",\"id\":" + Quote(id) + "}");
    fImpl->Update([&]() { op.phase = "complete"; fImpl->status.state = op.to; fImpl->status.error = 0; fImpl->status.message.clear(); fImpl->pending.clear(); });
}
void EccJournal::Unknown(const std::string &id, const std::string &message) {
    std::lock_guard<std::mutex> lock(fImpl->mutex); const auto bounded = EccErrorText(message); TextLimit(bounded); auto &op = fImpl->Pending(id);
    if (fImpl->status.needsReconciliation || fImpl->status.outcomeUnknown) throw std::runtime_error("Journal requires reconciliation; cannot append an old outcome");
    fImpl->Append(fImpl->Prefix("unknown") + ",\"id\":" + Quote(id) + ",\"message\":" + Quote(bounded) + "}");
    fImpl->Update([&]() { op.phase = "unknown"; op.message = bounded; fImpl->status.outcomeUnknown = true; fImpl->status.error = op.command; fImpl->status.message = bounded; });
}
void EccJournal::Held(const std::string &id,const std::string &evidence) {
    std::lock_guard<std::mutex> lock(fImpl->mutex);fImpl->Writable();TextLimit(evidence);
    const auto found=fImpl->operations.find(id);
    if(id!=fImpl->status.operationId || evidence.empty() || found==fImpl->operations.end() ||
       (found->second.phase!="complete" && found->second.phase!="unknown"))
        throw std::logic_error("ECC journal external hold requires the same completed/unknown original operation");
    fImpl->Append(fImpl->Prefix("held")+",\"id\":"+Quote(id)+",\"state\":"+std::to_string(int(fImpl->status.state))+
        ",\"evidence\":"+Quote(evidence)+"}");
    fImpl->Update([&]() {fImpl->status.outcomeUnknown=true;fImpl->status.needsReconciliation=true;
        fImpl->status.message="Original operation held for external reconciliation";});
}
void EccJournal::Reconciled(const std::string &id, EccState state, const std::string &evidence) {
    std::lock_guard<std::mutex> lock(fImpl->mutex); fImpl->Writable(); fImpl->NewId(id, evidence);
    if (state < EccState::Idle || state > EccState::Paused || (fImpl->pending.empty() && !fImpl->status.needsReconciliation))
        throw std::runtime_error("ECC journal has no valid reconciliation to record");
    fImpl->Append(fImpl->Prefix("reconciled") + ",\"id\":" + Quote(id) + ",\"from\":" + std::to_string(static_cast<int>(fImpl->status.state)) +
        ",\"to\":" + std::to_string(static_cast<int>(state)) + ",\"evidence\":" + Quote(evidence) + "}");
    fImpl->Update([&]() {
        EccOperation op; op.id = id; op.command = 10; op.phase = "reconciled"; op.from = fImpl->status.state; op.to = state; op.evidence = evidence;
        fImpl->operations.emplace(id, op); fImpl->order.push_back(id); fImpl->pending.clear();
        fImpl->status.state = state; fImpl->status.error = 0; fImpl->status.outcomeUnknown = false; fImpl->status.needsReconciliation = false;
        fImpl->status.operationId = id; fImpl->status.message.clear();
    });
}
std::vector<EccOperation> EccJournal::Operations() const {
    std::lock_guard<std::mutex> lock(fImpl->mutex); std::vector<EccOperation> result; result.reserve(fImpl->order.size());
    for (const auto &id : fImpl->order) result.push_back(fImpl->operations.at(id));
    return result;
}
EccOperation EccJournal::Operation(const std::string &id) const {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    const auto found = fImpl->operations.find(id);
    if (found == fImpl->operations.end()) throw std::runtime_error("Operation ID is not recorded");
    return found->second;
}
}
