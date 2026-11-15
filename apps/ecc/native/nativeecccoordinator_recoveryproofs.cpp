#include "nativeecccoordinator_p.h"
#include <stdexcept>
namespace GetDAQ {
void NativeEccCoordinator::Impl::CheckSourceRetirementProof() {
    if(!sourceRetirementDirectory || !sourceRetirementProof || !sourceRetirementStopProof || sourceRetirementFiles.empty())
        throw std::logic_error("Missing complete original source retirement proof");
    try {
        sourceRetirementDirectory->Check(); sourceRetirementProof->Check(); sourceRetirementStopProof->Check();
        for (const auto &file:sourceRetirementFiles) file->Check();
        CheckProductionStopProof();
    } catch (...) { storageFault=true; throw; }
}

void NativeEccCoordinator::Impl::CheckPreparationCleanupProof() {
    if(!failedPreparationDirectory || !failedPreparationAudit || !failureCleanupDirectory || !failureCleanupProof || failureCleanupFiles.empty())
        throw std::logic_error("No retained complete preparation cleanup proof");
    try {failedPreparationDirectory->Check();failedPreparationAudit->Check();failureCleanupDirectory->Check();failureCleanupProof->Check();
        for(const auto &file:failureCleanupFiles)file->Check();
        CheckProductionStopProof();}
    catch(...) {storageFault=true;throw;}
}

void NativeEccCoordinator::Impl::CheckProductionStopProof() {
    try {productionStopProof->Check();} catch(...) {storageFault=true;throw;}
}

void NativeEccCoordinator::Impl::CheckFailureConnectionsProof() {
    if(!failureBreakupDirectory || !failureBreakupProof || !failureBreakupStopProof || failureBreakupFiles.empty())
        throw std::logic_error("Missing complete original failure connection cleanup proof");
    try {
        failureBreakupDirectory->Check();failureBreakupProof->Check();
        failureBreakupStopProof->Check();
        for(const auto &file:failureBreakupFiles)file->Check();
        CheckProductionStopProof();
    } catch(...) {storageFault=true;throw;}
}

void NativeEccCoordinator::Impl::CheckReceiverAbortProof() {
    if(!receiverAbortDirectory || !receiverAbortProof || !receiverAbortStopProof)
        throw std::logic_error("Missing complete original receiver Abort proof");
    try {receiverAbortDirectory->Check();receiverAbortProof->Check();receiverAbortStopProof->Check();CheckProductionStopProof();}
    catch(...) {storageFault=true;throw;}
}

void NativeEccCoordinator::Impl::CheckReceiverRetirementProof() {
    if(!receiverRetirementDirectory || !receiverRetirementProof)
        throw std::logic_error("Missing complete original data retirement proof");
    try {receiverRetirementDirectory->Check();receiverRetirementProof->Check();
        CheckFailureConnectionsProof();CheckReceiverAbortProof();}
    catch(...) {storageFault=true;throw;}
}

void NativeEccCoordinator::Impl::CheckAcquisitionUnprepareProof() {
    if(!acquisitionUnprepareDirectory || !acquisitionUnprepareProof || acquisitionUnprepareFiles.empty())
        throw std::logic_error("Missing complete original acquisition Unprepare proof");
    try {
        acquisitionUnprepareDirectory->Check(); acquisitionUnprepareProof->Check();
        for (const auto &file:acquisitionUnprepareFiles) file->Check();
        CheckReceiverRetirementProof();
    } catch (...) { storageFault=true; throw; }
}
} // namespace GetDAQ
