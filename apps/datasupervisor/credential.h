#ifndef GETDAQ_SUPERVISORCREDENTIAL_H
#define GETDAQ_SUPERVISORCREDENTIAL_H
#ifndef _WIN32
#include "supervisorio.h"
#endif
#include <memory>
#include <string>
namespace GetDAQ {
// Credentials are read from an owner-only regular file and never logged.
class SupervisorCredential {
public:
    explicit SupervisorCredential(const std::string &path);
    ~SupervisorCredential();
    SupervisorCredential(const SupervisorCredential &) = delete;
    SupervisorCredential &operator=(const SupervisorCredential &) = delete;
    void Check() const;
    bool Authorize(const std::string &authorizationHeader) const;
    std::string Token() const; // 仅供客户端构造认证头，调用方不得记录。
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
