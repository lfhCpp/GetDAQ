#ifndef SAVETHREAD_H
#define SAVETHREAD_H

#include <QObject>
#include <QThread>
#include <QFileDialog>
#include <QTimer>
#include <QMutex>

class SaveThread : public QThread
{
    Q_OBJECT
public:
    explicit SaveThread();

     ~SaveThread();

    static SaveThread* GetInstance();//静态成员函数只能访问静态成员变量

    void SaveThreadStart();//线程处理开始

    void SetSaveFile(QFile *File){this->SaveFile=File;}//设置文件指针

    void run();//线程处理函数

signals:

   void ShowSignal(const QByteArray &data);//给解码程序发信号

   void EmitFileSize(QString size);//发送文件大小


public slots:

    void SaveSlot();//读取数据并保存

    void SaveAndShowSlot();//读取数据并保存、展示

    void CalculateThresholdSlot();//读取数据并发送给分析程序在线显示

private:

    static SaveThread *fSaveThread;

    QFile *SaveFile;

    QMutex* mutex; // 在全局范围内定义互斥锁


};

#endif // SAVETHREAD_H
