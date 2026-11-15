// Description:  SaveThread
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20240424 09:37
//           By: Fenhua Lu
//     Update #: 3.7

#include "savethread.h"
#include <QDebug>
#include <multithread.h>
#include <mainwindow.h>
#include "analyse.h"


#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"
SaveThread*SaveThread::fSaveThread = nullptr;//类内静态变量初始化

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

SaveThread::SaveThread(){
    this->fSaveThread=this;
    mutex=new QMutex(); // 在定义互斥锁
    this->mutex->lock();// 使用互斥锁->加锁
    this->mutex->unlock();//使用互斥锁->解锁


}

SaveThread* SaveThread::GetInstance(){
    if (fSaveThread==nullptr) {
      fSaveThread = new SaveThread();
    }
    return fSaveThread;
}

SaveThread::~SaveThread(){
    if(mutex!=nullptr){
        delete mutex;
        mutex=nullptr;
    }

}

void SaveThread::SaveThreadStart(){


    MainWindow* mainWindow =MainWindow::GetInstance();
    Multithread* multithread =Multithread::GetInstance();
    if(mainWindow->WhetherSave()){//return OnlyShow  只显示波形，不保存数据
        //断开连接防止重复触发   
        QObject::disconnect(mainWindow,&MainWindow::TriggerSignal, 0, 0);
        QObject::disconnect(multithread,&Multithread::ReadyReadSignal, 0, 0);
        connect(mainWindow,SIGNAL(TriggerSignal()),this,SLOT(CalculateThresholdSlot()));
    }
    else {
        //断开连接防止重复触发
        QObject::disconnect(mainWindow,SIGNAL(TriggerSignal()),this,SLOT(CalculateThresholdSlot()));
        QObject::disconnect(multithread,&Multithread::ReadyReadSignal, 0, 0);
        if (mainWindow->GetWidgetPlot2D()->isVisible()) {//示波器界面可见，显示波形并保存数据
              QObject::connect(multithread, SIGNAL(ReadyReadSignal()), this, SLOT(SaveAndShowSlot()));
               //cout<<"示波器界面可见，显示波形并保存数据";
        }
        else{// 界面隐藏  仅仅保存数据
             QObject::connect(multithread, SIGNAL(ReadyReadSignal()), this, SLOT(SaveSlot()));
            // cout<<"界面隐藏  仅仅保存数据";
        }

    }

}

void SaveThread::run(){

    this->SaveThreadStart();

}


void SaveThread::SaveSlot(){


    //QString size = QString::number(this->SaveFile->size()/(1024*1024),'f',2);
    //emit EmitFileSize(size);//会增加丢数

    Multithread* multithread =Multithread::GetInstance();
    static const int bufferSize = 204800;// 200k 定义缓冲区大小，根据实际情况调整
    if (multithread->GetDataSocket()->bytesAvailable() <= bufferSize) {
          return;  // 大于2000个字节，才处理数据
      }

    this->SaveFile->write(multithread->GetDataSocket()->readAll()); // 写入文件
    this->SaveFile->flush(); // 刷新文件缓冲区，确保数据被保存到磁盘 阻塞


}

void SaveThread::SaveAndShowSlot(){


    //QString size = QString::number(this->SaveFile->size()/(1024*1024),'f',2);
    //emit EmitFileSize(size);//会增加丢数

    Multithread* multithread =Multithread::GetInstance();
    static const int bufferSize = 20480;// 20k 定义缓冲区大小，根据实际情况调整

    qint64 DataSize=multithread->GetDataSocket()->bytesAvailable();
    if(bufferSize>DataSize){
        return;// 大于200个字节，才处理数据
    }

    static QByteArray DataBuffer;
    DataBuffer=multithread->GetDataSocket()->read(bufferSize);//抽取20k数据再线显示
    emit ShowSignal(DataBuffer);//online
    this->SaveFile->write(DataBuffer); // 写入文件
    this->SaveFile->write(multithread->GetDataSocket()->readAll()); //读取剩下的，并写入文件
    this->SaveFile->flush(); // 刷新文件缓冲区，确保数据被保存到磁盘 阻塞




}

void SaveThread::CalculateThresholdSlot(){//在线显示

    static const qint64 bufferDataSize = 20480; // 20k 定义缓冲区大小，根据实际情况调整

    Multithread* multithread =Multithread::GetInstance();
    qint64 DataSize=multithread->GetDataSocket()->bytesAvailable();

    if(bufferDataSize>DataSize){
        return;// 大于2000个字节，才处理数据
    }

   static QByteArray data;
   multithread->GetDataSocket()->read((DataSize-bufferDataSize));//清套接字缓存-先读超过20k的
   data=multithread->GetDataSocket()->read(bufferDataSize);

   emit ShowSignal(data);

}
