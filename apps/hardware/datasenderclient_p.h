#ifndef GETDAQ_DATASENDERCLIENT_P_H
#define GETDAQ_DATASENDERCLIENT_P_H
#include "datasenderclient.h"
#include <Ice/Ice.h>
#include <mdaq/DaqControl.h>
#include <SourceControl.h>
#include <memory>
#include <mutex>
namespace GetDAQ {
namespace DataSenderClientInternal {
class Parser { public: Ice::CommunicatorPtr communicator; explicit Parser(int timeout); ~Parser(); };
void CheckRetirement(const FailedSourceRetirement &receipt,const SourceInfo &info);
}
class DataSenderClient::Impl {
public:
    DataSenderOptions options;
    std::unique_ptr<DataSenderClientInternal::Parser> parser;
    mdaq::DaqCtrlNodePrx proxy;
    DataSourceControlPrx sourceProxy;
    Ice::ConnectionPtr sourceConnection;
    mutable std::mutex statusMutex;
    std::mutex operationMutex;
    DataSenderSnapshot snapshot;
    std::string completedCommandChain,failureRetirementChain;
    void Validate(bool allowSourceUnknown=false);
    void ValidateRemote(bool allowSourceUnknown);
    DataSenderSnapshot Status() const;
    void Change(DataSenderSnapshot::State target,const std::string &path,const std::function<void()> &check);
};
}
#endif
