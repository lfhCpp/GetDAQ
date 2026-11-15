// Description:  MainWindow
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20240424 09:37
//           By: Fenhua Lu
//     Update #: 3.7


#include "mainwindow.h"
#include "ui_mainwindow.h"

#include<QTcpSocket>
#include<QLabel>
#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QTimer>


#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"

MainWindow* MainWindow::fMainWindow = nullptr;//类内静态变量初始化
QTcpSocket* MainWindow::fcmdSocket= nullptr;
Multithread* MainWindow::fMultithread = nullptr;
Analyse* MainWindow::fAnalyse = nullptr;
QThread* MainWindow::fThread_Ana = nullptr;
SaveThread* MainWindow::fSaveThread = nullptr;
WidgetPlot2D* MainWindow::fWidgetPlot2D = nullptr;
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

MainWindow::MainWindow(QWidget *parent) :QMainWindow(parent),ui(new Ui::MainWindow){
    TestNumber=6;
    ui->setupUi(this);
    this->setWindowIcon(QIcon(":/image/MUSIC.png"));
    MainWindow::InitializePar();
}

MainWindow::MainWindow(const MainWindow & ReMainWindow):QMainWindow(nullptr){//拷贝构造函数
    this->ui=new Ui::MainWindow(*ReMainWindow.ui);
}

void MainWindow::InitializePar(){

    this->fThread_Ana=new QThread;//分析数据多线程
    this->fAnalyse =Analyse::GetInstance(); //启动
    fAnalyse->moveToThread(fThread_Ana);
    fThread_Ana->start();
    connect(this,SIGNAL(EmitReadSignal(QString)),fAnalyse,SLOT(readAllDataSlot(QString)));//等待分析文件信号触发

    this->fTriggerTimer = new QTimer();
    this->fTriggerTimer->start(500); // 定时器间隔为0.5秒
    connect(this->fTriggerTimer,&QTimer::timeout,this,&MainWindow::TriggerTimerSlot);//发送定时触发


    for(int i=0;i<8;i++){//阈值初始化
        Baseline[i]=0.0;
        Sigma[i]=0.0;
        Threshold[i]=0.0;
    }
    this->CalculationTimes=0.;
    OnlyShow=false;//保存文件判断

    this->fcmdSocket=new QTcpSocket();//创建socket对象
    this->fcmdSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);//禁用Nagle算法
    MainWindow::fMainWindow=this;//静态对象初始化：否则GetInstance()会创建其他对象
    this->MainWindow::setLED(this->ui->CmdNetLabel,1,10);//网口指示灯
    this->MainWindow::setLED(this->ui->DataNetLabel,1,10);//网口指示灯
    this->MainWindow::setLED(this->ui->BufferLabel,2,10);//Buffer指示灯
    this->ui->TransmissionSpeed->setStyleSheet("background-color:transparent");//设置背景透明
    this->ui->TransmissionSpeed->setText("  0 Mbps");
    QDir dir;
    srcDirPath= dir.QDir::currentPath();//默认当前工作路径
    srcDirPath=srcDirPath+"/SaveData/";
    dir.QDir::mkpath(srcDirPath);
    Filename="NULL";

    this->fIPWindow = IPWindow::GetInstance();//实例化：new IPWindow();
    //QObject::connect(&myWidget,&MyWidget::MySignal,信号接收者，槽函数)
    //不能在其他类里面操作界面   控制状态灯
    connect(fIPWindow,&IPWindow::ControlCmdNetLED,this,&MainWindow::OpenCmdNetLED);
    connect(fIPWindow,&IPWindow::ControlDataNetLED,this,&MainWindow::OpenDataNetLED);
    connect(fIPWindow,&IPWindow::DisableCmdNetLED,this,&MainWindow::CloseCmdNetLED);
    connect(fIPWindow,&IPWindow::DisableDataNetLED,this,&MainWindow::CloseDataNetLED);
   //通过千兆网口获取数据板信息
    connect(fIPWindow,&IPWindow::SendcmdSocketByte,this,&MainWindow::ShowCmdSlot);

    this->fSaveThread =SaveThread::GetInstance();//保存文件多线程:发送文件大小
    QObject::connect(this->fSaveThread, SIGNAL(EmitFileSize(QString)),this, SLOT(ShowFileSizeSlot(QString)));

}

