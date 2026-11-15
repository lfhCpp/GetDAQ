#ifndef GETDAQ_HARDWARECLIENT_P_H
#define GETDAQ_HARDWARECLIENT_P_H
#include "hardwareclient.h"
#include "hardwareaudit.h"
#include <Ice/Ice.h>
#include <mdaq/hw/Control.h>
#include <get/cobo/CtrlNode.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
namespace GetDAQ {
namespace HardwareClientInternal {

void CheckName(const std::string &name);
void ValidatePlanStructure(const std::vector<HardwareStep> &steps, bool hasBoundDescription);
std::uint64_t Bits(Ice::Long value);
Ice::Long Wire(std::uint64_t value);
std::uint64_t Mask(int bits);
std::string Message(const std::exception &e);
typedef HardwareAudit Audit;

}
using namespace HardwareClientInternal;
class HardwareClient::Impl {
public:
    struct PlanTarget { mdaq::hw::DevicePrx proxy; DeviceInfo description; };
    Ice::CommunicatorPtr communicator;
    mdaq::hw::NodePrx node;
    std::string address;
    int minor, timeout;
    std::mutex mutex;
    std::string completedAuditChain;
    std::map<std::string, DeviceInfo> expected;
    std::string boundNodeId;
    bool descriptionOutcomeUnknown = false, destroyAttempted = false;
    void ExecutePlan(const std::vector<HardwareStep> &steps, const std::string &auditPath,
                     const std::function<void()> &checkpoint, const std::string &plan,
                     const std::vector<int> &widths,
                     const std::map<std::string, PlanTarget> &targets,
                     get::cobo::AsAdPulserMgrPrx pulser,
                     get::cobo::LedManagerPrx leds,
                     get::cobo::AsAdAlarmMonitorPrx alarmMonitor);
    ~Impl();
    template<class T> T Options(T proxy) {
        Ice::EncodingVersion encoding; encoding.major = 1; encoding.minor = minor;
        return T::uncheckedCast(proxy->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(timeout)->ice_encodingVersion(encoding));
    }
    mdaq::hw::DevicePrx Device(const std::string &name);
    DeviceInfo Description(const mdaq::hw::DevicePrx &device, const std::string &requested);
    int Width(const DeviceInfo &info, const std::string &regName, const std::string &fieldName, bool writing);
};

}
#endif
