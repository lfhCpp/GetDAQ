// Description:  Multithread
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20240424 09:37
//           By: Fenhua Lu
//     Update #: 3.7


#include "multithread.h"
#include <mainwindow.h>
#include <QDebug>
#include <QTime>
#include "analyse.h"
#include "widgetplot2d.h"
#include "savethread.h"

//启用快速 ACK  使用底层操作系统提供的接口与库来实现
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>

//#define ReceiveAndSave


#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"
Multithread* Multithread::fMultithread = nullptr;//类内静态变量初始化
QTcpSocket* Multithread::fDataSocket= nullptr;
SaveThread* Multithread::fSaveThread= nullptr;
#ifdef ReceiveAndSave
QTime* Multithread::lastSendTime= nullptr;
#endif

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
Multithread::Multithread(){

    this->fDataSocket=new QTcpSocket();//创建socket对象
    this->fDataSocket->setReadBufferSize(1024*1024*1024);//1GB缓冲区
    this->fDataSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,1024*1024*1024);//接收缓冲区
    this->fDataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);//禁用Nagle算法

//    int tcpSocket = fDataSocket->socketDescriptor(); // 获取底层套接字描述符
//    int enableQuickAck = 1; // 启用快速 ACK
//    setsockopt(tcpSocket,IPPROTO_TCP, TCP_QUICKACK, &enableQuickAck, sizeof(enableQuickAck));


    this->fMultithread=this;
    SaveFile= nullptr;
    this->fSaveThread =SaveThread::GetInstance();//保存文件多线程


#ifdef ReceiveAndSave
    lastSendTime=new QTime();
    lastSendTime->start();//计时器

    //发送数据至示波器
    Analyse* analyse =Analyse::GetInstance();
    QObject::connect(this, SIGNAL(SentToPlot(QByteArray)), analyse, SLOT(ShowSlot(QByteArray)));

#endif
    MainWindow* mainWindow =MainWindow::GetInstance();
    //关闭文件
    QObject::connect(mainWindow, &MainWindow::StopReceiveData, this,  &Multithread::CloseFileSlot);


    //发送开始发送数据信号
    QObject::connect(this, SIGNAL(StartSendData()),mainWindow, SLOT(StartSendDataSlot()));
    //发送停止发送数据信号
    QObject::connect(this, SIGNAL(StopSendData()),mainWindow, SLOT(StopSendDataSlot()));

}

Multithread* Multithread::GetInstance(){
    if (fMultithread==nullptr) {
      fMultithread = new Multithread();
    }
    return fMultithread;
}

Multithread::~Multithread(){
    cout<<"~~Multithread";

    if(fDataSocket!=nullptr){
        delete fDataSocket;
        fDataSocket=nullptr;
    }
    if(SaveFile!=nullptr){
        SaveFile->flush();
        SaveFile->close();
        delete SaveFile;
        SaveFile=nullptr;
    }
    if(fSaveThread!=nullptr){
        delete fSaveThread;
        fSaveThread=nullptr;
    }
}
void Multithread::CreateFILE(){

    Multithread::CloseFileSlot();//判断并关闭上一个文件

    //保存数据
    MainWindow* mainWindow =MainWindow::GetInstance();
    QString FilePath=mainWindow->GetFilePath();
    //使用当前时间设置文件名
    QString Filename=QDateTime::currentDateTime().toString("MM-dd_hh_mm_ss");
    QString Name=".bin";
    //SaveFile->open(QIODevice::WriteOnly);
    SaveFile=new QFile;
    if(mainWindow->MainWindow::GetFilename()=="NULL"){
        this->SaveFile->setFileName(FilePath+Filename+Name);
    }
    else{
        this->SaveFile->setFileName(FilePath+mainWindow->MainWindow::GetFilename()+Name);
        mainWindow->MainWindow::SetFilename("NULL");//设定文件名后只生效一次，防止文件覆盖
    }


    if(!SaveFile->open(QIODevice::WriteOnly|QIODevice::Unbuffered)){//Unbuffered绕过设备中的任何缓冲区。
        qDebug()<<"Open file error!";
        //return;
    }
    else{
        qDebug()<<"Create file successfully,ready for recieve the data from ADC!";
    }
}