MainWindow::~MainWindow(){

    cout<<"~~MainWindow";

    if(ui!=nullptr){
        delete ui;
        ui=nullptr;
    }

    if(fIPWindow!=nullptr){
        delete fIPWindow;
        fIPWindow=nullptr;
    }

    if(fcmdSocket!=nullptr){
        delete fcmdSocket;
        fcmdSocket=nullptr;
    }

    if(fSaveThread!=nullptr){
        delete fSaveThread;
        fSaveThread=nullptr;
    }

    if(fWidgetPlot2D!=nullptr){
        delete fWidgetPlot2D;
        fWidgetPlot2D=nullptr;
    }
    if(fMultithread!=nullptr){
        delete fMultithread;
        fMultithread=nullptr;
    }
    if(fAnalyse!=nullptr){
        delete fAnalyse;
        fAnalyse=nullptr;
    }
    if(fThread_Ana!=nullptr){
        delete fThread_Ana;
        fThread_Ana=nullptr;
    }
}

MainWindow* MainWindow::GetInstance(){
    if (fMainWindow==nullptr) {
      fMainWindow = new MainWindow();
    }
    return fMainWindow;
}


void MainWindow::on_ConnectNet_triggered(){

    //fIPWindow = IPWindow::GetInstance();//实例化：new IPWindow();
    fIPWindow->show();//构造函数里GetInstance()
}

void MainWindow::on_Cmd_triggered(){
    fCmdWindow = CmdWindow::GetInstance();//实例化：new IPWindow();
    fCmdWindow->show();
}


void MainWindow::on_ReceiveData_clicked(){

    //    //将连接信息显示在信息窗口
    //    if(!(fcmdSocket->waitForConnected(1000)&&fDataSocket->waitForConnected(1000))){
    //       QMessageBox::warning(this, "连接提示", "请注意网口连接情况！");
    //       return;
    //    }


    static bool DeleteTCP=false;//是否删除千兆网套接字

    //发送按键失能
    this->ui->ReceiveData->setEnabled(false);
    this->ui->SetThreshold->setEnabled(false);

    //启动多线程
    this->fMultithread =Multithread::GetInstance(); //
    this->fMultithread->start();                    //开始线程
    // emit SaveDataSignal();//发送保存文件信号
    //cout<<"启动多线程";



    fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new WidgetPlot2D();
    //fWidgetPlot2D->show();
    //fWidgetPlot2D->hide();

    //1秒钟后发送命令
     QTimer::singleShot(1000, [=](){

         QByteArray FPGA("0b0001");
         QByteArray cmd_data0 = QByteArray::fromHex(FPGA);
         this->fcmdSocket->write(cmd_data0);
         this->fcmdSocket->flush();//清零事件号

         QTimer::singleShot(100, [=](){
            qDebug() << "FPGA_readadc" ;
            QByteArray FPGA_readadc("010001");
            QByteArray cmd_data = QByteArray::fromHex(FPGA_readadc);

            this->fcmdSocket->write(cmd_data);
            this->fcmdSocket->flush();
            this->ui->textBrowser->append("启动多线程：ReceiveData");

             if(DeleteTCP){
                 //获取通信套接字的内容、并删除套接字
                this->fcmdSocket->disconnectFromHost();
                //this->fcmdSocket->waitForDisconnected(200);
                this->fcmdSocket->close();
                QByteArray byte=fcmdSocket->readAll();
                //断开之前的连接 注意：仅断开信号与槽的连接
                disconnect(this->fcmdSocket, &QTcpSocket::connected, 0, 0);
                disconnect(this->fcmdSocket, &QTcpSocket::disconnected, 0, 0);
                disconnect(this->fcmdSocket, &QTcpSocket::readyRead, 0, 0);
                if(this->fcmdSocket!=nullptr){
                    delete this->fcmdSocket;
                    this->fcmdSocket=nullptr;
                }
           }//if end
          });//QTimer::singleShot end here
     });//QTimer::singleShot end here

     connect(this,&MainWindow::TriggerSignal,[this](){
         QString size = QString::number(this->fMultithread->GetQFile()->size()/(1024*1024),'f',2);
         QString show="FileSize:";
         show=show+size;
         show=show+"MB";
         this->ui->FilenemeEdit->setText(show);
     });
}


