#ifndef GETDAQ_REPOSITORYCREDENTIAL_H
#define GETDAQ_REPOSITORYCREDENTIAL_H
#include <memory>
#include <string>
namespace GetDAQ {
class RepositoryCredential {
public:
    static void Generate(const std::string &path);
    explicit RepositoryCredential(const std::string &path);
    ~RepositoryCredential();
    bool Authorize(const std::string &header) const;
private:
    class Impl;std::unique_ptr<Impl> fImpl;
};
}
#endif
