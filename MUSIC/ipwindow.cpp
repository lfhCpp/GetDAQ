// Description:  IPWindow
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20240424 09:37
//           By: Fenhua Lu
//     Update #: 3.7


#include "ipwindow.h"
#include "ui_ipwindow.h"
#include <mainwindow.h>
#include <QDebug>


#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"
class MainWindow;

IPWindow* IPWindow::fIPWindow = nullptr;//类内静态变量初始化
Multithread* IPWindow::fMultithread = nullptr;//类内静态变量初始化
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

IPWindow::IPWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::IPWindow)
{
    this->ui->setupUi(this);
    this->fIPWindow=this;
    this->ui->IPAdress_CmdNet->setText("10.16.8.197");
    this->ui->Port_CmdNet->setText("22");
    this->ui->IPAdress_DataNet->setText("10.16.121.164");
    this->ui->Port_DataNet->setText("8888");

//    this->ui->IPAdress_DataNet->setText("192.168.1.10");
//    this->ui->Port_DataNet->setText("1024");
//    this->ui->IPAdress_CmdNet->setText("192.168.2.3");
//    this->ui->Port_CmdNet->setText("24");

    IPWindow::on_CmdNetConnect_clicked();//程序初始化时，自动连接一次
    IPWindow::on_DataNetConnect_clicked();

    //连接千兆网
    MainWindow* mainWindow =MainWindow::GetInstance();
    connect(mainWindow ,&MainWindow::EmitConnectCmdSignal,this,&IPWindow::CmdNetConnect);
    connect(mainWindow ,&MainWindow::EmitConnectDataSignal,this,&IPWindow::DataNetConnect);
}

IPWindow::~IPWindow(){

    cout<<"~~IPWindow()";

    if(ui!=nullptr){
        delete ui;
        ui=nullptr;
    }
    if(fMultithread!=nullptr){
        delete fMultithread;
        fMultithread=nullptr;
    }
}

IPWindow* IPWindow::GetInstance(){
    if (fIPWindow==nullptr) {
      fIPWindow = new IPWindow();
    }
    return fIPWindow;
}

void IPWindow::on_CmdNetConnect_clicked(){

     MainWindow* mainWindow =MainWindow::GetInstance();

    //获取ip地址和端口号
    QString IP = this->ui->IPAdress_CmdNet->text();
    QString port=this->ui->Port_CmdNet->text();

    //如果为空指针，重新创建socket对象
    if(mainWindow->GetcmdSocket()==nullptr){
        mainWindow->NewcmdSocket();
    }

    // 检查是否已经连接
    if(mainWindow->GetcmdSocket()->state() == QAbstractSocket::ConnectedState) {
           // 如果已经连接，则不执行连接操作void IPWindow::CmdNetConnect()
           return;
    }

    //如有连接，则断开之前的连接 注意：仅断开信号与槽的连接
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::connected, 0, 0);
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::disconnected, 0, 0);
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::readyRead, 0, 0);

    //连接数据采集版
    mainWindow->GetcmdSocket()->connectToHost(QHostAddress(IP), port.toShort());


    //连接数据采集版成功，socket对象会发出信号
    connect(mainWindow->GetcmdSocket(), &QTcpSocket::connected, [this](){
        //QMessageBox::information(this, "连接提示", "千兆网口连接成功");
        emit ControlCmdNetLED();
    });

    //连接断开，socket会发出信号
    connect(mainWindow->GetcmdSocket(), &QTcpSocket::disconnected, [this](){
        //QMessageBox::warning(this, "连接提示", "千兆网口连接已断开！");
        emit DisableCmdNetLED();
    });

    //获取socket的信息并发射出去
    connect(mainWindow->GetcmdSocket(),&QTcpSocket::readyRead,[=](){
        //获取通信套接字的内容
         QByteArray byte=mainWindow->GetcmdSocket()->readAll();
         emit SendcmdSocketByte(byte);
    });
}

void IPWindow::CmdNetConnect(){

     MainWindow* mainWindow =MainWindow::GetInstance();

    //获取ip地址和端口号
    QString IP = this->ui->IPAdress_CmdNet->text();
    QString port=this->ui->Port_CmdNet->text();

    //如果为空指针，重新创建socket对象
    if(mainWindow->GetcmdSocket()==nullptr){
        mainWindow->NewcmdSocket();
    }

    // 检查是否已经连接
    if(mainWindow->GetcmdSocket()->state() == QAbstractSocket::ConnectedState) {
           // 如果已经连接，则不执行连接操作
           return;
    }

    //如有连接，则断开之前的连接 注意：仅断开信号与槽的连接
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::connected, 0, 0);
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::disconnected, 0, 0);
    disconnect(mainWindow->GetcmdSocket(), &QTcpSocket::readyRead, 0, 0);

    //连接数据采集版
    mainWindow->GetcmdSocket()->connectToHost(QHostAddress(IP), port.toShort());


    //连接数据采集版成功，socket对象会发出信号
    connect(mainWindow->GetcmdSocket(), &QTcpSocket::connected, [this](){
        //QMessageBox::information(this, "连接提示", "千兆网口连接成功");
        emit ControlCmdNetLED();
    });

    //连接断开，socket会发出信号
    connect(mainWindow->GetcmdSocket(), &QTcpSocket::disconnected, [this](){
        //QMessageBox::warning(this, "连接提示", "千兆网口连接已断开！");
        emit DisableCmdNetLED();
    });

    //获取socket的信息并发射出去
    connect(mainWindow->GetcmdSocket(),&QTcpSocket::readyRead,[=](){
        //获取通信套接字的内容
         QByteArray byte=mainWindow->GetcmdSocket()->readAll();
         emit SendcmdSocketByte(byte);
    });
}

