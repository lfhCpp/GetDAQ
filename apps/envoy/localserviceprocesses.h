#ifndef GETDAQ_LOCAL_SERVICE_PROCESSES_H
#define GETDAQ_LOCAL_SERVICE_PROCESSES_H
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QStringList>
struct LocalServiceProcess {qint64 pid=0;QString executable;QByteArray started;};
QVector<LocalServiceProcess> FindLocalServiceProcesses(const QString &program,const QString &config);
QVector<LocalServiceProcess> FindLocalArgumentProcesses(const QString &program,const QByteArray &argument,const QString &path);
QVector<LocalServiceProcess> FindLocalReceiverProcesses(const QString &program,const QString &supervisorConfig);
QStringList LocalServiceConfigurations(const QString &program);
bool SameLocalServiceProcess(const LocalServiceProcess &process);
#endif
