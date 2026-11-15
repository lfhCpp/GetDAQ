#include "receiverprocessmanager.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include <map>
#include <stdexcept>
namespace GetDAQ {
ReceiverProcessManager::ReceiverProcessManager(const DataSupervisorOptions &options) : fOptions(options),fSupervisor(options) {}
std::string ReceiverProcessManager::BindingSha256() const { return fSupervisor.Status().planSha256; }
bool ReceiverProcessManager::NeedsReconciliation() const { return fSupervisor.Status().needsReconciliation; }
void ReceiverProcessManager::BindController(const std::string &snapshot,const std::string &root) {
    std::lock_guard<std::mutex> lock(fControllerScopeMutex);
    SupervisorIO::Digest(snapshot); SupervisorIO::Path(root);
    const auto scope=ConfigurationSha256("getdaq.data-controller:1\n"+snapshot+'\n'+root);
    if (!fControllerScope.empty() && fControllerScope!=scope) throw std::runtime_error("Receiver manager already bound to a different parent controller");
    fControllerScope=scope;
}
void ReceiverProcessManager::MatchSenders(const std::vector<DataSenderOptions> &senders) const {
    ValidateDataSenderSet(senders); std::map<std::string,const DataSenderOptions *> addresses;
    for (const auto &s:senders) {
        if (!s.exclusiveStoppedOwnerConfirmed) throw std::invalid_argument("Receiver manager requires explicit sender owner/stopped declaration");
        addresses.emplace(s.senderId,&s);
    }
    for (const auto &source:fOptions.sources) {
        const auto found=addresses.find(source.id);
        if (found==addresses.end() || found->second->receiverAddress!=source.receiverAddress || found->second->protocol!=source.inputProtocol)
            throw std::invalid_argument("Receiver plan sender ID/target differs from parent ECC (including protocol)");
        addresses.erase(found);
    }
    if (!addresses.empty()) throw std::invalid_argument("Parent ECC has sender targets absent from receiver plan");
}
void ReceiverProcessManager::Validate(EccCommand,EccState,const std::vector<DataSenderOptions> &senders,const std::string &config,const std::string &artifact) const {
    if (config!=fOptions.configurationSha256 || artifact!=fOptions.artifactSha256) throw std::invalid_argument("Receiver plan configuration/artifact differs from parent ECC");
    MatchSenders(senders); fSupervisor.Validate();
}
void ReceiverProcessManager::VerifyStopped() const { fSupervisor.VerifyStopped(); }
void ReceiverProcessManager::StartReceivers(const std::string &path,const std::vector<DataSenderOptions> &senders) { MatchSenders(senders); fSupervisor.Start(path); }
void ReceiverProcessManager::VerifyReady() const { fSupervisor.VerifyReady(); }
std::vector<IceReceiverSessionBinding> ReceiverProcessManager::IceReceiverSessions() const {
    VerifyReady();const auto status=Status();std::vector<IceReceiverSessionBinding> bindings;
    for(std::size_t i=0;i<fOptions.sources.size();++i) if(fOptions.sources[i].inputProtocol=="ICE") {
        if(status.state!=DataSupervisorStatus::Ready || i>=status.receivers.size() || status.receivers[i].id!=fOptions.sources[i].id || status.receivers[i].sessionId!=status.sessionId || status.receivers[i].reaped)
            throw std::runtime_error("Local ICE receiver session/source mapping differs");
        IceReceiverSessionBinding binding;binding.senderId=status.receivers[i].id;binding.sessionId=status.receivers[i].sessionId;bindings.push_back(binding);
    }
    return bindings;
}
void ReceiverProcessManager::FinishReceivers(const std::string &path) { fSupervisor.Finish(path); }
void ReceiverProcessManager::FinishReceivers(const std::string &path,const std::vector<std::string> &unused) { fSupervisor.Finish(path,unused); }
void ReceiverProcessManager::AbortStartup(const std::string &path) { fSupervisor.AbortStartup(path); }
void ReceiverProcessManager::AbortFailedReceivers(const std::string &path,const std::string &parentFailure) {
    fSupervisor.AbortFailedSession(path,fSupervisor.Status().sessionId,parentFailure);
}
void ReceiverProcessManager::RetireFailedReceivers(const std::string &path,const std::string &parentFailure) {
    fSupervisor.RetireFailedSession(path,fSupervisor.Status().sessionId,parentFailure);
}
DataSupervisorStatus ReceiverProcessManager::Status() const { return fSupervisor.Status(); }
}