void Multithread::run(){

#ifdef ReceiveAndSave
    MainWindow* mainWindow =MainWindow::GetInstance();

    if(mainWindow->WhetherSave()){//return OnlyShow

        //断开连接防止重复触发
        QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);
        //准备读取数据
        QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::CalculateThresholdSlot);

        qDebug()<<"OnlyShow";

    }
    else{

        this->Multithread::CreateFILE();//创建文件
        //断开连接防止重复触发
        QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);
        //准备读取数据
        //QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SentToSaveSlot);//多线程,接收数据与保存数据分离
        QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SaveDataSlot);//同一线程
        //QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SaveToBufferSlot);//大buffer

        qDebug()<<"SaveData";

        ////启动保存文件多线程
        this->fSaveThread =SaveThread::GetInstance(); //
        this->fSaveThread->start();                    //开始线程

    }
#else

    MainWindow* mainWindow =MainWindow::GetInstance();
    if(mainWindow->WhetherSave()){//return OnlyShow
        qDebug()<<"OnlyShow start!";
        this->fSaveThread->start();
    }
    else{
        this->Multithread::CreateFILE();//创建文件
        this->fSaveThread->SetSaveFile(this->SaveFile);
        //断开连接防止重复触发
        QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);//！！！！！！！！！！！！！！！！
        QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SentReadyReadSlot);//多线程,接收数据与保存数据分离
        qDebug()<<"Save start!";
        this->fSaveThread->start(); //开始保存文件线程

    }

#endif

    // 检查是否已经连接
    if(this->fDataSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug()<<"Multithread::run() start!";
    }
    else{
        qDebug()<<"Multithread::run() cann't start!";
    }


}//Multithread::run() end here

void Multithread::CloseFileSlot(){//关闭文件
    if(SaveFile!= nullptr){
       SaveFile->write(fDataSocket->readAll()); // 将数据写入文件中
       SaveFile->flush();
       SaveFile->close();
       delete SaveFile;
       SaveFile=nullptr;
       qDebug()<<"SaveFile!= nullptr Save file close!";
    }
    else{
          fDataSocket->readAll();//清空套接字缓存
    }
}

#ifdef ReceiveAndSave
void Multithread::SaveDataSlot(){

    static const int bufferSize = 20480*10240*2; // 20k 定义缓冲区大小，根据实际情况调整

    if (fDataSocket->bytesAvailable() <= bufferSize) {
          return;  // 大于2000个字节，才处理数据
      }

    //emit StopSendData();////万兆以太网停止发送数据信号
    emit  StartSendData();////万兆以太网开始发送数据信号
    //断开连接防止重复触发
    QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);

    QByteArray data;//QByteArray STL 堆内开启

    while (fDataSocket->bytesAvailable() >= bufferSize) {
            data = fDataSocket->read(bufferSize); // 每次读取 bufferSize 大小的数据
            SaveFile->write(data); // 将数据写入文件中
    }

   data = fDataSocket->readAll(); // 读取剩余不足 bufferSize 的数据
   SaveFile->write(data); // 写入文件
   SaveFile->flush(); // 刷新文件缓冲区，确保数据被保存到磁盘 阻塞

   QString size = QString::number(SaveFile->size()/(1024*1024),'f',2);
   qDebug() << "BytesWritten: " << size << " MB";


  // QTimer::singleShot(100, [=](){

   //emit StopSendData();////万兆以太网停止发送数据信号
   emit  StartSendData();////万兆以太网开始发送数据信号
   //});//QTimer::singleShot end here

    //重新连接
    QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SaveDataSlot);

}

