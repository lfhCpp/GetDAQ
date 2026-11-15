#include "../apps/ecc/native/cobolifecycleprojection.h"
#include "../apps/ecc/eccstatemachine.h"
#include <iostream>
#include <stdexcept>

using namespace GetDAQ;

namespace {
void Require(bool condition,const char *message) { if(!condition)throw std::runtime_error(message); }
EccControlOperation Board(const std::string &id,const std::string &command,const std::string &cobo,const std::string &phase) {
    EccControlOperation op;op.request.operationId=id;op.request.command=command;op.request.coboId=cobo;op.phase=phase;return op;
}
std::vector<const EccControlOperation *> Pointers(const std::vector<EccControlOperation> &operations) {
    std::vector<const EccControlOperation *> result;result.reserve(operations.size());
    for(const auto &op:operations)result.push_back(&op);
    return result;
}
void Run() {
    EccControlControllerStatus controller;controller.state=static_cast<unsigned int>(EccState::Prepared);
    EccControlMaintenanceTarget a("C0",""),b("C1","");
    std::vector<EccControlOperation> ops{Board("cfg0","configure-cobo","C0","complete")};
    auto refs=Pointers(ops);
    ProjectCoBoLifecycle(a,controller,refs);ProjectCoBoLifecycle(b,controller,refs);
    Require(a.lifecycleState=="configured","completed configure should project only onto its target");
    Require(b.lifecycleState=="prepared","other board should retain controller lifecycle state");
    std::vector<EccControlMaintenanceTarget> bulk{EccControlMaintenanceTarget("C0",""),EccControlMaintenanceTarget("C1","")};
    ProjectCoBoLifecycles(bulk,controller,refs);
    Require(bulk[0].lifecycleState==a.lifecycleState && bulk[0].operationId==a.operationId &&
            bulk[1].lifecycleState==b.lifecycleState && bulk[1].operationId==b.operationId,
            "bulk projection should match independent target projections");

    ops.push_back(Board("prep0","reprepare-cobo","C0","complete"));
    refs=Pointers(ops);ProjectCoBoLifecycle(a,controller,refs);
    Require(a.lifecycleState=="prepared" && a.operationId=="prep0","latest board maintenance should determine its projection");
    ops.push_back(Board("led1","set-cobo-led","C1","complete"));
    refs=Pointers(ops);ProjectCoBoLifecycle(a,controller,refs);ProjectCoBoLifecycle(b,controller,refs);
    Require(a.lifecycleState=="prepared","another board's maintenance must not alter this board");
    Require(b.lifecycleState=="prepared" && b.operationId=="led1","non-lifecycle maintenance should preserve state and expose operation");

    controller.transition=static_cast<unsigned int>(EccCommand::ConfigureCoBo);
    ops.push_back(Board("active0","configure-cobo","C0","running"));
    refs=Pointers(ops);ProjectCoBoLifecycle(a,controller,refs);ProjectCoBoLifecycle(b,controller,refs);
    Require(a.lifecycleState=="transitioning","selected board should show an in-flight board transition");
    Require(b.lifecycleState=="prepared","board transition must not make other boards unknown");
    controller.transition=0;

    ops.push_back(Board("failed0","configure-cobo","C0","failed"));
    refs=Pointers(ops);ProjectCoBoLifecycle(a,controller,refs);
    Require(a.lifecycleState=="unknown","failed board transition should project unknown");
    ProjectCoBoLifecycle(b,controller,refs);
    Require(b.lifecycleState=="prepared","failed board transition should remain isolated");

    controller.needsReconciliation=true;
    ProjectCoBoLifecycle(a,controller,refs);ProjectCoBoLifecycle(b,controller,refs);
    Require(a.lifecycleState=="unknown" && b.lifecycleState=="unknown","controller reconciliation must invalidate every projection");

    controller.needsReconciliation=false;controller.transition=0;controller.state=static_cast<unsigned int>(EccState::Ready);
    EccControlOperation global;global.request.command="configure";global.request.operationId="global-configure";global.phase="complete";
    EccControlControllerStatus completed;completed.state=static_cast<unsigned int>(EccState::Ready);global.resultJson=EncodeEccControlControllerStatus(completed);
    ops.push_back(global);refs=Pointers(ops);
    ProjectCoBoLifecycle(a,controller,refs);ProjectCoBoLifecycle(b,controller,refs);
    Require(a.lifecycleState=="configured" && b.lifecycleState=="configured","latest global lifecycle must supersede older board receipts for every target");
}
}

int main() {
    try { Run();std::cout<<"CoBo lifecycle projection checks passed\n";return 0; }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
