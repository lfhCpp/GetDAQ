#include "nativeecccoordinator_p.h"
#include <stdexcept>
namespace GetDAQ {
void NativeEccCoordinator::Impl::Validate(EccCommand c,EccState from,EccState to) {
    Match(c,from,to);
    const bool stopRunning=(from==EccState::Running || from==EccState::Paused) && (c==EccCommand::Pause || c==EccCommand::Stop || c==EccCommand::Breakup);
    // 已运行的负向动作不被数据进程失效/远端不可达预检查挡住；子流程独立停全部生产。
    if (!stopRunning) {
        if (data.BindingSha256()!=dataBinding) throw std::runtime_error("Native ECC data lifecycle binding changed");
        data.Validate(c,from,senders,bound.configurationSha256,bound.artifactSha256);
    }
    if (c==EccCommand::Describe || c==EccCommand::Prepare || c==EccCommand::Configure || c==EccCommand::Reset || c==EccCommand::ReprepareCoBo || c==EccCommand::ConfigureCoBo || c==EccCommand::SetCoBoLed || c==EccCommand::TriggerAsAdPulser || c==EccCommand::BlinkCoBoLeds || c==EccCommand::FlipCoBoLed || c==EccCommand::PulseCoBoLed) data.VerifyStopped();
    if (c==EccCommand::Start || c==EccCommand::Resume) data.VerifyReady();
    if (c==EccCommand::Describe) {
        std::unique_ptr<SystemDescriptionClient> fresh(new SystemDescriptionClient(options.description));
        fresh->Validate(); description=std::move(fresh); return;
    }
    if (!description || description->GetState()!=SystemDescriptionClient::State::Described) throw std::logic_error("Native ECC has no confirmed described topology");
    if (c==EccCommand::Prepare) {
        if (preparation) throw std::logic_error("Native ECC Prepare requires a fresh preparation epoch");
    } else if (c!=EccCommand::Reset || from!=EccState::Described) {
        if (!preparation) throw std::logic_error("Native ECC has no retained preparation object");
        if (c==EccCommand::Configure || c==EccCommand::ReprepareCoBo || c==EccCommand::ConfigureCoBo || c==EccCommand::SetCoBoLed || c==EccCommand::TriggerAsAdPulser || c==EccCommand::BlinkCoBoLeds || c==EccCommand::FlipCoBoLed || c==EccCommand::PulseCoBoLed || (c==EccCommand::Reset && from==EccState::Prepared)) {
            if (preparation->GetState()!=SystemPreparationClient::State::Prepared) throw std::logic_error("Native ECC requires the same Prepared object");
        } else {
            if (preparation->GetState()!=SystemPreparationClient::State::Configured || !acquisition) throw std::logic_error("Native ECC requires retained Configured/run objects");
        }
        if (c==EccCommand::Configure || c==EccCommand::ReprepareCoBo || c==EccCommand::ConfigureCoBo || c==EccCommand::SetCoBoLed || c==EccCommand::TriggerAsAdPulser || c==EccCommand::BlinkCoBoLeds || c==EccCommand::FlipCoBoLed || c==EccCommand::PulseCoBoLed || c==EccCommand::Start || c==EccCommand::Resume) preparation->CheckInterlock();
    }
    if (!stopRunning) ValidateNativeEccHardware(bound,senders,c,from,preparation.get(),
        context.maintenanceNodeId,context.maintenanceAsadId,context.maintenanceLedType,context.maintenanceLedState,context.pulserVoltageMv,context.maintenanceLedFlipId,context.maintenanceLedPulseId,context.maintenanceLedPulsePeriodMs);
}
} // namespace GetDAQ
