#ifndef GETDAQ_SUPERVISORQUERYFILE_H
#define GETDAQ_SUPERVISORQUERYFILE_H
#include <cstddef>
#include <string>
namespace GetDAQ {
// Read a bounded regular request file without following a symbolic link.
std::string ReadSupervisorQueryFile(const std::string &path,std::size_t maximum);
}
#endif
