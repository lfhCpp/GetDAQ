#include "eccjournal_replay.h"
#include "eccjournal_p.h"
#include "strictjson.h"
#include <cerrno>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace EccJournalInternal {
namespace {

void NewId(const ReplayState &state, const std::string &id, const std::string &details)
{
    IdLimit(id);
    TextLimit(details);
    if (state.operations.count(id))
        throw std::runtime_error("Operation ID already recorded; query its outcome instead of replaying");
    if (state.operations.size() >= MaxOperations)
        throw std::runtime_error("ECC journal operation limit reached");
}

EccOperation &Pending(ReplayState &state, const std::string &id)
{
    if (state.pending != id || !state.operations.count(id) ||
        state.operations.at(id).phase != "intent")
        throw std::runtime_error("Journal completion does not match the pending intent");
    return state.operations.at(id);
}

void Parse(ReplayState &state, const std::string &scope, const std::string &line)
{
    if (line.empty() || line.size() > MaxLine || line.front() != '{' || line.back() != '}')
        throw std::runtime_error("Invalid/truncated ECC journal record");
    const auto root = ParseStrictJson(line);
    const auto phase = Text(root.At("phase"));
    const auto seq = Number(root.At("seq"), true);
    if (!state.records) {
        Keys(root, {"schema_version", "kind", "phase", "seq", "scope", "state"});
        if (Number(root.At("schema_version")) != 1 ||
            Text(root.At("kind")) != "getdaq.ecc-journal" || phase != "header" || seq != 0 ||
            State(root.At("state")) != EccState::Idle)
            throw std::runtime_error("Unsupported ECC journal header");
        if (Text(root.At("scope")) != scope)
            throw std::runtime_error("ECC journal scope differs from the requested targets/deployment");
    } else {
        if (seq != state.sequence + 1)
            throw std::runtime_error("ECC journal sequence is not continuous");
        if (phase == "opened") {
            Keys(root, {"seq", "phase", "state"});
            if (State(root.At("state")) != state.status.state)
                throw std::runtime_error("Journal reopened state mismatch");
            state.status.needsReconciliation = true;
        } else if (phase == "intent") {
            Keys(root, {"seq", "phase", "id", "command", "from", "to", "request_key"});
            EccOperation op;
            op.id = Text(root.At("id"));
            op.requestKey = Text(root.At("request_key"));
            NewId(state, op.id, op.requestKey);
            const auto command = Number(root.At("command"));
            if (command < 1 ||
                (command > 9 && command != 11 && command != 12 && command != 13 &&
                 command != 14 && command != 15 && command != 16 && command != 17) ||
                !state.pending.empty() || state.status.outcomeUnknown ||
                state.status.needsReconciliation)
                throw std::runtime_error("Journal intent violates operation/state constraints");
            op.command = static_cast<int>(command);
            op.from = State(root.At("from"));
            op.to = State(root.At("to"));
            if (op.from != state.status.state ||
                EccStateMachine::Target(op.from, static_cast<EccCommand>(command)) != op.to)
                throw std::runtime_error("Journal intent transition mismatch");
            op.phase = phase;
            state.pending = op.id;
            state.status.operationId = op.id;
            state.operations.emplace(op.id, op);
            state.order.push_back(op.id);
        } else if (phase == "complete" || phase == "unknown") {
            if (phase == "complete") Keys(root, {"seq", "phase", "id"});
            else Keys(root, {"seq", "phase", "id", "message"});
            auto &op = Pending(state, Text(root.At("id")));
            op.phase = phase;
            if (phase == "complete") {
                state.status.state = op.to;
                state.status.error = 0;
                state.status.message.clear();
                state.pending.clear();
            } else {
                op.message = Text(root.At("message"));
                TextLimit(op.message);
                state.status.outcomeUnknown = true;
                state.status.error = op.command;
                state.status.message = op.message;
            }
        } else if (phase == "held") {
            Keys(root, {"seq", "phase", "id", "state", "evidence"});
            const auto id = Text(root.At("id"));
            const auto evidence = Text(root.At("evidence"));
            TextLimit(evidence);
            const auto found = state.operations.find(id);
            if (id != state.status.operationId || evidence.empty() ||
                found == state.operations.end() ||
                (found->second.phase != "complete" && found->second.phase != "unknown") ||
                State(root.At("state")) != state.status.state)
                throw std::runtime_error("Journal hold lacks matching original operation/state");
            state.status.outcomeUnknown = true;
            state.status.needsReconciliation = true;
            state.status.message = "Original operation held for external reconciliation";
        } else if (phase == "reconciled") {
            Keys(root, {"seq", "phase", "id", "from", "to", "evidence"});
            EccOperation op;
            op.id = Text(root.At("id"));
            op.evidence = Text(root.At("evidence"));
            NewId(state, op.id, op.evidence);
            op.from = State(root.At("from"));
            op.to = State(root.At("to"));
            op.command = 10;
            op.phase = phase;
            if (op.from != state.status.state ||
                (state.pending.empty() && !state.status.needsReconciliation))
                throw std::runtime_error("Journal reconciliation has no matching recovery need");
            state.operations.emplace(op.id, op);
            state.order.push_back(op.id);
            state.pending.clear();
            state.status.state = op.to;
            state.status.error = 0;
            state.status.outcomeUnknown = false;
            state.status.needsReconciliation = false;
            state.status.operationId = op.id;
            state.status.message.clear();
        } else {
            throw std::runtime_error("Unknown ECC journal phase");
        }
        state.sequence = seq;
    }
    if (++state.records > MaxRecords)
        throw std::runtime_error("Excessive ECC journal records");
}

} // namespace

ReplayState Replay(int fd, const std::string &scope)
{
    ReplayState state;
    if (::lseek(fd, 0, SEEK_SET) < 0)
        throw std::runtime_error("Cannot seek ECC journal");
    std::string line;
    char block[8192];
    while (true) {
        const auto count = ::read(fd, block, sizeof(block));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::runtime_error("Cannot read ECC journal");
        if (!count) break;
        state.bytes += static_cast<std::size_t>(count);
        if (state.bytes > MaxBytes) throw std::runtime_error("ECC journal exceeds 64 MiB");
        for (ssize_t i = 0; i < count; ++i) {
            if (block[i] == '\n') {
                try {
                    Parse(state, scope, line);
                } catch (const std::exception &error) {
                    throw std::runtime_error("ECC journal record " +
                        std::to_string(state.records + 1) + ": " + error.what());
                }
                line.clear();
            } else {
                line += block[i];
                if (line.size() > MaxLine)
                    throw std::runtime_error("Excessive ECC journal line");
            }
        }
    }
    if (!line.empty() || !state.records)
        throw std::runtime_error("ECC journal is empty/truncated; preserve it for inspection");
    if (!state.pending.empty()) {
        state.status.outcomeUnknown = true;
        state.status.error = state.operations.at(state.pending).command;
        if (state.status.message.empty())
            state.status.message = "Operation intent has no confirmed completion; do not replay";
    }
    // A journal proves history, not that hardware remained unchanged after restart.
    state.status.needsReconciliation = true;
    return state;
}

} // namespace EccJournalInternal
} // namespace GetDAQ