void MainWindow::on_StopReceive_clicked(){

//    //将连接信息显示在信息窗口
//    if(!(fcmdSocket->waitForConnected(1000)&&fDataSocket->waitForConnected(1000))){
//       QMessageBox::warning(this, "连接提示", "请注意网口连接情况！");
//       return;
//    }

    //重新创建socket对象
    if(this->fcmdSocket==nullptr){
        this->fcmdSocket=new QTcpSocket();
    }
    //发送连接千兆以太网信号，给ipwindow类接收
    emit EmitConnectCmdSignal();
    this->fcmdSocket->waitForConnected(1000);//等待连接成功


    qDebug() << "FPGA_StopReadADC" ;
    QByteArray FPGA("010000");
    QByteArray cmd_data = QByteArray::fromHex(FPGA);
    this->fcmdSocket->write(cmd_data);
    this->fcmdSocket->flush();
    this->ui->textBrowser->append("StopReceiveData");

    //可能需要延时
    //emit fMainWindow->StopReceiveData();//发送停止接收文件信号

    //按键激活
    this->OnlyShow=false;//保存文件判断
    this->ui->ReceiveData->setEnabled(true);
    this->ui->SetThreshold->setEnabled(true);

    QObject::disconnect(this,&MainWindow::TriggerSignal, 0, 0);//触发信号停止发送广播

}

void MainWindow::on_ModeBox_activated(int){

    Multithread* multithread =Multithread::GetInstance();

    //将连接信息显示在信息窗口
    if(!(fcmdSocket->waitForConnected(1000)&&multithread->GetDataSocket()->waitForConnected(1000))){
       QMessageBox::warning(this, "连接提示", "请注意网口连接情况！");
       return;
    }

    if(ui->ModeBox->currentIndex()==1){
        QByteArray FPGA("020001");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "Mode:CH1" <<endl;
        this->ui->textBrowser->append("Mode:CH1");
    }
    else if(ui->ModeBox->currentIndex()==2){
        QByteArray FPGA("020002");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "Mode:CH1 CH2" <<endl;
        this->ui->textBrowser->append("Mode:CH1 CH2");
    }
    else if(ui->ModeBox->currentIndex()==3){
        QByteArray FPGA("020004");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "Mode:CH1 CH2 CH3 CH4" <<endl;
        this->ui->textBrowser->append("Mode:CH1 CH2 CH3 CH4");
    }
    else if(ui->ModeBox->currentIndex()==4){
        QByteArray FPGA("020008");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "FullMode" <<endl;
        this->ui->textBrowser->append("FullMode");
    }
    else if(ui->ModeBox->currentIndex()==5){
        QByteArray FPGA("0b0001");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "ReSetEventCounts" <<endl;
        this->ui->textBrowser->append("ReSetEventCounts");
    }
    else if(ui->ModeBox->currentIndex()==6){
        QByteArray FPGA("0e0001");
            QByteArray cmd_data = QByteArray::fromHex(FPGA);
            fcmdSocket->write(cmd_data);
            fcmdSocket->flush();
        qDebug() << "RestartDataBoard"<<endl;
        this->ui->textBrowser->append("RestartDataBoard");
        //1秒钟后发送命令
        QTimer::singleShot(2000, [=](){
            //发送连接千兆以太网信号，给ipwindow类接收
            emit EmitConnectCmdSignal();
            this->fcmdSocket->waitForConnected(1000);//等待连接成功
            //发送连接万兆以太网信号，给ipwindow类接收
            emit EmitConnectDataSignal();
            this->fMultithread->GetDataSocket()->waitForConnected(1000);//等待连接成功
        });//QTimer::singleShot end here
    }
}

