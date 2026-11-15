// Description:  analyse
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20250323 19:18
//           By: Fenhua Lu
//     Update #: 3.8


#include "analyse.h"
#include <mainwindow.h>
#include <QDebug>
#include <QTime>
#include <QFileDialog>
#include <QDir>
#include "widgetplot2d.h"
#include "savethread.h"

#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"
Analyse* Analyse::fAnalyse = nullptr;//类内静态变量初始化

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

Analyse::Analyse(QObject *parent): QObject(parent){
    this->fAnalyse=this;
    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();
    QObject::connect(fAnalyse , &Analyse::EmitWaveData, fWidgetPlot2D, &WidgetPlot2D::addDataPoint);

    for(int i=0;i<8;i++){
        Baseline[i]=0.;
        Threshold[i]=0.;
        RMSE[i]=0.;
        for(int j=0;j<2000;j++){
            wave[i][j]=0.;
        }
    }

    SaveThread* saveThread =SaveThread::GetInstance();
    connect(saveThread, SIGNAL(ShowSignal(QByteArray)), this, SLOT(OnlineSlot(QByteArray)));
}

Analyse* Analyse::GetInstance(){
    if (fAnalyse==nullptr) {
      fAnalyse = new Analyse();
    }
    return fAnalyse;
}

Analyse::~Analyse(){
    cout<<"~Analyse()";

}

void Analyse::CalculateThreshold(){

    int counts=150;
    for(int i=0;i<9;i++){
        for(int j=0;j<counts;j++){
            Baseline[i]=Baseline[i]+wave[i][j];
        }
        RMSE[i]=0.;
        Baseline[i]=Baseline[i]/150.;
    }

    for(int i=0;i<9;i++){
        for(int j=0;j<counts;j++){
            RMSE[i]=RMSE[i]+(wave[i][j]-Baseline[i])*(wave[i][j]-Baseline[i]);
        }
        RMSE[i]=sqrt(double(RMSE[i])/150.);
        //cout<<"RMSE[i]"<<RMSE[i];
        //Threshold[i]=Baseline[i]+7*RMSE[i];//Baseline+7倍Sigma作为阈值
    }

    MainWindow* mainWindow =MainWindow::GetInstance();
    mainWindow->SetThreshold(Baseline,RMSE);

}

