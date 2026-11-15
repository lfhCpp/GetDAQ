#ifndef GETDAQ_RECEIVERPROCESSPOSIX_P_H
#define GETDAQ_RECEIVERPROCESSPOSIX_P_H

#include "datasupervisor.h"
#include "supervisorio.h"

#include <string>
#include <vector>
#include <sys/types.h>

namespace GetDAQ { namespace ReceiverProcessInternal {

int OpenLog(const std::string &path);
void CycleControl(SupervisorIO::Fd &reader, SupervisorIO::Fd &writer);
void SendCycleControl(int fd, char request);
pid_t Spawn(const std::string &executable, const std::vector<std::string> &args,
            const std::string &log, int input = -1);
bool Exited(pid_t pid);
int Reap(pid_t pid);
ReceiverProcessObservation Observe(const ManagedReceiverStatus &child);

} }

#endif
