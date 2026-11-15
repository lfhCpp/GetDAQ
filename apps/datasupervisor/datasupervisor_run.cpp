#include "datasupervisor_p.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace GetDAQ {
using namespace DataSupervisorInternal;
using namespace SupervisorIO;
using namespace ReceiverProcessInternal;
using Clock=std::chrono::steady_clock;

void DataSupervisor::Validate() const {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore(); p.CheckRetirement(); ValidateOptions(p.options);
}
void DataSupervisor::VerifyStopped() const {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore();
    if (p.snapshot.state==DataSupervisorStatus::Retired) { p.CheckRetirement(); return; }
    if (p.snapshot.state!=DataSupervisorStatus::Fresh && p.snapshot.state!=DataSupervisorStatus::Stopped) throw std::runtime_error("Managed receivers are not confirmed stopped");
    for (const auto &c:p.children) if (!c->status.reaped || !c->status.sealVerified) throw std::runtime_error("Managed receiver stop/seal is unconfirmed");
}
void DataSupervisor::VerifyControlHandoff() const {
    auto &p=*fImpl;
    std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore();
    if (p.snapshot.state!=DataSupervisorStatus::Stopped || p.children.size()!=p.options.sources.size() || p.children.empty())
        throw std::runtime_error("Control handoff requires the original normally stopped receiver session");
    try {
        for (std::size_t i=0;i<p.children.size();++i) {
            const auto expected=p.children[i]->status;
            if (!expected.reaped || !expected.sealVerified || expected.exitCode!=0)
                throw std::runtime_error("Control handoff requires confirmed normal receiver exit and seal");
            p.Seal(i,expected.sourceUnusedConfirmed);
            const auto &actual=p.children[i]->status;
            if (actual.receivedBytes!=expected.receivedBytes || actual.frames!=expected.frames || actual.files!=expected.files ||
                actual.sourceEndConfirmed!=expected.sourceEndConfirmed || actual.sourceUnusedConfirmed!=expected.sourceUnusedConfirmed)
                throw std::runtime_error("Original receiver seal/counters changed before control handoff");
        }
        p.CheckStore();
    } catch (...) {
        std::lock_guard<std::mutex> state(p.statusMutex);
        p.snapshot.needsReconciliation=true;
        p.snapshot.state=DataSupervisorStatus::Failed;
        p.snapshot.message="Original completed receiver proof changed; control handoff remains held";
        throw;
    }
}
void DataSupervisor::Start(const std::string &path) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore(); p.CheckRetirement(); ValidateOptions(p.options);
    if (p.snapshot.state!=DataSupervisorStatus::Fresh && p.snapshot.state!=DataSupervisorStatus::Stopped && p.snapshot.state!=DataSupervisorStatus::Retired) throw std::logic_error("Supervisor Start requires Fresh/Stopped/Retired");
    // Reject an already-unsuitable output volume before publishing Start intent,
    // creating a run directory, or spawning a receiver. The receiver rechecks
    // its own volume immediately before opening output files to cover races.
    for (const auto &source:p.options.sources) CheckOutputReserve(source);
    Audit audit(path); p.session=RandomSession(); p.children.clear(); p.startupFailed=false; p.failedAbortAttempted=false;
    p.failedAbortComplete=false; p.retirementAttempted=false; p.failedAbortParent.clear(); p.failedAbortAudit.reset(); p.retirementAudit.reset();
    p.Publish(DataSupervisorStatus::Starting);
    try {
        audit.Row("{\"schema\":1,\"phase\":\"intent\",\"plan_sha256\":"+EncodeJsonString(p.planHash)+",\"session_id\":"+EncodeJsonString(p.session)+"}");
        p.Record("start-intent","\"operation_audit\":"+EncodeJsonString(path));
        const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.startupTimeoutMs);
        for (std::size_t i=0;i<p.options.sources.size();++i) {
            const auto &source=p.options.sources[i]; std::unique_ptr<Impl::Child> child(new Impl::Child);
            child->status.id=source.id; child->status.sessionId=p.session; child->status.outputDirectory=source.outputRoot+"/getdaq-"+p.session+"-source-"+std::to_string(i);
            child->log=path+"/source-"+std::to_string(i); CreateDirectory(child->status.outputDirectory); child->directory.reset(new Directory(child->status.outputDirectory));
            WriteNew(child->status.outputDirectory+"/.getdaq-managed-session.json","{\"session_id\":"+EncodeJsonString(p.session)+",\"plan_sha256\":"+EncodeJsonString(p.planHash)+"}\n");
            p.children.push_back(std::move(child)); auto &c=*p.children.back();
            const auto fields="\"source_id\":"+EncodeJsonString(source.id)+",\"output_directory\":"+EncodeJsonString(c.status.outputDirectory);
            p.CheckStore(); audit.Row("{\"phase\":\"spawn-intent\","+fields+"}"); p.Record("spawn-intent",fields);
            Fd cycleRead;
            if(source.inputProtocol=="FDT") CycleControl(cycleRead,c.cycleControl);
            c.status.pid=Spawn(p.Executable(i),p.Arguments(i,c),c.log,cycleRead.value); p.Publish(DataSupervisorStatus::Starting);
            const auto spawned=fields+",\"pid\":"+std::to_string(c.status.pid); audit.Row("{\"phase\":\"spawned\","+spawned+"}"); p.Record("spawned",spawned);
            for (;;) {
                if (Exited(c.status.pid)) throw std::runtime_error("Receiver exited during startup; preserve PID/logs, do not retry");
                try { p.ReadyStatus(i,true); break; } catch (const std::exception &) { if (Clock::now()>=deadline) throw; }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        p.CheckStore(); for (std::size_t i=0;i<p.children.size();++i) p.ReadyStatus(i);
        audit.Row("{\"phase\":\"ready\",\"raw_data_received_by_supervisor\":false}"); p.Record("ready"); audit.Check(); p.Publish(DataSupervisorStatus::Ready);
    } catch (const std::exception &e) {
        p.startupFailed=true;
        try { p.Record("failed","\"message\":"+EncodeJsonString(e.what())+",\"automatic_retry\":false,\"automatic_cleanup\":false"); } catch (...) {}
        p.Publish(DataSupervisorStatus::Failed,e.what()); throw;
    }
}
void DataSupervisor::VerifyReady() const {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore();
    if (p.snapshot.state!=DataSupervisorStatus::Ready || p.children.size()!=p.options.sources.size()) throw std::logic_error("Supervisor is not Ready");
    for (std::size_t i=0;i<p.children.size();++i) p.ReadyStatus(i);
}
void DataSupervisor::Finish(const std::string &path,const std::vector<std::string> &unusedSourceIds) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.Eligible(); p.CheckStore(); if (p.snapshot.state!=DataSupervisorStatus::Ready) throw std::logic_error("Supervisor Finish requires Ready");
    std::set<std::string> unused;
    for (const auto &id:unusedSourceIds) {
        if (!unused.insert(id).second) throw std::invalid_argument("Duplicate unused source ID");
        const auto source=std::find_if(p.options.sources.begin(),p.options.sources.end(),[&](const ReceiverSourcePlan &s) {return s.id==id;});
        if (source==p.options.sources.end() || source->inputProtocol!="FDT") throw std::invalid_argument("Unused source requires an existing FDT receiver");
        const auto i=std::size_t(source-p.options.sources.begin());const auto status=p.ReadyStatus(i);
        if (Unsigned(status.At("connections")) || Unsigned(status.At("received_bytes")) || Unsigned(status.At("frames")) || Unsigned(status.At("files")) || Boolean(status.At("connected")))
            throw std::runtime_error("Unused source already accepted a connection/data");
    }
    Audit audit(path); p.Publish(DataSupervisorStatus::Finishing);
    try {
        const bool ice=std::any_of(p.options.sources.begin(),p.options.sources.end(),[](const ReceiverSourcePlan &s) {return s.inputProtocol=="ICE";});
        audit.Row(ice ? "{\"phase\":\"finish-intent\",\"wait_for_source_boundaries\":true,\"ice_source_end_required\":true,\"signal_sent\":false}" :
            "{\"phase\":\"finish-intent\",\"wait_for_sender_eof\":true,\"signal_sent\":false}"); p.Record("finish-intent","\"operation_audit\":"+EncodeJsonString(path));
        const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.finishTimeoutMs); std::string failure;
        // 先向所有FDT子进程提交持久意图和私有请求，再等任何一个退出。
        for (std::size_t i=0;i<p.children.size();++i) if (p.options.sources[i].inputProtocol=="FDT") {
            auto &c=*p.children[i];
            try {
                if (Exited(c.status.pid)) continue;
                p.ReadyStatus(i);
                const auto fields="\"source_id\":"+EncodeJsonString(c.status.id)+",\"pid\":"+std::to_string(c.status.pid)+",\"control\":\"stdin-socket-v1\",\"sender_eof_is_required\":"+
                    (unused.count(c.status.id) ? "false,\"unused_source_requested\":true" : "true");
                audit.Row("{\"phase\":\"cycle-finish-intent\","+fields+"}");p.Record("cycle-finish-intent",fields);
                SendCycleControl(c.cycleControl.value,unused.count(c.status.id) ? 'E' : 'F');
                audit.Row("{\"phase\":\"cycle-finish-requested\","+fields+"}");
            } catch (const std::exception &e) {if(failure.empty()) failure=e.what();}
        }
        for (std::size_t i=0;i<p.children.size();++i) {
            auto &c=*p.children[i];
            try {
                while (!Exited(c.status.pid)) {
                    if (Clock::now()>=deadline) throw std::runtime_error("Receiver EOF/drain/exit timed out; no signal/restart issued");
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                c.status.exitCode=Reap(c.status.pid); c.status.reaped=true;
                p.Record("receiver-exit","\"source_id\":"+EncodeJsonString(c.status.id)+",\"pid\":"+std::to_string(c.status.pid)+",\"exit_code\":"+std::to_string(c.status.exitCode));
                if (c.status.exitCode!=0) throw std::runtime_error("Receiver abnormal exit; seal cannot commit the data cycle");
                p.Seal(i,unused.count(c.status.id)!=0); p.Record("receiver-seal-verified","\"source_id\":"+EncodeJsonString(c.status.id)+",\"received_bytes\":"+EncodeJsonString(std::to_string(c.status.receivedBytes)));
            } catch (const std::exception &e) { if (failure.empty()) failure=e.what(); }
            p.Publish(DataSupervisorStatus::Finishing);
        }
        if (!failure.empty()) throw std::runtime_error(failure);
        p.CheckStore(); audit.Row("{\"phase\":\"complete\",\"all_owned_processes_reaped\":true,\"all_seals_verified\":true,\"raw_data_received_by_supervisor\":false}");
        p.Record("stopped-all-seals-verified"); audit.Check(); p.Publish(DataSupervisorStatus::Stopped);
    } catch (const std::exception &e) {
        try { p.Record("failed","\"message\":"+EncodeJsonString(e.what())+",\"automatic_retry\":false,\"signal_sent\":false"); } catch (...) {}
        p.Publish(DataSupervisorStatus::Failed,e.what()); throw;
    }
}

}