void MainWindow::on_SetThreshold_clicked(){

    //发送按键失能
    this->ui->ReceiveData->setEnabled(false);
    this->ui->SetThreshold->setEnabled(false);

    this->OnlyShow=true;
    this->CalculationTimes=0.;//重新计算阈值
    for(int i=0;i<8;i++){
        Baseline[i]=0.0;
        Sigma[i]=0.0;
        Threshold[i]=0.0;
    }

    //启动多线程
    this->fMultithread =Multithread::GetInstance(); //
    this->fMultithread->start();                    //开始线程
    // emit SaveDataSignal();//发送保存文件信号
    //cout<<"启动多线程";

    fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new WidgetPlot2D();
    fWidgetPlot2D->show();

    //1秒钟后发送命令
     QTimer::singleShot(1000, [=](){

         qDebug() << "FPGA_readadc" ;
         QByteArray FPGA_readadc("010001");
         QByteArray cmd_data = QByteArray::fromHex(FPGA_readadc);

         this->fcmdSocket->write(cmd_data);
         this->fcmdSocket->flush();
         this->ui->textBrowser->append("阈值计算与在线监视：ReceiveData");

     });//QTimer::singleShot end here

}

void MainWindow::SetThreshold(double* b,double* s){
    this->CalculationTimes++;
    for(int i=0;i<8;i++){//加权计算
       Baseline[i]=Baseline[i]*(CalculationTimes-1)/CalculationTimes+b[i]/CalculationTimes;
       Sigma[i]=Sigma[i]*(CalculationTimes-1)/CalculationTimes+s[i]/CalculationTimes;
       Threshold[i]=Baseline[i]+7*Sigma[i];
    }
}

/**********************自定义函数*******************/
// 该函数将label控件变成一个圆形指示灯，需要指定颜色color以及直径size
// color 0:grey 1:red 2:green 3:yellow
// size  单位是像素
void MainWindow::setLED(QLabel* label,int color, int size){
    // 将label中的文字清空
    label->setText("");
    // 先设置矩形大小
    // 如果ui界面设置的label大小比最小宽度和高度小，矩形将被设置为最小宽度和最小高度；
    // 如果ui界面设置的label大小比最小宽度和高度大，矩形将被设置为最大宽度和最大高度；
    QString min_width = QString("min-width: %1px;").arg(size);              // 最小宽度：size
    QString min_height = QString("min-height: %1px;").arg(size);            // 最小高度：size
    QString max_width = QString("max-width: %1px;").arg(size);              // 最小宽度：size
    QString max_height = QString("max-height: %1px;").arg(size);            // 最小高度：size
    // 再设置边界形状及边框
    QString border_radius = QString("border-radius: %1px;").arg(size/2);    // 边框是圆角，半径为size/2
    QString border = QString("border:1px solid black;");                    // 边框为1px黑色
    // 最后设置背景颜色
    QString background = "background-color:";
    switch (color) {
    case 0:
        // 灰色
        background += "rgb(190,190,190)";
        break;
    case 1:
        // 红色
        background += "rgb(255,0,0)";
        break;
    case 2:
        // 绿色
        background += "rgb(0,255,0)";
        break;
    case 3:
        // 黄色
        background += "rgb(255,255,0)";
        break;
    default:
        break;
    }
    const QString SheetStyle = min_width + min_height + max_width + max_height + border_radius + border + background;
    label->setStyleSheet(SheetStyle);
}

void MainWindow::OpenCmdNetLED(){
    QLabel* label=this->ui->CmdNetLabel;
    int color=2;
    int size=10;
    MainWindow::setLED(label,color,size);
}

void MainWindow::OpenDataNetLED(){
    QLabel* label=this->ui->DataNetLabel;
    int color=2;
    int size=10;
    MainWindow::setLED(label,color,size);
}

