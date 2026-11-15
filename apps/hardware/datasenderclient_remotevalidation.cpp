#include "datasenderclient_p.h"
#include "datasenderclient_validation_p.h"
#include "../../common/text/utf8.h"
#include "strictjson.h"
#include <set>
#include <stdexcept>
namespace GetDAQ {
using DataSenderValidation::Text;
using DataSenderClientInternal::CheckRetirement;
void DataSenderClient::Impl::Validate(bool allowSourceUnknown) {
    try {ValidateRemote(allowSourceUnknown);}catch(...) {
        std::lock_guard<std::mutex> lock(statusMutex);
        if(options.protocol=="ICE" && snapshot.state!=DataSenderSnapshot::Fresh) {
            snapshot.state=DataSenderSnapshot::Unknown;snapshot.outcomeUnknown=true;
        }
        throw;
    }
}

void DataSenderClient::Impl::ValidateRemote(bool allowSourceUnknown) {
    // 对实际远端发ice_isA，不能用typed代理本地checkedCast当接口证明。
    if (!proxy->ice_isA(mdaq::DaqCtrlNode::ice_staticId())) throw std::runtime_error("Data target does not implement mdaq::DaqCtrlNode");
    if(options.protocol=="ICE") {
        if(!sourceProxy) {
            if(!proxy->ice_isA(GetDAQ::DataSourceControl::ice_staticId())) throw std::runtime_error("ICE source lacks explicit source-control/session/end capability");
            sourceConnection=proxy->ice_getConnection();
            if(!sourceConnection || sourceConnection->type()!="tcp") throw std::runtime_error("ICE source control requires checked TCP connection");
            Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=Ice::Byte(options.encodingMinor);
            const auto fixed=sourceConnection->createProxy(proxy->ice_getIdentity())->ice_encodingVersion(encoding)->ice_twoway();
            sourceProxy=GetDAQ::DataSourceControlPrx::uncheckedCast(fixed);proxy=mdaq::DaqCtrlNodePrx::uncheckedCast(fixed);
        }
        const auto info=sourceProxy->describeSource();Text(info.sourceId,128);Text(info.receiverAddress,64);
        if(info.version!=1 || info.sourceId!=options.senderId || info.receiverAddress!=options.receiverAddress || info.instanceId.size()!=64 || info.instanceId.find_first_not_of("0123456789abcdef")!=std::string::npos ||
            info.issuedBytes<0 || info.locallySentBytes<0 || info.locallySentBytes>info.issuedBytes || info.error.size()>512)
            throw std::runtime_error("ICE source capability/identity/counters differ");
        ValidateJsonUtf8(info.error);
        if(!info.receiverSession.empty() && (info.receiverSession.size()!=64 || info.receiverSession.find_first_not_of("0123456789abcdef")!=std::string::npos)) throw std::runtime_error("ICE source receiver session is malformed");
        const auto prior=Status();
        if((!prior.sourceInstanceId.empty() && prior.sourceInstanceId!=info.instanceId) ||
           (prior.state!=DataSenderSnapshot::Fresh && info.receiverSession!=prior.receiverSession)) {
            if(prior.state!=DataSenderSnapshot::Fresh) {std::lock_guard<std::mutex> lock(statusMutex);snapshot.state=DataSenderSnapshot::Unknown;snapshot.outcomeUnknown=true;}
            throw std::runtime_error("ICE source instance/session changed; no adoption/replay");
        }
        bool retiredFresh=false;
        if(!allowSourceUnknown && prior.state==DataSenderSnapshot::Fresh && info.state=="unknown" && !info.receiverSession.empty()) {
            const auto recovery=DataSourceRecoveryPrx::checkedCast(sourceProxy);
            if(recovery){CheckRetirement(recovery->describeFailedSourceRetirement(info.receiverSession),info);retiredFresh=true;}
        }
        if(!allowSourceUnknown && !retiredFresh && (info.state=="unknown" || !info.error.empty())) {
            if(prior.state!=DataSenderSnapshot::Fresh) {std::lock_guard<std::mutex> lock(statusMutex);snapshot.state=DataSenderSnapshot::Unknown;snapshot.outcomeUnknown=true;}
            throw std::runtime_error("ICE source outcome UNKNOWN; normal control refused");
        }
        if(info.sourceEndAcknowledged && (!info.finishIssued || !info.stopAcknowledged || !info.drainAcknowledged || (info.state!="disconnected" && info.state!="unknown"))) throw std::runtime_error("ICE source end lacks source-side stopped/drained acknowledgement");
        const std::set<std::string> states={"fresh","connecting","connected","starting","running","stopping","stopped","disconnecting","disconnected","unknown"};
        if(!states.count(info.state)) throw std::runtime_error("Unsupported ICE source state");
        std::lock_guard<std::mutex> lock(statusMutex);snapshot.sourceInstanceId=info.instanceId;
    }
}


} // namespace GetDAQ
