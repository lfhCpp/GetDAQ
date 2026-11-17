#ifndef GETDAQ_COBO_CONNECTION_WINDOW_H
#define GETDAQ_COBO_CONNECTION_WINDOW_H
#include <QDialog>
#include <QProcess>
#include <QElapsedTimer>
class QLineEdit;class QSpinBox;class QLabel;class QTreeWidget;class QPushButton;
// Direct, bounded read-only RPC observations. Never constructs an ECC Run.
class CoBoConnectionWindow:public QDialog {
    Q_OBJECT
public:
    explicit CoBoConnectionWindow(QWidget *parent=nullptr,bool connectNow=false);
protected:
    void reject() override;
private:
    QLineEdit *fAddress,*fObject,*fPrograms;
    QSpinBox *fPort;
    QLabel *fStatus;
    QTreeWidget *fValues;
    QPushButton *fConnect,*fDisconnect;
    QProcess *fProcess;
    QString fProxy,fProgram;
    int fStep=-1;
    quint64 fGeneration=0;
    bool fBusy=false;
    QElapsedTimer fFresh;
    void Inspect();
    void Next();
    void Finished(int code,QProcess::ExitStatus status);
    void Invalidate();
    void Fail(const QString &message);
};
#endif