void MainWindow::CloseCmdNetLED(){
    QLabel* label=this->ui->CmdNetLabel;
    int color=1;
    int size=10;
    MainWindow::setLED(label,color,size);
}

void MainWindow::CloseDataNetLED(){
    QLabel* label=this->ui->DataNetLabel;
    int color=1;
    int size=10;
    MainWindow::setLED(label,color,size);
}

void MainWindow::on_SavePath_triggered(){
    this->srcDirPath = QFileDialog::getExistingDirectory(
               this, "choose src Directory",
               "/");
   if (srcDirPath.isEmpty()){
       return;
   }
   else{
       qDebug() << "srcDirPath=" << srcDirPath;
       srcDirPath += "/";
   }
    this->ui->textBrowser->append("File is stored in: \n"+srcDirPath);

}

void MainWindow::on_SetFilename_clicked(){
    QString tem = this->ui->FilenemeEdit->text();
    Filename=tem;
    cout<<"Filename=tem"<<Filename;
    this->ui->textBrowser->append("Set Filename : "+Filename);
}

void MainWindow::ShowCmdSlot(QByteArray byte){

    QString s=byte.toHex().toUpper();
    QString s2=s.at(0);s2=s2+s.at(1);
    if(s2=="FF"){//万兆网口传输速率
        bool ok;
        s.replace(0,2,"0");
        this->ui->TransmissionSpeed->setText(QString::number(s.toInt(&ok,16))+"Mbps");
    }
    else if(s=="101010"){
      this->MainWindow::setLED(this->ui->BufferLabel,1,10);//数据板Buffer已满
    }
    else if(s=="010101"){
      this->MainWindow::setLED(this->ui->BufferLabel,2,10);
    }
}

void MainWindow::ShowFileSizeSlot(QString size){

    QString show="FileSize:";
    show=show+size;
    show=show+"MB";
    this->ui->FilenemeEdit->setText(show);

}


void MainWindow::on_commandLinkButton_clicked(){

    fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new WidgetPlot2D();
    fWidgetPlot2D->show();

    /* 使用WidgetPlot2D绘制实时波形只需两步：
     * 1.初始化波形名称：函数initGraphName(QStringList)
     * 2.给对应的波形添加数据：函数addData(QString, double)
     * 波形名称和数据要一一对应。
     * 函数addData为公有槽函数，也可使用信号槽传递波形数据。*/
//    timer1 = new QTimer();
//    connect(timer1, SIGNAL(timeout()), this, SLOT(timer1Handler()));
//    timer1->start(1);//1s

}

/* 定时器1溢出事件 */
void MainWindow::timer1Handler()
{
    // 模拟正弦波
    static double degrees = 0;
    double radians = qDegreesToRadians(degrees);
    double s = 15.0 * qSin(radians);
    fWidgetPlot2D->addDataPoint("正弦波", s);
    degrees += 1.0;
    if (degrees >= 360.0) degrees = 0;
}


void MainWindow::on_ana_file_triggered(){


    QString datfilename ;
    datfilename = QFileDialog::getOpenFileName(this, tr("open file"), " ",  tr("txt(*.bin);;Allfile(*.*)"));
    fWidgetPlot2D = WidgetPlot2D::GetInstance();//实例化：new WidgetPlot2D();
    fWidgetPlot2D->show();
    if(!(fWidgetPlot2D->GetBusy())){
        this->ui->textBrowser->append("启动多线程分析文件："+datfilename);
    }
    else{
        this->ui->textBrowser->append("ERROR:示波器忙碌，请稍后再试！！！");
    }

    emit EmitReadSignal(datfilename);//解析文件信号


}


