#ifndef GETDAQ_APPLICATION_INSTANCE_H
#define GETDAQ_APPLICATION_INSTANCE_H
#include <QObject>
#include <QLocalServer>
#include <QPointer>
class QWidget;
QString LocalInstallationDirectory();
QString StartupConfigurationKey();
bool LoadLocalSiteEnvironment(const QString &installation,QString *error);
// GUI instance control belongs to the binary; never to a daemon or launcher.
class ApplicationInstance:public QObject {
public:
    explicit ApplicationInstance(const QString &installation,QObject *parent=nullptr);
    bool Acquire(const QString &configuration=QString());
    QString Error() const {return fError;}
    void SetWindow(QWidget *window);
private:
    QLocalServer *fServer=nullptr;
    QString fPrefix,fName,fError;
    QPointer<QWidget> fWindow;
    bool fPending=false;
    void Activate();
};
#endif
