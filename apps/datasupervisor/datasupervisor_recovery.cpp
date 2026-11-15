#include "datasupervisor_p.h"
#include "systemexperimentconfiguration.h"

#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
using namespace DataSupervisorInternal;
using namespace SupervisorIO;
using namespace ReceiverProcessInternal;
using Clock=std::chrono::steady_clock;

void DataSupervisor::AbortFailedSession(const std::string &path,const std::string &expectedSession,const std::string &parentFailure) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock);
    if(!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running; failed-session abort was not queued");
    Digest(expectedSession);p.CheckStore();
    if(!parentFailure.empty()) SupervisorOperationId(parentFailure);
    if(expectedSession!=p.session || p.session.empty() || p.startupFailed || p.failedAbortAttempted ||
       p.children.size()!=p.options.sources.size() || p.children.empty() ||
       (p.snapshot.state!=DataSupervisorStatus::Ready && p.snapshot.state!=DataSupervisorStatus::Failed))
        throw std::logic_error("AbortFailedSession requires the same retained Ready/Failed session, a completed startup and no prior abort attempt");
    // Authenticated original parent may declare a global failure while this
    // host's sibling receivers are still healthy. This is not hardware proof.
    bool failed=p.snapshot.state==DataSupervisorStatus::Failed || !parentFailure.empty();
    for(std::size_t i=0;i<p.children.size();++i) {
        auto &child=*p.children[i];
        if(child.status.pid<=0 || child.status.id!=p.options.sources[i].id || child.status.sessionId!=expectedSession)
            throw std::runtime_error("Failed-session child/source identity differs; no signal issued");
        child.directory->Check();const auto observation=Observe(child.status);
        if(observation.state=="unavailable" || observation.state=="not-started")
            throw std::runtime_error("Failed-session child ownership unavailable; no PID adoption");
        if((observation.state=="exited" || observation.state=="reaped") && observation.exitCode>0) failed=true;
        if(observation.state=="running") p.LockHeld(child,false);
    }
    if(!failed) throw std::logic_error("AbortFailedSession refuses a healthy or normally completed session; use the normal Finish lifecycle");
    p.failedAbortAudit.reset(new Audit(path)); Audit &audit=*p.failedAbortAudit;
    p.failedAbortAttempted=true; p.failedAbortParent=parentFailure;
    try {
        p.Record("abort-failed-session-intent","\"operation_audit\":"+EncodeJsonString(path)+",\"expected_session_id\":"+EncodeJsonString(expectedSession)+
                 ",\"parent_failure_operation_id\":"+EncodeJsonString(parentFailure)+",\"parent_failure_independently_verified\":false,\"acquisition_sealed\":false,\"hardware_stopped_verified\":false,\"automatic_retry\":false");
        // Signal all still-owned children before waiting for any one of them.
        for(auto &owned:p.children) {
            auto &child=*owned;if(child.status.reaped || Exited(child.status.pid)) continue;
            p.LockHeld(child,false);
            const auto fields="\"source_id\":"+EncodeJsonString(child.status.id)+",\"pid\":"+std::to_string(child.status.pid);
            audit.Row("{\"phase\":\"signal-intent\","+fields+",\"signal\":\"SIGTERM\"}");p.Record("failed-session-signal-intent",fields);
            if(::kill(child.status.pid,SIGTERM) && !(errno==ESRCH && Exited(child.status.pid)))
                throw std::runtime_error("Failed-session signal failed; preserve intent and do not replay");
        }
        const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.finishTimeoutMs);
        for(auto &owned:p.children) {
            auto &child=*owned;if(child.status.reaped) continue;
            while(!Exited(child.status.pid)) {
                if(Clock::now()>=deadline) throw std::runtime_error("Failed-session exit timed out; no SIGKILL or signal replay");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            child.status.exitCode=Reap(child.status.pid);child.status.reaped=true;
            const auto fields="\"source_id\":"+EncodeJsonString(child.status.id)+",\"pid\":"+std::to_string(child.status.pid)+",\"exit_code\":"+std::to_string(child.status.exitCode);
            audit.Row("{\"phase\":\"receiver-reaped\","+fields+"}");p.Record("failed-session-receiver-reaped",fields);
        }
        audit.Row("{\"phase\":\"aborted\",\"acquisition_sealed\":false,\"raw_files_deleted\":false,\"hardware_stopped_verified\":false,\"reconciliation_required\":true}");
        p.Record("aborted-failed-session-reconciliation-required");audit.Check();
        p.CheckRetainedStoppedChildren(); p.failedAbortComplete=true;
        {std::lock_guard<std::mutex> state(p.statusMutex);p.snapshot.needsReconciliation=true;}
        p.Publish(DataSupervisorStatus::Aborted,"Failed session receiver processes stopped; raw data remains incomplete and hardware reconciliation is still required");
    } catch(const std::exception &e) {
        try {p.Record("failed-session-abort-failed","\"message\":"+EncodeJsonString(e.what())+",\"signal_replay_allowed\":false");} catch(...) {}
        {std::lock_guard<std::mutex> state(p.statusMutex);p.snapshot.needsReconciliation=true;}
        p.Publish(DataSupervisorStatus::Aborted,e.what());throw;
    }
}
void DataSupervisor::RetireFailedSession(const std::string &path,const std::string &expectedSession,const std::string &parentFailure) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running; retirement was not queued");
    Digest(expectedSession); if (!parentFailure.empty()) SupervisorOperationId(parentFailure); p.CheckStore();
    if (p.snapshot.state!=DataSupervisorStatus::Aborted || !p.snapshot.needsReconciliation ||
        expectedSession!=p.session || parentFailure!=p.failedAbortParent || p.startupFailed ||
        !p.failedAbortComplete || !p.failedAbortAudit || p.retirementAttempted ||
        p.children.empty() || p.children.size()!=p.options.sources.size())
        throw std::logic_error("Retirement requires the same completed failed-session abort and original parent binding");
    try { p.failedAbortAudit->Check(); }
    catch (...) {
        p.storageFault=true;
        { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=true; }
        p.Publish(DataSupervisorStatus::Failed,"Original abort audit changed; restoration cannot permit retirement"); throw;
    }
    p.CheckRetainedStoppedChildren();
    p.retirementAudit.reset(new Audit(path)); p.retirementAttempted=true;
    try {
        const auto fields="\"expected_session_id\":"+EncodeJsonString(expectedSession)+
            ",\"parent_failure_operation_id\":"+EncodeJsonString(parentFailure)+
            ",\"raw_files_deleted\":false,\"acquisition_sealed\":false,\"hardware_stopped_verified\":false";
        p.retirementAudit->Row("{\"phase\":\"intent\","+fields+"}");
        p.Record("retire-failed-session-intent","\"operation_audit\":"+EncodeJsonString(path)+","+fields);
        p.failedAbortAudit->Check(); p.CheckRetainedStoppedChildren(); p.CheckStore();
        p.retirementAudit->Row("{\"phase\":\"retired\","+fields+"}");
        p.Record("failed-session-retired",fields); p.retirementAudit->Check();
        p.failedAbortAudit->Check(); p.CheckRetainedStoppedChildren();
        { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=false; }
        p.Publish(DataSupervisorStatus::Retired,"Original failed data session retired; old files remain incomplete, hardware/Run recovery is separate");
    } catch (const std::exception &e) {
        try { p.Record("failed-session-retirement-failed","\"message\":"+EncodeJsonString(e.what())); } catch (...) {}
        { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=true; }
        p.Publish(DataSupervisorStatus::Aborted,e.what()); throw;
    }
}
void DataSupervisor::AbortStartup(const std::string &path) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    p.CheckStore();
    if (p.snapshot.needsReconciliation || p.snapshot.state!=DataSupervisorStatus::Failed || !p.startupFailed || p.abortAttempted)
        throw std::logic_error("AbortStartup requires the same failed startup object with no prior abort attempt");
    // 全部仍活着的子进程先证明从未接入数据；不能把断开后的connected=false当作空启动。
    for (std::size_t i=0;i<p.children.size();++i) {
        auto &c=*p.children[i]; if (!c.status.pid || c.status.reaped || Exited(c.status.pid)) continue;
        const auto status=p.ReadyStatus(i);
        if (Boolean(status.At("connected")) || Unsigned(status.At("connections")) || Unsigned(status.At("received_bytes"))) throw std::runtime_error("AbortStartup refuses a receiver that accepted data/connection");
    }
    Audit audit(path); p.abortAttempted=true;
    try {
    p.Record("abort-startup-intent","\"operation_audit\":"+EncodeJsonString(path));
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.finishTimeoutMs);
    for (std::size_t i=0;i<p.children.size();++i) {
        auto &c=*p.children[i]; if (!c.status.pid || c.status.reaped) continue;
        if (!Exited(c.status.pid)) {
            const auto status=p.ReadyStatus(i);
            if (Boolean(status.At("connected")) || Unsigned(status.At("connections")) || Unsigned(status.At("received_bytes")))
                throw std::runtime_error("Receiver accepted a connection before explicit startup abort; no signal issued");
            const auto fields="\"source_id\":"+EncodeJsonString(c.status.id)+",\"pid\":"+std::to_string(c.status.pid);
            if(p.options.sources[i].inputProtocol=="FDT") {
                audit.Row("{\"phase\":\"cycle-startup-abort-intent\","+fields+"}");p.Record("cycle-startup-abort-intent",fields);
                SendCycleControl(c.cycleControl.value,'A');
            } else {
                audit.Row("{\"phase\":\"signal-intent\","+fields+",\"signal\":\"SIGTERM\"}"); p.Record("abort-signal-intent",fields);
                if (::kill(c.status.pid,SIGTERM)) throw std::runtime_error("Cannot signal confirmed empty startup receiver; do not replay");
            }
        }
        while (!Exited(c.status.pid)) { if (Clock::now()>=deadline) throw std::runtime_error("Startup abort exit timed out; no SIGKILL/retry"); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
        c.status.exitCode=Reap(c.status.pid); c.status.reaped=true;
        p.Record("abort-receiver-exit","\"source_id\":"+EncodeJsonString(c.status.id)+",\"pid\":"+std::to_string(c.status.pid)+",\"exit_code\":"+std::to_string(c.status.exitCode));
        const auto out=ReadFile(c.log+".stdout",65536);
        if (c.status.exitCode==2 && out.empty() && !c.lockBound) continue; // 已绑定的可信程序在Run前构造失败。
        const auto last=out.find_last_not_of("\r\n"),start=last==std::string::npos ? std::string::npos : out.rfind('\n',last);
        const bool cycle=p.options.sources[i].inputProtocol=="FDT";
        if (last==std::string::npos || c.status.exitCode!=((cycle || p.options.sources[i].inputProtocol=="ICE") ? 1 : 0)) throw std::runtime_error("Startup abort cannot prove an empty receiver exit");
        const auto status=ParseStrictJson(out.substr(start==std::string::npos ? 0 : start+1,last-(start==std::string::npos ? 0 : start+1)+1));
        p.Identity(status,c,p.options.sources[i]);
        if(cycle && (!Boolean(status.At("cycle_startup_aborted")) || String(status.At("error"))!="Cycle startup aborted before sender connection"))
            throw std::runtime_error("Cycle startup abort lacks actual no-connection acknowledgement");
        if (p.options.sources[i].inputProtocol=="ICE" && (Boolean(status.At("sender_end_confirmed")) || Boolean(status.At("completion_receipt_published")) ||
            String(status.At("error"))!="ICE input interrupted; no confirmed source stream end")) throw std::runtime_error("ICE startup abort lacks explicit incomplete zero-source exit");
        if (Unsigned(status.At("connections")) || Unsigned(status.At("received_bytes")) || Boolean(status.At("sender_eof_confirmed")))
            throw std::runtime_error("Connection raced with startup abort; acquisition outcome remains unconfirmed");
    }
    audit.Row("{\"phase\":\"aborted\",\"acquisition_sealed\":false,\"reconciliation_required\":true}"); p.Record("aborted-startup-reconciliation-required"); audit.Check();
    { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=true; }
    p.Publish(DataSupervisorStatus::Aborted);
    } catch (const std::exception &e) {
        try { p.Record("abort-failed","\"message\":"+EncodeJsonString(e.what())+",\"signal_replay_allowed\":false"); } catch (...) {}
        { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=true; }
        p.Publish(DataSupervisorStatus::Aborted,e.what()); throw;
    }
}
}
