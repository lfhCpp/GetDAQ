#ifndef ANALYSE_H
#define ANALYSE_H

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QDebug>
#include <QFileDialog>
#include <mainwindow.h>

class MainWindow;

class Analyse : public QObject
{
    Q_OBJECT
public:
    explicit Analyse(QObject *parent = nullptr);
    ~Analyse();
    static Analyse* GetInstance();//静态成员函数只能访问静态成员变量

    void CalculateThreshold();//计算阈值

signals:

    void EmitWaveData(QString Ch,int data);//发生单个点数据
    void EmitWaveDataBatch(const QVarLengthArray<QVector<int>>& channels);//批量发送数据

public slots:
    void readAllDataSlot(QString datfilename);//解析文件
    void readAllDataSlot_backup1(QString datfilename);//解析文件
    void readAllDataSlot_backup0(QString datfilename);//解析文件 低效率解码方式，不推荐使用
    void OnlineSlot(const QByteArray &data);//示波器Online   
    void ShowSlot(QByteArray data);//示波器Online,不同线程调用
    void ShowSlot_backup(QByteArray data);//示波器Online,不同线程调用 原低效率解码方式，不推荐使用

private:
    static Analyse *fAnalyse;
    int wave[9][2000];    //8通道数据
    double Threshold[9];
    double Baseline[9];
    double RMSE[9];

};

#endif // ANALYSE_H
