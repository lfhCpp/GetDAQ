#include "cobolifecycleprojection.h"
#include "../eccstatemachine.h"
#include <map>

namespace GetDAQ {
namespace {
std::string Lifecycle(unsigned int state) {
    switch(state) {
    case static_cast<unsigned int>(EccState::Idle):return "uninitialized";
    case static_cast<unsigned int>(EccState::Described):return "described";
    case static_cast<unsigned int>(EccState::Prepared):return "prepared";
    case static_cast<unsigned int>(EccState::Ready):return "configured";
    case static_cast<unsigned int>(EccState::Running):return "running";
    case static_cast<unsigned int>(EccState::Paused):return "paused";
    default:return "unknown";
    }
}
bool IsBoardOperation(const std::string &command) {
    return command=="reprepare-cobo" || command=="configure-cobo" || command=="set-cobo-led" ||
           command=="blink-cobo-leds" || command=="flip-cobo-led" || command=="pulse-cobo-led" || command=="trigger-asad-pulser";
}
bool IsLifecycleOperation(const std::string &command) {
    return command=="describe" || command=="prepare" || command=="configure" || command=="start" || command=="stop" ||
           command=="pause" || command=="resume" || command=="breakup" || command=="reset" ||
           command=="reconcile-description" || command=="reconcile-preparation" || command=="reconcile-preparation-with-alarm" ||
           command=="reconcile-acquisition" || command=="reconcile-acquisition-with-sources" || command=="reconcile-acquisition-with-alarm";
}
}

void ProjectCoBoLifecycles(std::vector<EccControlMaintenanceTarget> &targets,
                           const EccControlControllerStatus &controller,
                           const std::vector<const EccControlOperation *> &operations) {
    const bool selectedBoardTransition=controller.transition>=int(EccCommand::ReprepareCoBo) &&
                                       controller.transition<=int(EccCommand::ConfigureCoBo);
    const std::string baseline=(controller.outcomeUnknown || controller.needsReconciliation ||
                                (controller.transition && !selectedBoardTransition)) ? "unknown" : Lifecycle(controller.state);
    std::map<std::string,std::size_t> targetIndices;
    for(std::size_t i=0;i<targets.size();++i) {
        targets[i].lifecycleState=baseline;
        targets[i].operationPhase="idle";targets[i].operationId.clear();targets[i].operation.clear();
        targetIndices.emplace(targets[i].id,i);
    }

    std::size_t lastLifecycle=0;bool hasLifecycle=false;
    for(std::size_t i=0;i<operations.size();++i) {
        if(operations[i] && IsLifecycleOperation(operations[i]->request.command)){lastLifecycle=i;hasLifecycle=true;}
    }
    const std::size_t first=hasLifecycle ? lastLifecycle+1 : 0;
    for(std::size_t i=first;i<operations.size();++i) {
        if(!operations[i])continue;
        const auto &op=*operations[i];
        if(IsLifecycleOperation(op.request.command) || !IsBoardOperation(op.request.command))continue;
        const auto selected=targetIndices.find(op.request.coboId);
        if(selected==targetIndices.end())continue;
        auto &target=targets[selected->second];
        target.operationId=op.request.operationId;target.operation=op.request.command;target.operationPhase=op.phase;
        if(op.phase=="accepted" || op.phase=="running")target.lifecycleState="transitioning";
        else if(op.phase!="complete")target.lifecycleState="unknown";
        else if(op.request.command=="reprepare-cobo")target.lifecycleState="prepared";
        else if(op.request.command=="configure-cobo")target.lifecycleState="configured";
    }

    // A controller restart invalidates the complete old projection, including
    // otherwise complete per-board maintenance receipts.
    if(controller.outcomeUnknown || controller.needsReconciliation)
        for(auto &target:targets)target.lifecycleState="unknown";
}

void ProjectCoBoLifecycle(EccControlMaintenanceTarget &target,
                          const EccControlControllerStatus &controller,
                          const std::vector<const EccControlOperation *> &operations) {
    std::vector<EccControlMaintenanceTarget> one(1,target);
    ProjectCoBoLifecycles(one,controller,operations);
    target=one.front();
}
}
