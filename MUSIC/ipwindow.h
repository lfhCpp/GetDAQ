#ifndef IPWINDOW_H
#define IPWINDOW_H

#include <QWidget>
#include<QTcpSocket>
#include <mainwindow.h>
#include <multithread.h>

class MainWindow;

namespace Ui {
class IPWindow;
}

class IPWindow : public QWidget
{
    Q_OBJECT

public:
    explicit IPWindow(QWidget *parent = 0);
    ~IPWindow();
    static IPWindow* GetInstance();//静态成员函数只能访问静态成员变量



signals:

    //QObject::connect(&myWidget,&MyWidget::MySignal,信号接收者，槽函数);

    void ControlCmdNetLED();

    void ControlDataNetLED();

    void DisableCmdNetLED();

    void DisableDataNetLED();

    void SendcmdSocketByte(QByteArray byte);//处理千兆网口发送的数据

private slots:

    void on_CmdNetConnect_clicked();//连接网口

    void CmdNetConnect();

    void on_DataNetConnect_clicked();

    void DataNetConnect();

    void on_CmdNetDisconnect_clicked();

    void on_DataNetDisconnect_clicked();

private:

    Ui::IPWindow *ui;

    static IPWindow *fIPWindow;

    static  Multithread *fMultithread;

};

#endif // IPWINDOW_H


