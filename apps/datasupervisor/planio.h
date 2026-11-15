#ifndef GETDAQ_SUPERVISORPLANIO_H
#define GETDAQ_SUPERVISORPLANIO_H
#include "datasupervisor.h"
#include "strictjson.h"
#include <set>
namespace GetDAQ {
struct SupervisorPlanDocument {
    DataSupervisorOptions options;
    std::string executableSha256;
};
// 严格类型/字段解析；不调用网络、不写文件，不把远端IPv4误认为本机接口。
SupervisorPlanDocument DecodeSupervisorPlan(const JsonValue &document);
SupervisorPlanDocument DecodeSupervisorPlan(const std::string &bytes);
void ExactSupervisorFields(const JsonValue &object,const std::set<std::string> &fields);
unsigned int SupervisorUnsigned32(const JsonValue &value);
}
#endif
