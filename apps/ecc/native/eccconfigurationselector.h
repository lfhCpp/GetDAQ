#ifndef GETDAQ_ECCCONFIGURATIONSELECTOR_H
#define GETDAQ_ECCCONFIGURATIONSELECTOR_H
#include <string>
namespace GetDAQ {
// 只建立下一次启动的新部署目录，不修改现有服务或运行状态。
std::string SelectEccConfiguration(const std::string &templateFile,const std::string &selectionFile,
    const std::string &newDirectory);
}
#endif
