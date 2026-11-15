#ifndef GETDAQ_TRANSFERWINDOW_H
#define GETDAQ_TRANSFERWINDOW_H
#include "httpclient.h"
#include "transferprotocol.h"
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
namespace Ui {class TransferWindow;}
class TransferWindow : public QDialog {
    Q_OBJECT
public:
    explicit TransferWindow(QWidget *parent=nullptr);
    ~TransferWindow() override;
private slots:
    void CheckSource();
    void Submit();
    void Observe();
    void Reconcile();
    void Resume();
    void Invalidate();
private:
    void Recover(bool resume);
    Ui::TransferWindow *ui;
    HttpClient *fHttp;
    QByteArray fCredential;
    GetDAQ::TransferIdentity fIdentity;
    GetDAQ::TransferRequest fRequest;
    QString fSourceKey;
    bool fBusy=false,fReady=false,fHaveRequest=false,fCredentialFault=false;
    quint64 fGeneration=0;
    QElapsedTimer fFresh;
    QTimer fPoll;
    QUrl Url(const QString &path) const;
    QMap<QByteArray,QByteArray> Headers();
    void Ready(bool ready);
    void Result(const QByteArray &body,QString error,bool submission);
};
#endif
