#ifndef GETDAQ_SYSTEMALIGNMENT_P_H
#define GETDAQ_SYSTEMALIGNMENT_P_H

#include "systemalignment.h"
#include <stdexcept>
#include "hardwareaudit.h"
#include <Ice/Ice.h>
#include <get/cobo/MutantLinkManager.h>
#include <get/mutant/AlignmentServer.h>
#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <set>

namespace GetDAQ {
namespace SystemAlignmentInternal {
std::string EndpointKey(const Ice::ObjectPrx &proxy);
std::string ObjectKey(const Ice::ObjectPrx &proxy);
HardwareStep ReadStep(const char *device,const char *reg,const char *field);
std::vector<HardwareStep> MutantPreflight();
std::string Reason(const std::exception &error);
std::string SlotJson(const std::vector<AlignmentSlotResult> &slots);
}

class SystemAlignmentClient::Impl {
public:
    SystemAlignmentOptions options;
    std::atomic<SystemAlignmentClient::State> state{SystemAlignmentClient::State::Fresh};
    std::mutex operation;
    Ice::CommunicatorPtr communicator;
    std::vector<get::cobo::MutantLinkManagerPrx> links;
    std::vector<get::mutant::AlignmentServerPrx> servers;
    std::vector<std::unique_ptr<HardwareClient>> coboHardware,mutantHardware;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory;
    unsigned int sequence=0,hardwareSequence=0;
    bool interfacesValidated=false;

    ~Impl() { if(communicator) { try { communicator->destroy(); } catch(...) {} } }
    template<class T> T Options(T proxy) {
        if(!proxy)throw std::invalid_argument("Empty topology proxy");
        Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=options.encodingMinor;
        return T::uncheckedCast(proxy->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(options.timeoutMs)->ice_encodingVersion(encoding));
    }
    void Record(const std::string &row);
    void Command(const std::string &operation,const std::string &details,const std::function<void()> &call);
    void BindInterfaces();
    void Preflight();
    void Link(std::size_t index,CoBoLinkMode mode);
    void Links(CoBoLinkMode mode,bool activeOnly);
    std::vector<AlignmentSlotResult> Slots(std::size_t module,AlignmentClock clock);
    LocatedCoBo Locate(std::size_t cobo,AlignmentClock clock);
    void Wsca(std::size_t index);
    void Multi(std::size_t index,AlignmentClock clock,unsigned int step,SystemAlignmentResult &result);
};
}

#endif
