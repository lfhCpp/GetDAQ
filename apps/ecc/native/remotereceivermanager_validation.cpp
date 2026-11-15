#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::Impl::Deadline(const Clock::time_point &deadline) const {
    if (Clock::now()>=deadline) throw std::runtime_error("Remote receiver command deadline expired; no operation resend");
}

void RemoteReceiverManager::Impl::Identity(std::size_t index,const Clock::time_point &deadline) {
    Deadline(deadline); CheckRoot(); auto &peer=*peers[index]; const auto v=Response(peer.client.Identity(),200);
    ExactSupervisorFields(v,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","plan"});
    if (Unsigned(v.At("schema"))!=1 || String(v.At("service"))!="getdaq-supervisor" || String(v.At("agent_id"))!=peer.endpoint.agentId ||
        String(v.At("controller_id"))!=options.controllerId || String(v.At("plan_sha256"))!=peer.endpoint.planSha256) throw std::runtime_error("Remote supervisor identity/controller/plan differs");
    const auto instance=String(v.At("instance_id")); Digest(instance); const auto plan=DecodeSupervisorPlan(v.At("plan"));
    if (ConfigurationSha256(EncodeOptions(plan.options,plan.executableSha256))!=peer.endpoint.planSha256) throw std::runtime_error("Remote supervisor plan bytes differ");
    if (!peer.instance.empty() && peer.instance!=instance) { Latch("Remote supervisor instance changed; verified recovery required"); throw std::runtime_error("Remote supervisor instance changed"); }
    peer.instance=instance; peer.plan=plan; peer.hasPlan=true; Publish(index); CheckRoot(); Deadline(deadline);
}

void RemoteReceiverManager::Impl::Match(const std::vector<DataSenderOptions> &senders,const std::string &configuration,const std::string &artifact) const {
    ValidateDataSenderSet(senders); std::map<std::string,const DataSenderOptions *> targets; std::set<std::string> addresses;
    for (const auto &sender:senders) {
        if (!sender.exclusiveStoppedOwnerConfirmed) throw std::invalid_argument("Remote receiver requires explicit sender owner/stopped declaration");
        targets.emplace(sender.senderId,&sender);
    }
    std::size_t sources=0;
    for (const auto &peer:peers) {
        if (!peer->hasPlan || peer->plan.options.configurationSha256!=configuration || peer->plan.options.artifactSha256!=artifact)
            throw std::invalid_argument("Remote receiver configuration/artifact differs from parent ECC");
        for (const auto &source:peer->plan.options.sources) {
            const auto found=targets.find(source.id);
            if (found==targets.end() || found->second->receiverAddress!=source.receiverAddress || found->second->protocol!=source.inputProtocol || !addresses.insert(source.receiverAddress).second)
                throw std::invalid_argument("Remote receiver sender ID/target differs or overlaps across nodes");
            targets.erase(found); ++sources;
        }
    }
    if (!targets.empty() || sources>195) throw std::invalid_argument("Parent sender coverage differs from all remote plans");
}

SupervisorControlRequest RemoteReceiverManager::Impl::Request(std::size_t index,SupervisorCommand command,const std::string &reference,const std::string &id,const std::string &experiment,unsigned int run,bool replaceCompletedRun,const std::string &catalogPhase,const std::string &replacementId) const {
    const auto &peer=*peers[index]; SupervisorControlRequest r; r.operationId=id; r.command=command; r.planSha256=peer.endpoint.planSha256; r.instanceId=peer.instance;
    r.controllerId=options.controllerId; r.controllerSnapshotSha256=scope; r.controllerEpoch=epoch; r.configurationSha256=peer.plan.options.configurationSha256;
    r.artifactSha256=peer.plan.options.artifactSha256; r.parentAuditReference=reference;
    if (command==SupervisorCommand::Catalog) {
        r.catalogExperiment=experiment; r.catalogRunNumber=run; r.catalogSessionId=peer.observed.sessionId;
        r.catalogReplaceExisting=replaceCompletedRun; r.catalogPhase=catalogPhase;
        if (catalogPhase!="single") r.catalogReplacementId=ConfigurationSha256("getdaq.remote-catalog-replacement-node:1\n"+replacementId+'\n'+peer.endpoint.agentId);
    }
    ValidateSupervisorControlRequest(r); return r;
}

void RemoteReceiverManager::Impl::Ready(Peer &peer,const DataSupervisorStatus &data,bool newCycle) {
    if (data.needsReconciliation || data.state!=DataSupervisorStatus::Ready || data.sessionId.empty() || data.receivers.size()!=peer.plan.options.sources.size())
        throw std::runtime_error("Remote receiver did not verify ready");
    std::map<std::string,const ManagedReceiverStatus *> receivers;
    for (const auto &r:data.receivers) {
        if (r.pid<=0 || r.reaped || r.sealVerified || r.exitCode!=-1 || r.sessionId!=data.sessionId || r.outputDirectory.empty()) throw std::runtime_error("Remote ready receiver identity differs");
        receivers.emplace(r.id,&r);
    }
    for (const auto &source:peer.plan.options.sources) if (!receivers.count(source.id)) throw std::runtime_error("Remote ready source coverage differs");
    if (!newCycle) SameCycle(peer,data,false);
}

void RemoteReceiverManager::Impl::SameCycle(const Peer &peer,const DataSupervisorStatus &data,bool sealed) const {
    if (!peer.hasCycle || data.sessionId!=peer.cycle.sessionId || data.receivers.size()!=peer.cycle.receivers.size()) throw std::runtime_error("Remote receiver cycle changed without confirmed start");
    for (const auto &old:peer.cycle.receivers) {
        const auto found=std::find_if(data.receivers.begin(),data.receivers.end(),[&](const ManagedReceiverStatus &r) { return r.id==old.id; });
        if (found==data.receivers.end() || found->pid!=old.pid || found->sessionId!=old.sessionId || found->outputDirectory!=old.outputDirectory ||
            (sealed && (!found->reaped || found->exitCode!=0 || !found->sealVerified))) throw std::runtime_error("Remote receiver PID/session/directory/verified seal differs from started cycle");
    }
}

void RemoteReceiverManager::Impl::Stopped(const Peer &peer,const DataSupervisorStatus &data) const {
    if (data.needsReconciliation) throw std::runtime_error("Remote stopped result requires reconciliation");
    if (!peer.hasCycle) {
        if (data.state!=DataSupervisorStatus::Fresh || !data.receivers.empty()) throw std::runtime_error("Remote stopped state has an unbound previous cycle");
    } else {
        if (peer.retiredCycle && data.state==DataSupervisorStatus::Retired) { SameCycle(peer,data,false); return; }
        if (data.state!=DataSupervisorStatus::Stopped) throw std::runtime_error("Remote cycle is not stopped");
        SameCycle(peer,data,true);
    }
}

} // namespace GetDAQ
