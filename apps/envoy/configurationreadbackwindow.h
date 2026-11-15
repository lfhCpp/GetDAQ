#ifndef GETDAQ_CONFIGURATIONREADBACKWINDOW_H
#define GETDAQ_CONFIGURATIONREADBACKWINDOW_H
#include <QDialog>
#include <QElapsedTimer>
namespace Ui {class ConfigurationReadbackWindow;}
class ConfigurationReadbackWindow : public QDialog {
    Q_OBJECT
public:
    explicit ConfigurationReadbackWindow(QWidget *parent=nullptr);
    ~ConfigurationReadbackWindow();
    void Bind(const QString &snapshot,quint64 generation,bool connected);
    void Observe(const QByteArray &body,const QString &error,quint64 generation);
signals:
    void InspectSignal(quint64 generation);
private:
    Ui::ConfigurationReadbackWindow *ui;
    QString fSnapshot,fError;
    QByteArray fBody;
    QElapsedTimer fClock;
    qint64 fObservedAt=-1;
    quint64 fGeneration=0;
    bool fConnected=false;
    void Render();
};
#endif