void Analyse::OnlineSlot(const QByteArray &data) {

    MainWindow* mainWindow = MainWindow::GetInstance();

    // 1. 性能优化：预定义常量
    const quint64 HEADER = 0x55aaeb90;   // 合并后的包头
    const quint64 ENDER = 0xffffffffffff; // 合并后的包尾

    // 2. 状态变量初始化
    static int DataN = 0;        // 数据计数器（保持跨调用状态）
    static int ChannelMode = 0;   // 通道模式
    static int nEvent = 0;        // 事件编号

    // 3. 忙状态检查优化
    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }

    // 4. 二进制处理主循环
    const quint8* pData = reinterpret_cast<const quint8*>(data.constData());
    const int dataSize = data.size();

    for (int i = 0; i <= dataSize - 8; i += 8) { // 每次处理8字节

        // 5. 安全读取64位数据（解决对齐问题）
        quint64 rdata;
        memcpy(&rdata, pData + i, sizeof(quint64));
        rdata = qFromLittleEndian(rdata); // 假设小端序

        //if(i==0||i==1)qDebug() << "Raw bytes:" << QByteArray(reinterpret_cast<const char*>(&rdata), sizeof(rdata)).toHex();

        // 6. 包头检测优化
        if ((rdata >> 32) == HEADER) { // 高位32位匹配包头

            // 提取包头信息
            ChannelMode = (rdata >> 24) & 0xFF;  // 通道模式在25-32位
            nEvent = rdata & 0xFFFF;             // 事件号在低16位
            //cout<<" mode:"<<ChannelMode<<"  m:"<<nEvent<<endl;
            DataN = 0;                          // 重置计数器

            i += 8; // 跳过时间戳
            continue;
        }

        // 7. 数据解析优化（根据通道模式）
        else if ( ((rdata>>16) & 0xFFFFFFFFFFFF) != ENDER && (rdata >> 32) != HEADER && DataN < 2000) {//数据 DataN<2000
            switch (ChannelMode) {
                    case 1: // 1通道模式（4个样本/64位）
                        wave[0][DataN++] = (rdata >> 48) & 0xFFFF;
                        wave[0][DataN++] = (rdata >> 32) & 0xFFFF;
                        wave[0][DataN++] = (rdata >> 16) & 0xFFFF;
                        wave[0][DataN++] = rdata & 0xFFFF;
                        break;
                    case 2: // 2通道模式（2个样本/64位）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[0][DataN+1] = (rdata >> 16) & 0xFFFF;
                        wave[1][DataN+1] = rdata & 0xFFFF;
                        DataN += 2;
                        break;
                    case 4: // 4通道模式（1个样本/64位）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] =(rdata >> 32) & 0xFFFF;
                        wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[3][DataN] = rdata & 0xFFFF;
                        DataN++;
                        break;
                    case 8: // 8通道模式（分两次处理）
                        // 第一部分（前4通道）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[3][DataN] = rdata & 0xFFFF;
                        ChannelMode = 88; // 标记状态
                        break;
                    case 88: // 8通道第二部分
                        wave[4][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[5][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[6][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[7][DataN] = rdata & 0xFFFF;
                        DataN++;
                        ChannelMode = 8; // 恢复模式
                        break;
                    case 9: // 9通道模式（14位数据示例）
                        wave[8][DataN] = (rdata>>48) & 0xFFFF; //16 bit
                        wave[7][DataN] = (rdata>>34) & 0x3FFF; //14 bit
                        wave[6][DataN] = (rdata>>20) & 0x3FFF; //14 bit
                        wave[5][DataN] = (rdata>>6) &  0x3FFF; //14 bit  位掩码0011 1111 1111 1111
                        wave[4][DataN] =rdata &  0x3F;        //6  bit  位掩码0011 1111
                        ChannelMode = 99; // 标记状态
                        break;
                    case 99: // 9通道模式（14位数据示例）
                        wave[4][DataN] = (wave[4][DataN]<<8) | ((rdata>>56) & 0xFF);
                        wave[3][DataN] = (rdata>>42) & 0x3FFF; //14 bit
                        wave[2][DataN] = (rdata>>28) & 0x3FFF; //14 bit
                        wave[1][DataN] = (rdata>>14) & 0x3FFF; //14 bit
                        wave[0][DataN] = rdata &  0x3FFF;      //14 bit
                        DataN++;
                        ChannelMode = 9; // 标记状态
                        break;
            default:
                   break;
            }//switch end
        }//数据解码


        if (((rdata>>16) & 0xFFFFFFFFFFFF) == ENDER) { // 包尾 低48位匹配包尾
            const int TimeWindow = rdata & 0xFFFF; // 时间窗在高16位
            if (TimeWindow != DataN) {
                //qWarning() << "Data length mismatch:" << TimeWindow << DataN;
            }
            else{
                if(mainWindow->WhetherSave()){//是否计算阈值
                    Analyse::CalculateThreshold();
                }
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                emit EmitWaveData("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                break;
                             case 9://9通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                 emit EmitWaveData("ch8", wave[8][i]);
                            break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else end

        }//包尾
    }//while end
    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//OnlineSlot end

void Analyse::ShowSlot(QByteArray data){

    MainWindow* mainWindow =MainWindow::GetInstance();

    //处理接受的数据
    /*int count=ba.count();
    if(count>17000){
        ba.resize(17000);//调整QByteArray的大小
        cout<<"ba.Size:"<<count;
    }*/

    // 1. 性能优化：预定义常量
    const quint64 HEADER = 0x55aaeb90;   // 合并后的包头
    const quint64 ENDER = 0xffffffffffff; // 合并后的包尾

    // 2. 状态变量初始化
    static int DataN = 0;        // 数据计数器（保持跨调用状态）
    static int ChannelMode = 0;   // 通道模式
    static int nEvent = 0;        // 事件编号

    // 3. 忙状态检查优化
    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }

    // 4. 二进制处理主循环
    const quint8* pData = reinterpret_cast<const quint8*>(data.constData());
    const int dataSize = data.size();

    for (int i = 0; i <= dataSize - 8; i += 8) { // 每次处理8字节

        // 5. 安全读取64位数据（解决对齐问题）
        quint64 rdata;
        memcpy(&rdata, pData + i, sizeof(quint64));
        rdata = qFromLittleEndian(rdata); // 假设小端序

        //if(i==0||i==1)qDebug() << "Raw bytes:" << QByteArray(reinterpret_cast<const char*>(&rdata), sizeof(rdata)).toHex();

        // 6. 包头检测优化
        if ((rdata >> 32) == HEADER) { // 高位32位匹配包头

            // 提取包头信息
            ChannelMode = (rdata >> 24) & 0xFF;  // 通道模式在25-32位
            nEvent = rdata & 0xFFFF;             // 事件号在低16位
            //cout<<" mode:"<<ChannelMode<<"  m:"<<nEvent<<endl;
            DataN = 0;                          // 重置计数器

            i += 8; // 跳过时间戳
            continue;
        }

        // 7. 数据解析优化（根据通道模式）
        else if ( ((rdata>>16) & 0xFFFFFFFFFFFF) != ENDER && (rdata >> 32) != HEADER && DataN < 2000) {//数据 DataN<2000
            switch (ChannelMode) {
                case 1: // 1通道模式（4个样本/64位）
                    wave[0][DataN++] = (rdata >> 48) & 0xFFFF;
                    wave[0][DataN++] = (rdata >> 32) & 0xFFFF;
                    wave[0][DataN++] = (rdata >> 16) & 0xFFFF;
                    wave[0][DataN++] = rdata & 0xFFFF;
                    break;
                case 2: // 2通道模式（2个样本/64位）
                    wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                    wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                    wave[0][DataN+1] = (rdata >> 16) & 0xFFFF;
                    wave[1][DataN+1] = rdata & 0xFFFF;
                    DataN += 2;
                    break;
                case 4: // 4通道模式（1个样本/64位）
                    wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                    wave[1][DataN] =(rdata >> 32) & 0xFFFF;
                    wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                    wave[3][DataN] = rdata & 0xFFFF;
                    DataN++;
                    break;
                case 8: // 8通道模式（分两次处理）
                    // 第一部分（前4通道）
                    wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                    wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                    wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                    wave[3][DataN] = rdata & 0xFFFF;
                    ChannelMode = 88; // 标记状态
                    break;
                case 88: // 8通道第二部分
                    wave[4][DataN] = (rdata >> 48) & 0xFFFF;
                    wave[5][DataN] = (rdata >> 32) & 0xFFFF;
                    wave[6][DataN] = (rdata >> 16) & 0xFFFF;
                    wave[7][DataN] = rdata & 0xFFFF;
                    DataN++;
                    ChannelMode = 8; // 恢复模式
                    break;
                case 9: // 9通道模式（14位数据示例）
                    wave[8][DataN] = (rdata>>48) & 0xFFFF; //16 bit
                    wave[7][DataN] = (rdata>>34) & 0x3FFF; //14 bit
                    wave[6][DataN] = (rdata>>20) & 0x3FFF; //14 bit
                    wave[5][DataN] = (rdata>>6) &  0x3FFF; //14 bit  位掩码0011 1111 1111 1111
                    wave[4][DataN] =rdata &  0x3F;        //6  bit  位掩码0011 1111
                    ChannelMode = 99; // 标记状态
                    break;
                case 99: // 9通道模式（14位数据示例）
                    wave[4][DataN] = (wave[4][DataN]<<8) | ((rdata>>56) & 0xFF);
                    wave[3][DataN] = (rdata>>42) & 0x3FFF; //14 bit
                    wave[2][DataN] = (rdata>>28) & 0x3FFF; //14 bit
                    wave[1][DataN] = (rdata>>14) & 0x3FFF; //14 bit
                    wave[0][DataN] = rdata &  0x3FFF;      //14 bit
                    DataN++;
                    ChannelMode = 9; // 标记状态
                    break;
            default:
                   break;
            }//switch end
        }//数据解码


        if (((rdata>>16) & 0xFFFFFFFFFFFF) == ENDER) { // 包尾 低48位匹配包尾
            const int TimeWindow = rdata & 0xFFFF; // 时间窗在高16位
            if (TimeWindow != DataN) {
                //qWarning() << "Data length mismatch:" << TimeWindow << DataN;
            }
            else{
                if(mainWindow->WhetherSave()){//是否计算阈值
                    Analyse::CalculateThreshold();
                }
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                emit EmitWaveData("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                break;
                             case 9://9通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                 emit EmitWaveData("ch8", wave[8][i]);
                            break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else end

        }//包尾
    }//while end
    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//ShowSlot end

void Analyse::readAllDataSlot(QString datfilename){

    //QFile 负责文件的 IO 设备接口，即与文件的物理交互；
    //QDataStream 以数据流的方式读取文件内容或写入文件内容
    //打开并解析文件

    QFile file(datfilename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file";

    }

    QByteArray data = file.readAll();
    file.close();


    // 1. 性能优化：预定义常量
    const quint64 HEADER = 0x55aaeb90;   // 合并后的包头
    const quint64 ENDER = 0xffffffffffff; // 合并后的包尾

    // 2. 状态变量初始化
    static int DataN = 0;        // 数据计数器（保持跨调用状态）
    static int ChannelMode = 0;   // 通道模式
    static int nEvent = 0;        // 事件编号



    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new IPWindow();
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }


    // 4. 二进制处理主循环
    const quint8* pData = reinterpret_cast<const quint8*>(data.constData());
    const int dataSize = data.size();

    for (int i = 0; i <= dataSize - 8; i += 8) { // 每次处理8字节

        // 5. 安全读取64位数据（解决对齐问题）
        quint64 rdata;
        memcpy(&rdata, pData + i, sizeof(quint64));
        rdata = qFromLittleEndian(rdata); // 假设小端序

        //if(i==0||i==1)qDebug() << "Raw bytes:" << QByteArray(reinterpret_cast<const char*>(&rdata), sizeof(rdata)).toHex();

        // 6. 包头检测优化
        if ((rdata >> 32) == HEADER) { // 高位32位匹配包头

            // 提取包头信息
            ChannelMode = (rdata >> 24) & 0xFF;  // 通道模式在25-32位
            nEvent = rdata & 0xFFFF;             // 事件号在低16位
            //cout<<" mode:"<<ChannelMode<<"  m:"<<nEvent<<endl;
            DataN = 0;                          // 重置计数器

            i += 8; // 跳过时间戳
            continue;
        }

        // 7. 数据解析优化（根据通道模式）
        else if ( ((rdata>>16) & 0xFFFFFFFFFFFF) != ENDER && (rdata >> 32) != HEADER && DataN < 2000) {//数据 DataN<2000
            switch (ChannelMode) {
                    case 1: // 1通道模式（4个样本/64位）
                        wave[0][DataN++] = (rdata >> 48) & 0xFFFF;
                        wave[0][DataN++] = (rdata >> 32) & 0xFFFF;
                        wave[0][DataN++] = (rdata >> 16) & 0xFFFF;
                        wave[0][DataN++] = rdata & 0xFFFF;
                        break;
                    case 2: // 2通道模式（2个样本/64位）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[0][DataN+1] = (rdata >> 16) & 0xFFFF;
                        wave[1][DataN+1] = rdata & 0xFFFF;
                        DataN += 2;
                        break;
                    case 4: // 4通道模式（1个样本/64位）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] =(rdata >> 32) & 0xFFFF;
                        wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[3][DataN] = rdata & 0xFFFF;
                        DataN++;
                        break;
                    case 8: // 8通道模式（分两次处理）
                        // 第一部分（前4通道）
                        wave[0][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[1][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[2][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[3][DataN] = rdata & 0xFFFF;
                        ChannelMode = 88; // 标记状态
                        break;
                    case 88: // 8通道第二部分
                        wave[4][DataN] = (rdata >> 48) & 0xFFFF;
                        wave[5][DataN] = (rdata >> 32) & 0xFFFF;
                        wave[6][DataN] = (rdata >> 16) & 0xFFFF;
                        wave[7][DataN] = rdata & 0xFFFF;
                        DataN++;
                        ChannelMode = 8; // 恢复模式
                        break;
                    case 9: // 9通道模式（14位数据示例）
                        wave[8][DataN] = (rdata>>48) & 0xFFFF; //16 bit
                        wave[7][DataN] = (rdata>>34) & 0x3FFF; //14 bit
                        wave[6][DataN] = (rdata>>20) & 0x3FFF; //14 bit
                        wave[5][DataN] = (rdata>>6) &  0x3FFF; //14 bit  位掩码0011 1111 1111 1111
                        wave[4][DataN] =rdata &  0x3F;        //6  bit  位掩码0011 1111
                        ChannelMode = 99; // 标记状态
                        break;
                    case 99: // 9通道模式（14位数据示例）
                        wave[4][DataN] = (wave[4][DataN]<<8) | ((rdata>>56) & 0xFF);
                        wave[3][DataN] = (rdata>>42) & 0x3FFF; //14 bit
                        wave[2][DataN] = (rdata>>28) & 0x3FFF; //14 bit
                        wave[1][DataN] = (rdata>>14) & 0x3FFF; //14 bit
                        wave[0][DataN] = rdata &  0x3FFF;      //14 bit
                        DataN++;
                        ChannelMode = 9; // 标记状态
                        break;
            default:
                   break;
            }//switch end
        }//数据解码


        if (((rdata>>16) & 0xFFFFFFFFFFFF) == ENDER) { // 包尾 低48位匹配包尾
            const int TimeWindow = rdata & 0xFFFF; // 时间窗在高16位
            if (TimeWindow != DataN) {
                //qWarning() << "Data length mismatch:" << TimeWindow << DataN;
            }
            else{
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                emit EmitWaveData("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                break;
                             case 9://9通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                 emit EmitWaveData("ch8", wave[8][i]);
                            break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else end

        }//包尾
    }//while end
    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//readAllDataSlot end

void Analyse::readAllDataSlot_backup1(QString datfilename){//低效率解码方式，不推荐使用

    //QFile 负责文件的 IO 设备接口，即与文件的物理交互；
    //QDataStream 以数据流的方式读取文件内容或写入文件内容
    //打开并解析文件

    QFile file(datfilename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file";

    }

    QByteArray data = file.readAll();
    file.close();

    QString header="55aaeb90";   //包头
    QString ender="ffffffffffff";//包尾
    int DataN= 0;             //读取数据计数
    int TimeWindow;           //时间窗、
    int nEvent = 0;           //事件-数据板提供
    //int Event = 1;            //root计数事件
    int  ChannelMode=0;      //通道模式：1、2、4、8

    QString S_16bit0;         //16位数据单元转换变量
    QString S_16bit1;
    QString S_16bit2;
    QString S_16bit3;

    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new IPWindow();
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }


    bool ok;
   quint8 byte;
   for (int i = 0; i < (data.size()-8);i=i+8) {

        byte = static_cast<quint8>(data.at(i+1));
        S_16bit3 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i));
        S_16bit3=S_16bit3+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+3));
        S_16bit2 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+2));
        S_16bit2=S_16bit2+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+5));
        S_16bit1 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+4));
        S_16bit1=S_16bit1+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+7));
        S_16bit0 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+6));
        S_16bit0=S_16bit0+QString("%1").arg(byte, 2, 16, QChar('0'));


        if((S_16bit0+S_16bit1)==header){//包头
           ChannelMode=S_16bit2.remove(2,2).toInt(&ok,16);//(QChar)S_16bit2.at(3);
           S_16bit2=S_16bit2.remove(0,2);
           DataN=0;//填充数据计数
           nEvent=(S_16bit3+S_16bit2).toInt(&ok,16);
           i=i+8;//B_64bit=ba.left(8);ba.remove(0,8);//时间戳

        }//包头

        else if((S_16bit0+S_16bit1)!=header && (S_16bit0+S_16bit1+S_16bit2)!=ender && DataN<2000){//数据 DataN<2000
            switch (ChannelMode){
                    case 1://1通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+2]=S_16bit2.toInt(&ok,16);
                        wave[0][DataN+3]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+4;
                        break;
                    case 2://2通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit2.toInt(&ok,16);
                        wave[1][DataN+1]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+2;
                        break;
                    case 4://4通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        break;
                    case 8://8通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        ChannelMode++;
                       break;
                    case 9://8通道模式
                        wave[4][DataN]=S_16bit0.toInt(&ok,16);
                        wave[5][DataN]=S_16bit1.toInt(&ok,16);
                        wave[6][DataN]=S_16bit2.toInt(&ok,16);
                        wave[7][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        ChannelMode--;
                       break;
            default:
                   break;
            }
        }//数据 Data

        else if(DataN>=2000){//数据错乱、数据重置：注意可能导致通道错乱！

        }

        if((S_16bit0+S_16bit1+S_16bit2)==ender){//包尾
            TimeWindow=S_16bit3.toInt(&ok,16);;//获取时间窗
            if(ChannelMode==9){
                ChannelMode=8;
            }
            if(TimeWindow!=DataN){
//               cout<<"Error->TimeWindow DataN nEvent:"<<TimeWindow<<" "<<DataN<<" "
//               <<nEvent<<" 注意：可能丢包头、包尾或数据 ！！！";
            }
            else{
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 fWidgetPlot2D->addDataPoint("ch2", wave[2][i]);
                                 fWidgetPlot2D->addDataPoint("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 fWidgetPlot2D->addDataPoint("ch2", wave[2][i]);
                                 fWidgetPlot2D->addDataPoint("ch3", wave[3][i]);
                                 fWidgetPlot2D->addDataPoint("ch4", wave[4][i]);
                                 fWidgetPlot2D->addDataPoint("ch5", wave[5][i]);
                                 fWidgetPlot2D->addDataPoint("ch6", wave[6][i]);
                                 fWidgetPlot2D->addDataPoint("ch7", wave[7][i]);
                                break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else TimeWindow==DataN
        }//包尾


    }//for end here


    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//readAllDataSlot end


void Analyse::readAllDataSlot_backup0(QString datfilename){//低效率解码方式，不推荐使用

    //QFile 负责文件的 IO 设备接口，即与文件的物理交互；
    //QDataStream 以数据流的方式读取文件内容或写入文件内容
    //打开并解析文件

    QFile readFile(datfilename);
    readFile.open(QIODevice::ReadOnly);
    QByteArray ba,B_64bit;

    QDataStream in(&ba, QIODevice::ReadWrite);

    bool ok;
    in << readFile.readAll();//读取所有数据
    ba.remove(0,4);//删除4个字节  4个字节意义？

    QString header="55aaeb90";   //包头
    QString ender="ffffffffffff";//包尾
    int DataN= 0;             //读取数据计数
    int TimeWindow;           //时间窗、
    int nEvent = 0;           //事件-数据板提供
    //int Event = 1;            //root计数事件
    int  ChannelMode=0;      //通道模式：1、2、4、8

    QString S_16bit0;         //16位数据单元转换变量
    QString S_16bit1;
    QString S_16bit2;
    QString S_16bit3;

    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new IPWindow();
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }

    while(true){


        if(ba.count()<4)break;
        //从左读取1个字节，并移除  大端
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit3=B_64bit.toHex();//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit3= B_64bit.toHex()+S_16bit3;//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit2=B_64bit.toHex();//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit2=B_64bit.toHex()+S_16bit2;//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit1=B_64bit.toHex();//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit1=B_64bit.toHex()+S_16bit1;//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit0=B_64bit.toHex();//转成字符串
        B_64bit=ba.left(1);ba.remove(0,1);
        S_16bit0=B_64bit.toHex()+S_16bit0;;//转成字符串


        if((S_16bit0+S_16bit1)==header){//包头
           ChannelMode=S_16bit2.remove(2,2).toInt(&ok,16);//(QChar)S_16bit2.at(3);
           S_16bit2=S_16bit2.remove(0,2);
           DataN=0;//填充数据计数
           nEvent=(S_16bit3+S_16bit2).toInt(&ok,16);
           B_64bit=ba.left(8);ba.remove(0,8);//时间戳
        }//包头

        else if((S_16bit0+S_16bit1)!=header && (S_16bit0+S_16bit1+S_16bit2)!=ender && DataN<2000){//数据 DataN<2000
            switch (ChannelMode){
                    case 1://1通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+2]=S_16bit2.toInt(&ok,16);
                        wave[0][DataN+3]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+4;
                        break;
                    case 2://2通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit2.toInt(&ok,16);
                        wave[1][DataN+1]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+2;
                        break;
                    case 4://4通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        break;
                    case 8://8通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        ChannelMode++;
                       break;
                    case 9://8通道模式
                        wave[4][DataN]=S_16bit0.toInt(&ok,16);
                        wave[5][DataN]=S_16bit1.toInt(&ok,16);
                        wave[6][DataN]=S_16bit2.toInt(&ok,16);
                        wave[7][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        ChannelMode--;
                       break;
            default:
                   break;
            }
        }//数据 Data

        else if(DataN>=2000){//数据错乱、数据重置：注意可能导致通道错乱！

        }

        if((S_16bit0+S_16bit1+S_16bit2)==ender){//包尾
            TimeWindow=S_16bit3.toInt(&ok,16);;//获取时间窗
            if(ChannelMode==9){
                ChannelMode=8;
            }
            if(TimeWindow!=DataN){
               cout<<"Error->TimeWindow DataN nEvent:"<<TimeWindow<<" "<<DataN<<" "
               <<nEvent<<" 注意：可能丢包头、包尾或数据 ！！！";
            }
            else{
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 fWidgetPlot2D->addDataPoint("ch2", wave[2][i]);
                                 fWidgetPlot2D->addDataPoint("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 fWidgetPlot2D->addDataPoint("ch0", wave[0][i]);
                                 fWidgetPlot2D->addDataPoint("ch1", wave[1][i]);
                                 fWidgetPlot2D->addDataPoint("ch2", wave[2][i]);
                                 fWidgetPlot2D->addDataPoint("ch3", wave[3][i]);
                                 fWidgetPlot2D->addDataPoint("ch4", wave[4][i]);
                                 fWidgetPlot2D->addDataPoint("ch5", wave[5][i]);
                                 fWidgetPlot2D->addDataPoint("ch6", wave[6][i]);
                                 fWidgetPlot2D->addDataPoint("ch7", wave[7][i]);
                                break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else TimeWindow==DataN
        }//包尾

    }//while end
    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//readAllDataSlot end

void Analyse::ShowSlot_backup(QByteArray data){

    MainWindow* mainWindow =MainWindow::GetInstance();

    //处理接受的数据
    /*int count=ba.count();
    if(count>17000){
        ba.resize(17000);//调整QByteArray的大小
        cout<<"ba.Size:"<<count;
    }*/

    QString header="55aaeb90";   //包头
    QString ender="ffffffffffff";//包尾

    int DataN= 0;             //读取数据计数
    int TimeWindow;           //时间窗、
    int nEvent = 0;           //事件-数据板提供
    //int Event = 1;            //root计数事件
    int  ChannelMode=0;      //通道模式：1、2、4、8

    QString S_16bit0;         //16位数据单元转换变量
    QString S_16bit1;
    QString S_16bit2;
    QString S_16bit3;

    WidgetPlot2D *fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new IPWindow();
    if(fWidgetPlot2D->GetBusy()){
        return;//如果示波器忙碌，则不处理
    }
    else{
       fWidgetPlot2D->SetBusy(true);
    }

   bool ok;
   quint8 byte;
   for (int i = 0; i < (data.size()-8);i=i+8) {

        byte = static_cast<quint8>(data.at(i+1));
        S_16bit3 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i));
        S_16bit3=S_16bit3+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+3));
        S_16bit2 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+2));
        S_16bit2=S_16bit2+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+5));
        S_16bit1 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+4));
        S_16bit1=S_16bit1+QString("%1").arg(byte, 2, 16, QChar('0'));

        byte = static_cast<quint8>(data.at(i+7));
        S_16bit0 = QString("%1").arg(byte, 2, 16, QChar('0'));
        byte = static_cast<quint8>(data.at(i+6));
        S_16bit0=S_16bit0+QString("%1").arg(byte, 2, 16, QChar('0'));


        if((S_16bit0+S_16bit1)==header){//包头
           ChannelMode=S_16bit2.remove(2,2).toInt(&ok,16);//(QChar)S_16bit2.at(3);
           S_16bit2=S_16bit2.remove(0,2);
           DataN=0;//填充数据计数
           nEvent=(S_16bit3+S_16bit2).toInt(&ok,16);
           i=i+8;//B_64bit=ba.left(8);ba.remove(0,8);//时间戳
        }//包头

        else if((S_16bit0+S_16bit1)!=header && (S_16bit0+S_16bit1+S_16bit2)!=ender && DataN<2000){//数据 DataN<2000
            switch (ChannelMode){
                    case 1://1通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+2]=S_16bit2.toInt(&ok,16);
                        wave[0][DataN+3]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+4;
                        break;
                    case 2://2通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[0][DataN+1]=S_16bit2.toInt(&ok,16);
                        wave[1][DataN+1]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+2;
                        break;
                    case 4://4通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        break;
                    case 8://8通道模式
                        wave[0][DataN]=S_16bit0.toInt(&ok,16);
                        wave[1][DataN]=S_16bit1.toInt(&ok,16);
                        wave[2][DataN]=S_16bit2.toInt(&ok,16);
                        wave[3][DataN]=S_16bit3.toInt(&ok,16);
                        ChannelMode++;
                       break;
                    case 9://8通道模式
                        wave[4][DataN]=S_16bit0.toInt(&ok,16);
                        wave[5][DataN]=S_16bit1.toInt(&ok,16);
                        wave[6][DataN]=S_16bit2.toInt(&ok,16);
                        wave[7][DataN]=S_16bit3.toInt(&ok,16);
                        DataN=DataN+1;
                        ChannelMode--;
                       break;
            default:
                   break;
            }
        }//数据 Data


        if((S_16bit0+S_16bit1+S_16bit2)==ender){//包尾
            TimeWindow=S_16bit3.toInt(&ok,16);;//获取时间窗
            if(ChannelMode==9){
                ChannelMode=8;
            }
            if(TimeWindow!=DataN){
               ;//cout<<"Error->TimeWindow DataN nEvent:"<<TimeWindow<<" "<<DataN<<" "
               //<<nEvent<<" 注意：可能丢包头、包尾或数据 ！！！";
            }
            else{
                if(mainWindow->WhetherSave()){//是否计算阈值
                    Analyse::CalculateThreshold();
                }
                for (int i=0;i<DataN;i++){
                     switch (ChannelMode){
                             case 1://1通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                 break;
                             case 2://2通道模式
                                emit EmitWaveData("ch0", wave[0][i]);
                                emit EmitWaveData("ch1", wave[1][i]);
                                 break;
                             case 4://4通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 break;
                             case 8://8通道模式
                                 emit EmitWaveData("ch0", wave[0][i]);
                                 emit EmitWaveData("ch1", wave[1][i]);
                                 emit EmitWaveData("ch2", wave[2][i]);
                                 emit EmitWaveData("ch3", wave[3][i]);
                                 emit EmitWaveData("ch4", wave[4][i]);
                                 emit EmitWaveData("ch5", wave[5][i]);
                                 emit EmitWaveData("ch6", wave[6][i]);
                                 emit EmitWaveData("ch7", wave[7][i]);
                                break;
                     default:
                            break;
                     }//switch end
                }//for end
            }//else TimeWindow==DataN
        }//包尾

    }//while end
    fWidgetPlot2D->SetBusy(false);//处理完毕，示波器空闲
}//ShowSlot end

