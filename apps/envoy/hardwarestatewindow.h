#ifndef GETDAQ_HARDWARESTATEWINDOW_H
#define GETDAQ_HARDWARESTATEWINDOW_H
#include "eccsystemclient.h"
#include <QDialog>
#include <QElapsedTimer>
namespace Ui {class HardwareStateWindow;}
class HardwareStateWindow : public QDialog {
    Q_OBJECT
public:
    explicit HardwareStateWindow(const EccSystemClientOptions &options,QWidget *parent=nullptr);
    ~HardwareStateWindow() override;
private slots:
    void Inspect();
    void Invalidate();
private:
    Ui::HardwareStateWindow *ui;
    HttpClient *fHttp;
    QElapsedTimer fFresh;
    quint64 fGeneration=0;
    QByteArray fCredential;
    bool fBusy=false;
    QMap<QByteArray,QByteArray> Headers();
    QUrl Url(const QString &path) const;
    void Failure(const QString &message);
};
#endif