void MainWindow::on_SetTh_clicked(){


    QTimer::singleShot(100, [=](){
        QString th= QString("%1%2").arg(3, 2, 16, QChar('0')).arg((int)Threshold[0],4,16, QChar('0'));
        QByteArray cmd = QByteArray::fromHex(th.toUtf8());
        this->fcmdSocket->write(cmd);
        this->fcmdSocket->flush();

        QTimer::singleShot(100, [=](){
            QString th= QString("%1%2").arg(1+ 3, 2, 16, QChar('0')).arg((int)Threshold[1],4,16, QChar('0'));
            QByteArray cmd = QByteArray::fromHex(th.toUtf8());
            this->fcmdSocket->write(cmd);
            this->fcmdSocket->flush();

            QTimer::singleShot(100, [=](){
                QString th= QString("%1%2").arg(2+ 3, 2, 16, QChar('0')).arg((int)Threshold[2],4,16, QChar('0'));
                QByteArray cmd= QByteArray::fromHex(th.toUtf8());
                this->fcmdSocket->write(cmd);
                this->fcmdSocket->flush();

                QTimer::singleShot(100, [=](){
                    QString th;
                    QByteArray cmd;
                    th = QString("%1%2").arg(3+ 3, 2, 16, QChar('0')).arg((int)Threshold[3],4,16, QChar('0'));
                    cmd = QByteArray::fromHex(th.toUtf8());
                    this->fcmdSocket->write(cmd);
                    this->fcmdSocket->flush();

                    QTimer::singleShot(100, [=](){
                        QString th= QString("%1%2").arg(4+ 3, 2, 16, QChar('0')).arg((int)Threshold[4],4,16, QChar('0'));
                        QByteArray cmd = QByteArray::fromHex(th.toUtf8());
                        this->fcmdSocket->write(cmd);
                        this->fcmdSocket->flush();

                        QTimer::singleShot(100, [=](){
                            QString th= QString("%1%2").arg(5+ 3, 2, 16, QChar('0')).arg((int)Threshold[5],4,16, QChar('0'));
                            QByteArray cmd = QByteArray::fromHex(th.toUtf8());
                            this->fcmdSocket->write(cmd);
                            this->fcmdSocket->flush();

                            QTimer::singleShot(100, [=](){
                                QString  th = QString("%1%2").arg(6+ 3, 2, 16, QChar('0')).arg((int)Threshold[6],4,16, QChar('0'));
                                QByteArray cmd = QByteArray::fromHex(th.toUtf8());
                                this->fcmdSocket->write(cmd);
                                this->fcmdSocket->flush();

                                QTimer::singleShot(100, [=](){
                                    QString th = QString("%1%2").arg(7+ 3, 2, 16, QChar('0')).arg((int)Threshold[7],4,16, QChar('0'));
                                    QByteArray cmd = QByteArray::fromHex(th.toUtf8());
                                    this->fcmdSocket->write(cmd);
                                    this->fcmdSocket->flush();

                                });//QTimer::singleShot end here
                            });//QTimer::singleShot end here
                        });//QTimer::singleShot end here
                    });//QTimer::singleShot end here
                });//QTimer::singleShot end here
            });//QTimer::singleShot end here
        });//QTimer::singleShot end here
    });//QTimer::singleShot end here


    this->ui->textBrowser->append("已设定Baseline+7*Sigma作为阈值:");
    this->ui->textBrowser->append("ch0:"+QString::number(Threshold[0])+"\n"
                                  "ch1:"+QString::number(Threshold[1])+"\n"
                                  "ch2:"+QString::number(Threshold[2])+"\n"
                                  "ch3:"+QString::number(Threshold[3])+"\n"
                                  "ch4:"+QString::number(Threshold[4])+"\n"
                                  "ch5:"+QString::number(Threshold[4])+"\n"
                                  "ch6:"+QString::number(Threshold[6])+"\n"
                                  "ch7:"+QString::number(Threshold[7])+"\n");

}//on_SetTh_clicked() end!

void MainWindow::StartSendDataSlot(){

    QByteArray FPGA("0e0001");
    QByteArray cmd = QByteArray::fromHex(FPGA);
    this->fcmdSocket->write(cmd);
    this->fcmdSocket->flush();//开始发送数据

}

void MainWindow::StopSendDataSlot(){

    QByteArray FPGA("0e0000");
    QByteArray cmd = QByteArray::fromHex(FPGA);
    this->fcmdSocket->write(cmd);
    this->fcmdSocket->flush();//停止发送数据

}


