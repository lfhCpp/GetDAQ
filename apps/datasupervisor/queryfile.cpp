#include "queryfile.h"
#include "supervisorio.h"
namespace GetDAQ {
std::string ReadSupervisorQueryFile(const std::string &path,std::size_t maximum) {
    return SupervisorIO::ReadFile(path,maximum);
}
}
