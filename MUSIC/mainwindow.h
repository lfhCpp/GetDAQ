#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<ipwindow.h>
#include<cmdwindow.h>
#include <QHostAddress>
#include <QMessageBox>
#include<QLabel>
#include <multithread.h>//TCP数据多线程
#include "widgetplot2d.h"
#include "analyse.h"//多线程
#include "savethread.h"//多线程



namespace Ui {
class MainWindow;
}

class IPWindow;

class Analyse;

class MainWindow : public QMainWindow
{
    Q_OBJECT


signals:

    void StopReceiveData();//停止保存文件信号

    void EmitReadSignal(QString datfilename);//解析文件信号

    void SaveDataSignal();//保存文件信号

    void TriggerSignal();//发送定时触发信号

    void EmitConnectCmdSignal();//连接千兆网信号

    void EmitConnectDataSignal();//连接万兆网信号


public:

    explicit MainWindow(QWidget *parent = 0);//explicit防止隐式转换

    explicit MainWindow(const MainWindow &ReMainWindow);

    void InitializePar();

    ~MainWindow();

    static MainWindow* GetInstance();//不会调用拷贝构造函数

    QTcpSocket* GetcmdSocket(){return fcmdSocket;}

    WidgetPlot2D* GetWidgetPlot2D(){return fWidgetPlot2D;}//获取示波器UI进指针

    void NewcmdSocket(){this->fcmdSocket=new QTcpSocket();}

    QString GetFilename(){return Filename;}

    void SetFilename(QString name){this->Filename=name;}

    void SetThreshold(double* b,double* s);//基线与方差

    QString GetFilePath(){return srcDirPath;}

    bool WhetherConnect(QTcpSocket* fSocket){return fSocket->waitForConnected(1000);}

    bool WhetherSave(){return OnlyShow;}

    void setLED(QLabel* label,int color,int size);

    int GetTestNumber(){return TestNumber;}//测试变量函数

private slots:

    void OpenCmdNetLED();//状态灯控制

    void OpenDataNetLED();

    void CloseCmdNetLED();

    void CloseDataNetLED();

    void on_ConnectNet_triggered();//连接网口

    void on_Cmd_triggered();//实例化：new IPWindow();

    void on_ReceiveData_clicked();//接受数据按钮

    void on_StopReceive_clicked();//停止接受按钮

    void on_ModeBox_activated(int);//通道模式选择

    void on_SavePath_triggered();//保存路径

    void on_SetFilename_clicked();//设置文件名

    void on_commandLinkButton_clicked();//示波器Online

    void ShowCmdSlot(QByteArray byte);//处理千兆网口获取的数据板信息

    void timer1Handler();

    void on_ana_file_triggered();//解析文件按钮

    void on_SetThreshold_clicked();//阈值计算、不保存文件

    void on_SetTh_clicked();//发送设置的阈值大小

    void TriggerTimerSlot(){emit TriggerSignal();}//发送定时触发信号


public  slots:

    void ShowFileSizeSlot(QString size);//处理万兆网口获取的文件大小信息

    void StartSendDataSlot();//万兆以太网是否发送数据

    void StopSendDataSlot();//万兆以太网是否发送数据

private:

    static MainWindow *fMainWindow;

    static QTcpSocket* fcmdSocket;//千兆套接字

    static Multithread *fMultithread;//万兆套接字、保存数据

    static Analyse *fAnalyse;//分析

    static QThread *fThread_Ana;//分析数据线程

    static SaveThread *fSaveThread;//保存数据线程

    static WidgetPlot2D *fWidgetPlot2D;//示波器界面

    Ui::MainWindow *ui;

    IPWindow *fIPWindow;//网口连接界面

    CmdWindow  *fCmdWindow ;//命令行连接界面

    QString srcDirPath;//保存路径

    QString Filename;  //文件名

    double Baseline[8];//基线

    double Sigma[8];//方差

    double Threshold[8];//阈值

    int CalculationTimes;//计算阈值次数

    bool OnlyShow;//是否仅进行阈值计算与在线监视

    int TestNumber;//测试变量

    QTimer* fTriggerTimer=nullptr; // 定时器对象


    //QTimer *timer1;


};

#endif // MAINWINDOW_H
