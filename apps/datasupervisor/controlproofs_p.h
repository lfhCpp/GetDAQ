#ifndef GETDAQ_SUPERVISORCONTROLPROOFS_P_H
#define GETDAQ_SUPERVISORCONTROLPROOFS_P_H

#include "supervisorio.h"
#include "controlprotocol.h"

#include <string>
#include <sys/stat.h>

namespace GetDAQ {

void WriteOrVerifyRetirementFile(const std::string &path,const std::string &bytes,
                                 bool completion,bool write);

class SupervisorHandoffProof {
public:
    SupervisorHandoffProof(const std::string &file,const std::string &expectedSha);
    void Check() const;

private:
    std::string fPath,fBytes,fRequestBytes,fSuccessorBytes,fCycleSha;
    bool fRetirement=false;
    SupervisorIO::Directory fDirectory;
    struct stat fIdentity{},fRequestIdentity{},fSuccessorIdentity{},fCycleIdentity{};
};

}

#endif
