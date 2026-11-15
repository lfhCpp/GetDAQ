#ifndef GETDAQ_RECOVERYSERVICE_H
#define GETDAQ_RECOVERYSERVICE_H
#include "httpserver.h"
#include <memory>
namespace GetDAQ {
// 独立于原ECC的低频恢复入口；原生CLI执行补齐，Qt只收发有界元数据。
class RecoveryService : public QObject {
    Q_OBJECT
public:
    explicit RecoveryService(const std::string &configurationFile,QObject *parent=nullptr);
    ~RecoveryService() override;
    void Start();
    bool Active() const;
    unsigned int Port() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
