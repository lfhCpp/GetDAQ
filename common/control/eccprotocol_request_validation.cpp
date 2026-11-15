#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"
#include <limits>
#include <stdexcept>

namespace GetDAQ {
using namespace EccProtocolInternal;
void ValidateEccControlRequest(const EccControlRequest &r) {
    Id(r.operationId); Digest(r.instanceId); Digest(r.snapshotSha256); Digest(r.clientEpoch); Text(r.clientId,128);
    if(r.command=="reprepare-cobo" || r.command=="configure-cobo" || r.command=="set-cobo-led" || r.command=="trigger-asad-pulser" || r.command=="blink-cobo-leds" || r.command=="flip-cobo-led" || r.command=="pulse-cobo-led")Text(r.coboId,128);
    else if(!r.coboId.empty())throw std::invalid_argument("Selected CoBo requires a selected-board command");
    if(r.command=="set-cobo-led" && (r.ledAsadId>3 || r.ledType>3 || r.ledState>3))throw std::invalid_argument("Invalid set-cobo-led selection");
    if(r.command!="set-cobo-led" && (r.ledAsadId || r.ledType || r.ledState))throw std::invalid_argument("LED selection requires set-cobo-led");
    if(r.command=="flip-cobo-led" && r.ledFlipId>16)throw std::invalid_argument("LED flip selection must be 0..15 or 16 for all LEDs");
    if(r.command!="flip-cobo-led" && r.ledFlipId)throw std::invalid_argument("LED flip selection requires flip-cobo-led");
    if(r.command=="pulse-cobo-led" && r.ledPulseId>16)throw std::invalid_argument("LED pulse selection must be 0..15 or 16 for all LEDs");
    if(r.command!="pulse-cobo-led" && (r.ledPulseId || r.ledPulsePeriodMs))throw std::invalid_argument("LED pulse selection requires pulse-cobo-led");
    if(r.command=="trigger-asad-pulser" && (r.pulserVoltageMv < -1000 || r.pulserVoltageMv > 1000))throw std::invalid_argument("Single pulser voltage must be within -1000..1000 mV");
    if(r.command!="trigger-asad-pulser" && r.pulserVoltageMv)throw std::invalid_argument("Pulser voltage requires trigger-asad-pulser");
    if (r.command=="handoff-data-control" || r.command=="retire-data-plans") {Digest(r.successorScopeSha256);Digest(r.successorEpoch);Digest(r.successorPlanSha256);Digest(r.successorDataBindingSha256);}
    else if (!r.successorScopeSha256.empty() || !r.successorEpoch.empty() || !r.successorPlanSha256.empty() || !r.successorDataBindingSha256.empty())
        throw std::invalid_argument("Successor fields require handoff-data-control");
    if (r.command=="retire-data-plans") {
        if (r.successorReceiverPlans.empty() || r.successorReceiverPlans.size()>195) throw std::invalid_argument("Retirement requires bounded successor receiver plans");
        std::string previous;
        for (const auto &target:r.successorReceiverPlans) {
            Text(target.agentId,128);Digest(target.planSha256);Text(target.planFile,4096);
            if ((!previous.empty() && target.agentId<=previous) || target.planFile.empty() || target.planFile[0]!='/' || target.planFile.back()=='/' ||
                target.planFile.find("//")!=std::string::npos || target.planFile.find("/./")!=std::string::npos || target.planFile.find("/../")!=std::string::npos ||
                target.planFile.substr(target.planFile.size()>2 ? target.planFile.size()-2 : 0)=="/." ||
                target.planFile.substr(target.planFile.size()>3 ? target.planFile.size()-3 : 0)=="/..")
                throw std::invalid_argument("Successor plans require sorted unique agents and absolute canonical paths");
            previous=target.agentId;
        }
        if (!r.successorPlanFile.empty()) {
            Text(r.successorPlanFile,4096);
            if (r.successorPlanFile[0]!='/' || r.successorPlanFile.back()=='/' || r.successorPlanFile.find("//")!=std::string::npos ||
                r.successorPlanFile.find("/./")!=std::string::npos || r.successorPlanFile.find("/../")!=std::string::npos ||
                r.successorPlanFile.substr(r.successorPlanFile.size()>2 ? r.successorPlanFile.size()-2 : 0)=="/." ||
                r.successorPlanFile.substr(r.successorPlanFile.size()>3 ? r.successorPlanFile.size()-3 : 0)=="/..")
                throw std::invalid_argument("Successor plan lease requires an absolute canonical service-host file");
        }
    } else if (!r.successorReceiverPlans.empty()) throw std::invalid_argument("Receiver plan targets require retire-data-plans");
    if (r.command!="retire-data-plans" && !r.successorPlanFile.empty()) throw std::invalid_argument("Successor plan lease requires retire-data-plans");
    if((r.command=="complete-data-handoff" || r.command=="resume-catalog-replacement" || r.command=="reconcile-description" || r.command=="reconcile-preparation" || (r.command=="reconcile-acquisition" || r.command=="reconcile-acquisition-with-sources") || r.command=="reconcile-acquisition-with-alarm" || r.command=="reconcile-preparation-with-alarm" || r.command=="release-stopped-alarm")) {Id(r.failedOperationId);if(r.failedOperationId==r.operationId)throw std::invalid_argument("Recovery requires a new operation ID");}
    else if(!r.failedOperationId.empty())throw std::invalid_argument("Failed operation binding requires reconcile-description");
    if(r.command=="reconcile-acquisition-with-alarm" || r.command=="reconcile-preparation-with-alarm" || r.command=="release-stopped-alarm") {if(r.command!="release-stopped-alarm" || !r.alarmOperationId.empty())Id(r.alarmOperationId);if(r.alarmOperationId==r.operationId || (r.command!="release-stopped-alarm" && r.alarmOperationId==r.failedOperationId))throw std::invalid_argument("Alarm recovery requires distinct original alarm/failure and new IDs");}
    else if(!r.alarmOperationId.empty())throw std::invalid_argument("Alarm operation binding requires explicit alarm recovery");
    if (r.command=="catalog") {
        Text(r.catalogExperiment,80);
        if (r.catalogExperiment.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos ||
            r.catalogExperiment[0]=='_' || r.catalogExperiment[0]=='-' || r.catalogRunNumber>=unsigned(std::numeric_limits<int>::max())) throw std::invalid_argument("Invalid catalog experiment/Run binding");
        return;
    }
    if (!r.catalogExperiment.empty() || r.catalogRunNumber || r.catalogReplaceExisting) throw std::invalid_argument("Catalog fields require catalog command");
    if (r.command=="handoff-data-control" || r.command=="retire-data-plans" || r.command=="reprepare-cobo" || r.command=="configure-cobo" || r.command=="set-cobo-led" || r.command=="trigger-asad-pulser" || r.command=="blink-cobo-leds" || r.command=="flip-cobo-led" || r.command=="pulse-cobo-led" || r.command=="complete-data-handoff" || r.command=="resume-catalog-replacement") return;
    const std::string names[]={"describe","prepare","configure","start","stop","pause","resume","breakup","reset","stop-production-after-failure","abort-failed-receivers","retire-failed-receivers","unprepare-acquisition-after-failure","unprepare-after-failure","breakup-after-failure","reconcile-description","reconcile-preparation","reconcile-acquisition","reconcile-acquisition-with-alarm","reconcile-preparation-with-alarm","release-stopped-alarm","retire-failed-sources","reconcile-acquisition-with-sources"};
    for (const auto &name:names) if (r.command==name) return;
    throw std::invalid_argument("Unsupported ECC control command");
}
}
