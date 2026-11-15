#include "remotehandoffevidence_delivery_p.h"
#include "remotehandoffevidence_p.h"
#include "planio.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace GetDAQ { namespace RemoteEvidenceInternal {
using namespace SupervisorIO;

void ApplyDeliveryHistory(std::vector<RemoteHandoffNodeEvidence> &nodes,
                          const std::vector<JsonValue> &rows, bool original) {
        std::map<std::string,std::size_t> indices;for(std::size_t i=0;i<nodes.size();++i)indices[nodes[i].request.operationId]=i;
        std::vector<unsigned int> local(nodes.size(),1);std::vector<bool> sent(nodes.size(),false),accepted(nodes.size(),false);
        std::vector<bool> completed(nodes.size(),false);
        std::vector<std::string> repairedProofs(nodes.size());
        std::vector<std::string> repairedHandoffOperations(nodes.size()),repairedHandoffRequests(nodes.size());
        bool ended=false;
        for(const auto &row:rows) {
            const auto phase=String(row.At("phase"));Require(!ended,"Handoff journal has records after its terminal event");
            if(phase=="complete" || phase=="unknown") {
                if(phase=="complete")Require(std::all_of(completed.begin(),completed.end(),[](bool value){return value;}),"Complete handoff journal lacks every original node proof");
                ended=true;continue;
            }
            if(phase=="before_post" || phase=="node_explicit_submit" || phase=="explicit_rejection" || phase=="accepted_reply" || phase=="node_original_observed") {
                const auto found=indices.find(String(row.At("operation_id")));Require(found!=indices.end(),"Handoff journal references an unrelated operation");
                const auto index=found->second;auto &node=nodes[index];
                if(phase=="before_post" || phase=="node_explicit_submit") {
                    Require(!sent[index] && String(row.At("request_key"))==SupervisorControlRequestKey(node.request),"Repeated/different handoff submission evidence");
                    Require((phase=="before_post")==original,"Handoff submission phase differs from its audit kind");sent[index]=true;local[index]=0;
                }else if(phase=="explicit_rejection") {
                    const auto code=Unsigned(row.At("http_status"));
                    Require(sent[index] && !accepted[index] && local[index]==0 && (code==409 || code==412) &&
                        String(row.At("request_key"))==SupervisorControlRequestKey(node.request),"Rejection lacks its exact earlier handoff submission");local[index]=unsigned(code);
                }else {
                    Require(phase=="node_original_observed" || (sent[index] && local[index]==0),"Accepted handoff follows no submission or a rejection");accepted[index]=true;local[index]=0;
                }
            }else if(phase=="node_failed_retirement_completed") {
                ExactSupervisorFields(row,{"phase","agent_id","operation_id","request_key","completion_request","completion_request_key","proof_sha256","no_post_sent"});
                const auto found=indices.find(String(row.At("operation_id")));
                Require(!original && found!=indices.end(),"Failed retirement completion is outside its original parent attempt");
                const auto index=found->second;const auto &node=nodes[index];
                const auto repair=DecodeSupervisorControlRequest(String(row.At("completion_request")));
                Require(node.request.command==SupervisorCommand::RetirePlan && repair.command==SupervisorCommand::CompletePlanRetirement &&
                    EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(repair))==EncodeSupervisorControlRequest(node.request) &&
                    String(row.At("agent_id"))==node.agentId && String(row.At("request_key"))==SupervisorControlRequestKey(node.request) &&
                    String(row.At("completion_request_key"))==SupervisorControlRequestKey(repair) && Boolean(row.At("no_post_sent")) &&
                    !accepted[index],"Failed retirement completion changed its original node/request or repeated an observation");
                repairedProofs[index]=String(row.At("proof_sha256"));Digest(repairedProofs[index]);
                accepted[index]=true;local[index]=0;
            }else if(phase=="node_failed_handoff_completed") {
                ExactSupervisorFields(row,{"phase","agent_id","operation_id","request_key","completion_request","completion_request_key","proof_sha256","no_post_sent"});
                const auto found=indices.find(String(row.At("operation_id")));
                Require(!original && found!=indices.end(),"Failed handoff completion is outside its original parent attempt");
                const auto index=found->second;const auto &node=nodes[index];
                const auto repair=DecodeSupervisorControlRequest(String(row.At("completion_request")));
                Require(node.request.command==SupervisorCommand::HandoffControl && repair.command==SupervisorCommand::CompleteControlHandoff &&
                    EncodeSupervisorControlRequest(OriginalControlHandoffRequest(repair))==EncodeSupervisorControlRequest(node.request) &&
                    String(row.At("agent_id"))==node.agentId && String(row.At("request_key"))==SupervisorControlRequestKey(node.request) &&
                    String(row.At("completion_request_key"))==SupervisorControlRequestKey(repair) && Boolean(row.At("no_post_sent")) &&
                    !accepted[index],"Failed handoff completion changed its original node/request or repeated an observation");
                repairedProofs[index]=String(row.At("proof_sha256"));Digest(repairedProofs[index]);
                repairedHandoffOperations[index]=repair.operationId;repairedHandoffRequests[index]=EncodeSupervisorControlRequest(repair);
                accepted[index]=true;local[index]=0;
            }else if(phase=="node_control_handoff_complete") {
                const auto &receipt=row.At("receipt");const auto found=indices.find(String(receipt.At("operation_id")));
                Require(found!=indices.end(),"Handoff node proof references an unrelated operation");const auto index=found->second;
                std::set<std::string> fields={"agent_id","instance_id","operation_id","request_key","proof_sha256","request"};
                if(receipt.members.count("completion_operation_id"))fields.insert("completion_operation_id");
                if(receipt.members.count("completion_request"))fields.insert("completion_request");
                Require(receipt.members.count("completion_operation_id")==receipt.members.count("completion_request"),"Handoff receipt has incomplete completion metadata");
                ExactSupervisorFields(receipt,fields);
                Require(accepted[index] && local[index]==0 && String(receipt.At("request"))==EncodeSupervisorControlRequest(nodes[index].request) &&
                    String(receipt.At("request_key"))==SupervisorControlRequestKey(nodes[index].request) && String(receipt.At("agent_id"))==nodes[index].agentId &&
                    (repairedProofs[index].empty() || String(receipt.At("proof_sha256"))==repairedProofs[index]) &&
                    ((!repairedProofs[index].empty() && receipt.members.count("completion_operation_id") &&
                      String(receipt.At("completion_operation_id"))==repairedHandoffOperations[index] &&
                      String(receipt.At("completion_request"))==repairedHandoffRequests[index]) ||
                     (repairedProofs[index].empty() && (!receipt.members.count("completion_operation_id") ||
                      (String(receipt.At("completion_operation_id")).empty() && String(receipt.At("completion_request")).empty())))),
                    "Handoff completion proof changed original request");accepted[index]=true;completed[index]=true;local[index]=0;
            }else Require(phase=="intent" || phase=="node_unknown" || phase=="node_complete" || phase=="node_original_not_accepted","Unknown handoff evidence journal phase");
        }
        for(std::size_t i=0;i<nodes.size();++i) {
            if(original)nodes[i].rejection=local[i];
            else if(local[i]==0)nodes[i].rejection=0;
            else if(local[i]!=1 && nodes[i].rejection!=0)nodes[i].rejection=local[i];
        }

}

} }
