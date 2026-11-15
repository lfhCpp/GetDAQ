#ifndef GETDAQ_REMOTEOBSERVATIONS_P_H
#define GETDAQ_REMOTEOBSERVATIONS_P_H

#include <string>

namespace GetDAQ {

struct RemoteReceiverOptions;
struct RemoteReceiverStatus;
struct RemoteReceiverEndpoint;

std::string RemoteObservationError(const char *error);
std::string ObserveRemoteReceiverEndpoints(const RemoteReceiverOptions &options,
                                          const RemoteReceiverStatus &cachedStatus);
std::string ObserveRemoteAcquisitionEvents(const RemoteReceiverEndpoint &endpoint,
    const std::string &controllerId,const std::string &instanceId,const std::string &planSha256,
    const std::string &senderId,unsigned int sourceIndex,const std::string &sessionId,int pid,bool fileBytes);

}

#endif
