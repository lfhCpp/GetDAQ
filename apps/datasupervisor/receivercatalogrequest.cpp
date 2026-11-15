#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace GetDAQ {
namespace ReceiverCatalogInternal {
using namespace SupervisorIO;

namespace {
void Component(const std::string &text,std::size_t maximum) {
    Text(text,maximum);
    Require(text.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos &&
        text[0]!='_' && text[0]!='-',"Catalog requires a bounded letter/digit path component");
}
}

void CheckOptions(const ReceiverCatalogOptions &o) {
    ValidateOptions(o.receiver,false); Path(o.receiver.catalogRoot); Digest(o.executableSha256); SupervisorOperationId(o.operationId);
    Component(o.experiment,80); Component(o.receiver.deploymentId,128); Require(o.runNumber<unsigned(std::numeric_limits<int>::max()),"Catalog Run number outside range");
    const auto &s=o.stopped;
    Require(s.state==DataSupervisorStatus::Stopped && !s.needsReconciliation && !s.receivers.empty() && s.receivers.size()==o.receiver.sources.size(),"Catalog requires the complete confirmed Stopped receiver set");
    Digest(s.sessionId);
    Require(s.planSha256==ConfigurationSha256(EncodeOptions(o.receiver,o.executableSha256)),"Catalog receiver plan SHA differs from stopped proof");
    std::map<std::string,const ManagedReceiverStatus *> receivers;
    for (const auto &r:s.receivers) {
        Require(receivers.emplace(r.id,&r).second && r.pid>0 && r.reaped && r.exitCode==0 && r.sealVerified && r.sessionId==s.sessionId,"Catalog receiver exit/session proof differs");
        Require(o.receiver.catalogRoot!=r.outputDirectory && o.receiver.catalogRoot.compare(0,r.outputDirectory.size()+1,r.outputDirectory+'/')!=0,"Catalog root must not be inside a receiver session");
    }
    for (std::size_t i=0;i<o.receiver.sources.size();++i) {
        const auto &source=o.receiver.sources[i]; const auto found=receivers.find(source.id);
        Require(found!=receivers.end() && found->second->sourceEndConfirmed==(source.inputProtocol=="ICE") && (!found->second->sourceUnusedConfirmed || (source.inputProtocol=="FDT" && !found->second->receivedBytes && !found->second->frames && !found->second->files)) && found->second->outputDirectory==source.outputRoot+"/getdaq-"+s.sessionId+"-source-"+std::to_string(i),"Catalog source ID/session/directory mapping differs");
    }
}
std::string CatalogPublicationTarget(const ReceiverCatalogOptions &o) {
    return ReceiverCatalogPublicationTarget(o.receiver.catalogRoot,o.receiver.deploymentId,o.experiment,o.runNumber);
}
std::string CatalogReplacementBackupPath(const std::string &target,const std::string &operationId) {
    SupervisorOperationId(operationId);
    return target+".replaced-"+ConfigurationSha256("getdaq.catalog-replacement-backup:1\n"+operationId).substr(0,24);
}
std::string CatalogReplacementStagePath(const std::string &target,const std::string &operationId) {
    SupervisorOperationId(operationId);
    return target+".pending-"+ConfigurationSha256("getdaq.catalog-replacement-stage:1\n"+operationId).substr(0,24);
}
}

using namespace ReceiverCatalogInternal;

std::string EncodeReceiverCatalogRequest(const ReceiverCatalogOptions &o) {
    CheckOptions(o);
    return "{\"schema\":"+std::string(o.stagedForReplacement ? "2" : "1")+",\"kind\":\"getdaq.receiver-catalog-request\",\"operation_id\":"+EncodeJsonString(o.operationId)+",\"experiment\":"+EncodeJsonString(o.experiment)+
        ",\"run_number\":"+std::to_string(o.runNumber)+",\"plan\":"+EncodeOptions(o.receiver,o.executableSha256)+",\"stopped\":"+EncodeSupervisorStatus(o.stopped)+
        (o.stagedForReplacement ? ",\"staged_for_replacement\":true" : "")+"}\n";
}
std::string ReceiverCatalogRequestKey(const ReceiverCatalogOptions &o) { return ConfigurationSha256("getdaq.receiver-catalog-request:1\n"+EncodeReceiverCatalogRequest(o)); }
std::string ReceiverCatalogPublicationTarget(const std::string &catalogRoot,const std::string &deploymentId,
                                              const std::string &experiment,unsigned int runNumber) {
    Path(catalogRoot);Component(deploymentId,128);Component(experiment,80);
    Require(runNumber<unsigned(std::numeric_limits<int>::max()),"Catalog Run number outside range");
    std::ostringstream number;number<<"run_"<<std::setw(4)<<std::setfill('0')<<runNumber;
    const auto target=catalogRoot+'/'+experiment+'/'+number.str()+'/'+deploymentId;Path(target);return target;
}
std::string ReceiverCatalogReplacementStagePath(const std::string &target,const std::string &operationId) {
    Path(target);return CatalogReplacementStagePath(target,operationId);
}
std::string ReceiverCatalogReplacementCandidatePath(const std::string &target,const std::string &operationId) {
    Path(target);SupervisorOperationId(operationId);
    return target+".candidate-"+ConfigurationSha256("getdaq.catalog-replacement-candidate:1\n"+operationId).substr(0,24);
}
std::string ReceiverCatalogReplacementStagePath(const ReceiverCatalogOptions &o) {
    Require(o.stagedForReplacement,"Replacement stage path requires explicit replacement staging");
    CheckOptions(o);
    return ReceiverCatalogReplacementStagePath(CatalogPublicationTarget(o),o.operationId);
}
std::string ReceiverCatalogTargetPath(const ReceiverCatalogOptions &o) {
    CheckOptions(o);
    return CatalogPublicationTarget(o);
}
std::string ReceiverCatalogReplacementCandidatePath(const ReceiverCatalogOptions &o) {
    CheckOptions(o);
    return ReceiverCatalogReplacementCandidatePath(CatalogPublicationTarget(o),o.operationId);
}
ReceiverCatalogOptions DecodeReceiverCatalogRequest(const std::string &bytes) {
    Require(bytes.size()<=1024*1024,"Catalog request exceeds 1 MiB"); const auto value=ParseStrictJson(bytes);
    const auto schema=Unsigned(value.At("schema"));
    if (schema==1) ExactSupervisorFields(value,{"schema","kind","operation_id","experiment","run_number","plan","stopped"});
    else if (schema==2) ExactSupervisorFields(value,{"schema","kind","operation_id","experiment","run_number","plan","stopped","staged_for_replacement"});
    else throw std::invalid_argument("Unsupported catalog request schema/kind");
    Require(String(value.At("kind"))=="getdaq.receiver-catalog-request","Unsupported catalog request schema/kind");
    ReceiverCatalogOptions result; const auto plan=DecodeSupervisorPlan(value.At("plan")); result.receiver=plan.options; result.executableSha256=plan.executableSha256;
    result.operationId=String(value.At("operation_id")); result.experiment=String(value.At("experiment")); result.runNumber=SupervisorUnsigned32(value.At("run_number"));
    result.stopped=DecodeSupervisorStatus(value.At("stopped"));
    if (schema==2) { Require(Boolean(value.At("staged_for_replacement")),"Schema 2 catalog request must explicitly stage a replacement"); result.stagedForReplacement=true; }
    CheckOptions(result); return result;
}
}
