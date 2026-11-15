// Read-only probe: never submits configuration or acquisition commands.
#include "runconfig.h"
#include "eccclient.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc!=2)return 2;RunConfig config;QString error;if(!config.Load(QString::fromLocal8Bit(argv[1]),&error)){qCritical().noquote()<<error;return 2;}if(config.simulation || config.nativeEcc || config.nodes.size()!=1)return 2;EccClient client(config.nodes[0],config);QObject::connect(&client,&EccClient::ReplySignal,&app,[&](int,QString operation,int state,int transition,QString failure){if(!failure.isEmpty()){qCritical().noquote()<<failure;app.exit(1);return;}qInfo().noquote()<<operation<<"state="<<state<<"transition="<<transition;app.exit(0);});QTimer::singleShot(10000,&app,[&]{client.Cancel();app.exit(3);});QTimer::singleShot(0,&client,[&]{client.Send("GetState");});return app.exec();}
