// Read-only discovery using the same implementation as the unified dialog.
#include "existingeccconnection.h"
#include "deploymentcommon.h"
#include <QCoreApplication>
#include <QDebug>
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc!=3 && argc!=4)return 2;try{RunConfig base=RunConfig::Default();if(argc==4){QString error;Deployment::Require(base.Load(QString::fromLocal8Bit(argv[3]),&error),error);}Deployment::OperationScope scope(nullptr,20000);const auto result=CheckExistingEcc("Existing ECC",QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]).toInt(),base);qInfo().noquote()<<result.protocol<<result.status<<"port="<<(result.configuration.nativeEcc?result.configuration.nativeClient.port:result.configuration.nodes[0].eccPort);qInfo().noquote()<<result.detail;return 0;}catch(const std::exception &error){qCritical().noquote()<<error.what();return 1;}}