void IPWindow::on_DataNetConnect_clicked(){

    Multithread* multithread =Multithread::GetInstance();

    //获取ip地址和端口号
    QString IP = this->ui->IPAdress_DataNet->text();
    QString port=this->ui->Port_DataNet->text();

    // 检查是否已经连接
    if(multithread->GetDataSocket()->state() == QAbstractSocket::ConnectedState) {
           // 如果已经连接，则不执行连接操作
           return;
    }

    //如有连接，则断开之前的连接 注意：仅断开信号与槽的连接
    disconnect(multithread->GetDataSocket(), &QTcpSocket::connected, 0, 0);
    disconnect(multithread->GetDataSocket(), &QTcpSocket::disconnected, 0, 0);

    //连接数据采集版
    multithread->GetDataSocket()->connectToHost(QHostAddress(IP), port.toShort());


    //连接数据采集版成功，socket对象会发出信号
    connect(multithread->GetDataSocket(), &QTcpSocket::connected, [this](){
        //QMessageBox::information(this, "连接提示", "万兆网口连接成功");
        emit ControlDataNetLED();
        this->hide();
    });


    //连接断开，socket会发出信号
    connect(multithread->GetDataSocket(), &QTcpSocket::disconnected, [this](){
        QMessageBox::warning(this, "连接提示", "万兆网口连接已断开！");
        emit DisableDataNetLED();
    });

}

void IPWindow::DataNetConnect(){

    Multithread* multithread =Multithread::GetInstance();

    //获取ip地址和端口号
    QString IP = this->ui->IPAdress_DataNet->text();
    QString port=this->ui->Port_DataNet->text();

    // 检查是否已经连接
    if(multithread->GetDataSocket()->state() == QAbstractSocket::ConnectedState) {
           // 如果已经连接，则不执行连接操作
           return;
    }

    //如有连接，则断开之前的连接 注意：仅断开信号与槽的连接
    disconnect(multithread->GetDataSocket(), &QTcpSocket::connected, 0, 0);
    disconnect(multithread->GetDataSocket(), &QTcpSocket::disconnected, 0, 0);

    //连接数据采集版
    multithread->GetDataSocket()->connectToHost(QHostAddress(IP), port.toShort());


    //连接数据采集版成功，socket对象会发出信号
    connect(multithread->GetDataSocket(), &QTcpSocket::connected, [this](){
        //QMessageBox::information(this, "连接提示", "万兆网口连接成功");
        emit ControlDataNetLED();
        this->hide();
    });


    //连接断开，socket会发出信号
    connect(multithread->GetDataSocket(), &QTcpSocket::disconnected, [this](){
        QMessageBox::warning(this, "连接提示", "万兆网口连接已断开！");
        emit DisableDataNetLED();
    });

}


void IPWindow::on_CmdNetDisconnect_clicked(){
     MainWindow* mainWindow =MainWindow::GetInstance();

     //如果为空指针,返回
     if(mainWindow->GetcmdSocket()==nullptr){
        return;
     }

     //取消已有的连接
     //mainWindow->GetcmdSocket()->abort();
     //断开连接
     mainWindow->GetcmdSocket()->disconnectFromHost();
     mainWindow->GetcmdSocket()->close();
}


void IPWindow::on_DataNetDisconnect_clicked(){
    Multithread* multithread =Multithread::GetInstance();
    //取消已有的连接
    //mainWindow->GetDataSocket()->abort();
    //断开连接
    multithread->GetDataSocket()->disconnectFromHost();
    multithread->GetDataSocket()->close();
}

/*
QByteArray()：构造函数，创建一个空的QByteArray对象
QByteArray(const char * data, int size = -1)：构造函数，使用data指针中的数据和size指定的大小初始化QByteArray对象
append(const char * data, int size)：将data指向的二进制数据添加到QByteArray末尾
prepend(const char * data, int size)：将data指向的二进制数据添加到QByteArray开头
insert(int pos, const char * data, int size)：在QByteArray中的pos位置插入data指向的二进制数据
remove(int pos, int len)：从QByteArray中删除由pos位置开始、长度为len的一段二进制数据
clear()：清空QByteArray中的所有数据
count()：获取QByteArray中的数据字节数
truncate(int pos)：缩短QByteArray的大小，使其包含pos个字节
resize(int size)：调整QByteArray的大小
toBase64()：将QByteArray转换为Base64编码的字符串，用于加密或网络传输
fromBase64(const QByteArray & base64)：将Base64编码的字符串转换为QByteArray
at(int i)：返回QByteArray中第i个字节的引用，可以进行修改
operator[](int i)：返回QByteArray中第i个字节的值
left(int len) / right(int len) / mid(int pos, int len)：分别返回QByteArray中左数、右数、指定位置开始、长度为len的一段二进制数据
indexOf(char c, int from = 0) / lastIndexOf(char c, int from = -1)：分别返回QByteArray中字符c（字节）出现的第一个/最后一个位置的索引
contains(const QByteArray & ba)：判断QByteArray中是否包含ba这个子串

编码问题：QByteArray同时支持文本和二进制数据，使用setText()和text()方法可以将二进制数据转换为文字，或将文字转换为二进制数据。在进行编码转换时，需要确保编码方式正确，并做好异常处理。
QByteArray::toHex：将字节数组转换为十六进制字符串。
QByteArray::fromHex：将十六进制字符串转换为字节数组。
*/
