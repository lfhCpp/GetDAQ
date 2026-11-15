#ifndef GETDAQ_SYSTEMPREPARATIONAUDITCAPTURE_P_H
#define GETDAQ_SYSTEMPREPARATIONAUDITCAPTURE_P_H

#include <string>

namespace GetDAQ {

// Capture a completed Configure audit only while its file identity is stable.
std::string ReadSystemPreparationConfigureAudit(const std::string &path);

}

#endif
