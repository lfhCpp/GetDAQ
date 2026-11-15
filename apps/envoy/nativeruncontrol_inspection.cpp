#include "nativeruncontrol.h"

void NativeRunControl::InspectConfiguration() {
    if (!fConnected || fShuttingDown || !fClient) {emit ConfigurationSignal({},"Native ECC is not connected");return;}
    if (!fConfigurationBody.isEmpty()) {emit ConfigurationSignal(fConfigurationBody,{});return;}
    if (!fClient->QueryConfiguration()) emit ConfigurationSignal({},"ECC identity is unavailable or configuration query is already pending");
    Publish();
}
void NativeRunControl::InspectConfigureReadbacks(quint64 generation) {
    if(!fConnected || fShuttingDown || !fClient){emit ConfigureReadbacksSignal({},"Native ECC is not connected",generation);return;}
    if(fClient->ConfigureReadbacksPending())return;
    fReadbacksRequestGeneration=generation;
    if(!fClient->QueryConfigureReadbacks() && !fClient->ConfigureReadbacksPending())
        emit ConfigureReadbacksSignal({},"ECC identity or credential is unavailable",generation);
}
void NativeRunControl::InspectOwnership(quint64 generation) {
    if (!fConnected || fShuttingDown || !fClient) {emit OwnershipSignal({},"Native ECC is not connected",generation);return;}
    if (fClient->OwnershipPending()) return;
    fOwnershipRequestGeneration=generation;
    if (!fClient->QueryOwnership() && !fClient->OwnershipPending())
        emit OwnershipSignal({},"ECC identity or credential is unavailable",generation);
}
void NativeRunControl::Configuration(const QByteArray &body,const QString &error) {
    if (!fConnected || fShuttingDown) return;
    if (error.isEmpty()) fConfigurationBody=body;
    else emit LogSignal("ECC startup configuration observation unavailable: "+error);
    emit ConfigurationSignal(error.isEmpty() ? body : fConfigurationBody,error);
    ContinueFromStatus();Publish();
}
