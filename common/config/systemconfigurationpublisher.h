#ifndef GETDAQ_SYSTEMCONFIGURATIONPUBLISHER_H
#define GETDAQ_SYSTEMCONFIGURATIONPUBLISHER_H
#include "xcfgdocument.h"
#include <QJsonObject>
class SystemConfigurationPublisher {
public:
    // 发布到新本地目录；编译前后核对来源，清单最后写入，不连接硬件。
    static bool Publish(const XcfgDocument &document,const QString &profile,const QString &directory,QJsonObject *result,QString *error);
};
#endif
