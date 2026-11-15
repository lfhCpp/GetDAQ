#include "datasupervisor_p.h"
#include "receivercycleevidence.h"

#include <chrono>
#include <mutex>
#include <stdexcept>

namespace GetDAQ {
using namespace DataSupervisorInternal;
using namespace ReceiverProcessInternal;

std::string DataSupervisor::CompletedCycleEvidenceJson() const {
    VerifyControlHandoff();auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Supervisor cycle evidence busy; not queued");
    p.Eligible();p.CheckStore();
    if (p.snapshot.state!=DataSupervisorStatus::Stopped || p.children.empty() || p.children.size()!=p.options.sources.size())
        throw std::runtime_error("Cycle evidence requires original normally stopped receivers");
    std::vector<std::string> metadata={p.options.auditRoot+"/plan.json",p.options.auditRoot+"/supervisor.jsonl"},directories;
    for (std::size_t i=0;i<p.children.size();++i) {
        const auto &child=*p.children[i];
        if (!child.status.reaped || child.status.exitCode!=0 || !child.status.sealVerified)
            throw std::runtime_error("Cycle evidence requires original normal receiver exit and seal");
        directories.push_back(child.status.outputDirectory);metadata.push_back(child.log+".stdout");
        metadata.push_back(child.status.outputDirectory+"/.getdaq-managed-session.json");
        metadata.push_back(child.status.outputDirectory+(p.options.sources[i].inputProtocol=="ICE" ? "/.getdaq-ice-receiver-seal.json" : "/.getdaq-receiver-seal.json"));
    }
    const auto bytes=CaptureReceiverCycleEvidence(p.options.auditRoot,metadata,directories);
    ReceiverCycleEvidence checked(bytes);checked.Check();p.CheckStore();return bytes;
}
DataSupervisorObservation DataSupervisor::ObserveReceivers() const {
    auto &p=*fImpl; DataSupervisorObservation result;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    result.busy=!operation.owns_lock(); result.lifecycle=Status();
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    result.observedAtUnixMs=now>0 ? std::uint64_t(now) : 0;
    if (!result.busy) for (const auto &child:p.children) result.receivers.push_back(Observe(child->status));
    return result;
}
ReceiverAcquisitionEventObservation DataSupervisor::ObserveAcquisitionEvents(std::size_t sourceIndex,bool fileBytes) const {
    auto &p=*fImpl;
    if (sourceIndex>=p.options.sources.size()) throw std::out_of_range("Receiver source index is outside the fixed plan");
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Supervisor operation already running; acquisition events are unavailable");
    if (p.children.size()!=p.options.sources.size() || sourceIndex>=p.children.size())
        throw std::runtime_error("Receiver acquisition events require the original active receiver session");
    const auto &source=p.options.sources[sourceIndex]; const auto &child=*p.children[sourceIndex];
    const auto process=Observe(child.status);
    if (process.state!="running") throw std::runtime_error("Receiver process is not confirmed running; acquisition events are unavailable");
    const auto status=ParseStrictJson(HttpStatus(source.statusPort,p.options.statusTimeoutMs));
    p.Identity(status,child,source);
    ReceiverAcquisitionEventObservation result;
    result.sourceId=source.id; result.sessionId=p.session; result.pid=child.status.pid;
    if(!fileBytes) {
        result.available=Boolean(status.At("acquisition_event_count_available"));
        if(result.available)result.acquisitionEvents=Unsigned(status.At("acquisition_events"));
    }
    if(fileBytes && status.members.count("accepted_file_bytes") && String(status.At("error")).empty()) {
        result.fileBytes=Unsigned(status.At("accepted_file_bytes"));
        // Writer and network counters are sampled independently; do not compare
        // a current file count to an earlier wire snapshot during live writes.
        result.fileBytesAvailable=true;
    }
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    result.observedAtUnixMs=now>0 ? std::uint64_t(now) : 0;
    return result;
}

}
