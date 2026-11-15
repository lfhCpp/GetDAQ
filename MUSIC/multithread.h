#ifndef MULTITHREAD_H
#define MULTITHREAD_H

#include <QObject>
#include <QThread>
#include "savethread.h"
#include <QTcpSocket>
#include <QDebug>
#include <QFileDialog>
//#define ReceiveAndSave

class Multithread : public QThread
{
    Q_OBJECT
public:

    explicit Multithread();

    ~Multithread();

    static Multithread* GetInstance();//静态成员函数只能访问静态成员变量

    QTcpSocket* GetDataSocket(){return fDataSocket;}

    QFile* GetQFile(){return SaveFile;}

    bool WhetherConnect(QTcpSocket* fSocket){return fSocket->waitForConnected(1000);}

    void run();//线程处理函数

    void CreateFILE();//创建文件

signals:

    void ReadyReadSignal();

    void SentToPlot(QByteArray byte);// 把数据发送给示波器

    void SentToSave(const QByteArray &byte);// 把数据发送给另一线程保存

    void StartSendData();//万兆网开始发送数据信号

    void StopSendData();//万兆网停止发送数据信号

public slots:

    void CloseFileSlot();//关闭文件

    void SentReadyReadSlot(){emit ReadyReadSignal();}

#ifdef ReceiveAndSave

    void SaveDataSlot();//处理readyread数据

    void SentToSaveSlot();//处理readyread数据，并发送给另一线程

    void CalculateThresholdSlot();//计算阈值

    void SaveToBufferSlot();//处理大buffer数据


#endif


private:
   #ifdef ReceiveAndSave
    static QTime *lastSendTime;
   #endif
    static SaveThread *fSaveThread;
    static Multithread *fMultithread;
    static QTcpSocket* fDataSocket;
    QFile *SaveFile;


};

#endif // MULTITHREAD_H
