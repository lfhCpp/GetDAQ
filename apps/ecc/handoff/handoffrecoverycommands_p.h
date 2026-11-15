#ifndef GETDAQ_HANDOFFRECOVERYCOMMANDS_P_H
#define GETDAQ_HANDOFFRECOVERYCOMMANDS_P_H

#include <string>

namespace GetDAQ {
struct EccControlRequest;
struct EccControlSystemStatus;
struct RemoteReceiverOptions;
namespace HandoffRecovery {

void VerifyReceipt(const std::string &bytes, const EccControlRequest &request,
                   const EccControlSystemStatus &original, const RemoteReceiverOptions &options);
int Complete(int argc, char **argv);
int ReviewCold(int argc, char **argv);

} // namespace HandoffRecovery
} // namespace GetDAQ

#endif