void Multithread::SentToSaveSlot(){

    static const int bufferSize = 20480; // 20k 定义缓冲区大小，根据实际情况调整

    if (fDataSocket->bytesAvailable() <= bufferSize) {
          return;  // 大于2000个字节，才处理数据
      }

    //断开连接防止重复触发
    QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);

    QByteArray data;//QByteArray STL 堆内开启


    data = fDataSocket->readAll(); // 读取数据

    emit SentToSave(data);


    //重新连接
    QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SentToSaveSlot);

}

void Multithread::CalculateThresholdSlot(){//在线显示

    static const int bufferDataSize = 20480; // 20k 定义缓冲区大小，根据实际情况调整


    if (fDataSocket->bytesAvailable() <= bufferDataSize) {
          return;  // 大于2000个字节，才处理数据
      }



    //断开连接防止重复触发
    QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);

    QByteArray data;//QByteArray STL 堆内开启

    while (fDataSocket->bytesAvailable() >= bufferDataSize) {
            data = fDataSocket->read(bufferDataSize); // 每次读取 bufferDataSize 大小的数据
    }
    // 检查是否满足发送时间间隔
    int elapsed = lastSendTime->elapsed();
        if (elapsed >= 500) {//每0.s最多给示波器发送一次数据

            emit SentToPlot(data); // 把数据发送给示波器
            //QString size = QString::number(SaveFile->size()/(1024*1024),'f',2);
            //emit EmitFileSize(size);
            //qDebug() << "BytesWritten: " << size << " MB";
            //qDebug() << "BytesWritten: ";
            lastSendTime->restart();
        }


    //重新连接
    QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::CalculateThresholdSlot);

}

void Multithread::SaveToBufferSlot() {//大缓存


    //static const qint64 totalDataSize = 1LL * 1024 * 1024 * 1024; // 1GB
    //static const qint64 bufferSize = 512 * 1024 * 1024; // 500MB - 1GB
    const qint64 totalDataSize = 1LL * 1024 * 1024 * 1024; // 1GB
    const qint64 bufferSize = 512 * 1024 * 1024; // 500MB - 1GB
    static QBuffer dataBuffer;
    static qint64 totalBytesReceived = 0;

    if (fDataSocket->bytesAvailable() <= bufferSize) {
          return;  // 大于bufferSize，才处理数据
      }

    //断开连接防止重复触发
    QObject::disconnect(this->fDataSocket, &QTcpSocket::readyRead, 0, 0);


    if (!dataBuffer.isOpen()) {
        if (dataBuffer.open(QIODevice::WriteOnly)) {
            qDebug() << "Buffer opened successfully.";
        } else {
            qDebug() << "Failed to open buffer.";
            return;
        }
        // 预分配缓冲区大小
        dataBuffer.buffer().resize(bufferSize);
    }

    while (fDataSocket->bytesAvailable() >= bufferSize) {
        qint64 bytesRead = fDataSocket->read(dataBuffer.buffer().data(), bufferSize); // 每次读取 bufferSize 大小的数据
        totalBytesReceived += bytesRead;


        // 处理数据，将数据写入缓冲区
         dataBuffer.seek(0);
         SaveFile->write(dataBuffer.buffer().constData(), bytesRead);
         // 刷新文件缓冲区，确保数据被保存到磁盘
         SaveFile->flush();

        if (totalBytesReceived >= totalDataSize) {
            // 达到总数据量，结束接收数据
            qDebug() << "Total Bytes Received: " << totalBytesReceived / (1024 * 1024 * 1024) << " GB";
            dataBuffer.close();
            qDebug() << "Buffer closed.";
            return;
        }
    }

    // 重新连接以允许后续的数据接收
    QObject::connect(this->fDataSocket, &QTcpSocket::readyRead, this, &Multithread::SaveToBufferSlot);
}
#endif
