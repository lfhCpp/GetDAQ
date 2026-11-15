#ifndef GETDAQ_CONFIGURATIONREPOSITORY_H
#define GETDAQ_CONFIGURATIONREPOSITORY_H
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
// 配置专用持久仓库；32KiB内容块与只读版本，不接受原始DAQ事件。
class ConfigurationRepository {
public:
    static void Initialize(const std::string &root);
    explicit ConfigurationRepository(const std::string &root);
    ~ConfigurationRepository();
    ConfigurationRepository(const ConfigurationRepository &)=delete;
    ConfigurationRepository &operator=(const ConfigurationRepository &)=delete;
    std::string IdentityJson() const;
    void PutObject(const std::string &sha256,const std::string &bytes);
    bool HasObject(const std::string &sha256) const;
    std::string ReadObject(const std::string &sha256) const;
    std::string RegisterDeployment(const std::vector<std::string> &manifestObjects);
    std::string DeploymentManifest(const std::string &releaseSha256) const;
    // 提交计划本身也由内容块组成；提交完成后返回登记凭据，重复提交核对原结果。
    std::string Commit(const std::vector<std::string> &planObjects);
    std::string VersionJson(const std::string &manifestSha256) const;
    std::string ListJson() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
