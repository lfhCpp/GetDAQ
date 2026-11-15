#ifndef GETDAQ_COLDHANDOFFWINDOW_H
#define GETDAQ_COLDHANDOFFWINDOW_H
#include "httpclient.h"
#include "coldhandoffprotocol.h"
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
namespace Ui {class ColdHandoffWindow;}
class ColdHandoffWindow : public QDialog {
    Q_OBJECT
public:
    explicit ColdHandoffWindow(QWidget *parent=nullptr);
    ~ColdHandoffWindow() override;
    bool SaveReceipt(const QString &path,QString *error=nullptr) const;
    bool LoadIntent(const QString &path,QString *error=nullptr);
private:
    Ui::ColdHandoffWindow *ui;
    HttpClient *fHttp;
    QTimer fPoll;
    QElapsedTimer fFresh;
    GetDAQ::ColdHandoffIdentity fIdentity;
    GetDAQ::ColdHandoffRequest fRequest;
    QByteArray fCredential,fReceipt;
    bool fBusy=false,fReady=false,fHaveRequest=false,fCredentialFault=false;
    quint64 fGeneration=0;
    QUrl Url(const QString &path) const;
    QMap<QByteArray,QByteArray> Headers();
    void Invalidate();
    void Controls();
    void Check();
    void Submit(bool inspect);
    void Observe();
    void Show(const QByteArray &bytes,const QString &error,const QString &taskId);
};
#endif
