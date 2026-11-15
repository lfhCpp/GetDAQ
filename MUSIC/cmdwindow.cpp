// Description:  CmdWindow
// Author:       Fenhua Lu
// Email:        lufenhua@impcas.ac.cn
// Created:      20220720 20:38
// Last-Updated: 20240424 09:37
//           By: Fenhua Lu
//     Update #: 3.7


#include "cmdwindow.h"
#include "ui_cmdwindow.h"
#include <mainwindow.h>
#include "ipwindow.h"
#include <QDebug>

#define cout qDebug()<<"["<<__FILE__<<":"<<__LINE__<<"]"
class MainWindow;

CmdWindow* CmdWindow::fCmdWindow = nullptr;//类内静态变量初始化
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

CmdWindow::CmdWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CmdWindow)
{
    ui->setupUi(this);
    fCmdWindow=this;
    //将信息显示在信息窗
    this->ui->textBrowser->append("指令（24bit）  含义 \n01 0001\t开始数据传输\n"
                                  "01 0000\t结束数据传输\n"
                                  "02 0001\t传输1个通道\n"
                                  "02 0002\t传输2个通道\n"
                                  "02 0004\t传输4个通道\n"
                                  "02 0008\t传输8个通道\n"
                                  "03 xxxx\t设置第1个通道触发阈值\n"
                                  "04 xxxx\t设置第2个通道触发阈值\n"
                                  "0a xxxx\t设置第8个通道触发阈值\n"
                                  "0b 0001\t清零序列号和时间戳\n"
                                  "0c xxxx\t设置触发间隔\n"
                                  "0d xxxx\t设置触发间隔系数\n"
                                  "0e 0001\t重启数据板\n");

   //连接信号与槽，接收cmd以太网发送的数据
    IPWindow* ipWindow =IPWindow::GetInstance();
    QObject::connect(ipWindow, &IPWindow::SendcmdSocketByte, this, &CmdWindow::ShowCmdSlot);
}

CmdWindow::~CmdWindow(){

    if(ui!=nullptr){
        delete ui;
        ui=nullptr;
    }
}

CmdWindow* CmdWindow::GetInstance(){
    if (fCmdWindow==nullptr) {
      fCmdWindow = new CmdWindow();
    }
    return fCmdWindow;
}

void CmdWindow::on_Send_clicked(){

   MainWindow* mainWindow =MainWindow::GetInstance();

   //判断套接字连接情况
   bool fWhetherConnect=mainWindow->WhetherConnect(mainWindow->GetcmdSocket());

   //将连接信息显示在信息窗口
   if(!fWhetherConnect){
      this->ui->textBrowser->append("千兆网口未连接，请先连接！！！");//不用settext 这样会覆盖之前的消息
      return;
   }

   //获取Cmd命令
   QString CMD = this->ui->CmdlineEdit->text();//03000001
   QByteArray Byte_CMD = CMD.toLatin1();//按照ASCII编码转换，无法转换中文
   QByteArray FPGA_CMD = QByteArray::fromHex(Byte_CMD);
   cout<<"QString CMD :"<<CMD<<"  Byte_CMD"<<Byte_CMD<<"  FPGA_CMD"<<FPGA_CMD;

   //将信息显示在信息窗
   this->ui->textBrowser->append("上位机："+CMD);

   //将信息写入到通信套接字
   mainWindow->GetcmdSocket()->write(FPGA_CMD);
   mainWindow->GetcmdSocket()->flush();





}
void CmdWindow::ShowCmdSlot(QByteArray byte){
    //获取通信套接字发送过来的内容在显示编辑区域显示
    this->ui->textBrowser->append("数据板千兆网口："+byte.toHex().toUpper());//不用settext 这样会覆盖之前的消息
}


void CmdWindow::on_ClearButton_clicked(){
     this->ui->textBrowser->clear();
}

void CmdWindow::on_ReceiveData_clicked(){
   Multithread* multithread =Multithread::GetInstance();
    //判断套接字连接情况
    bool fWhetherConnect=multithread->Multithread::WhetherConnect(multithread->GetDataSocket());

    //将连接信息显示在信息窗口
    if(!fWhetherConnect){
       this->ui->textBrowser->append("万兆网口未连接，请先连接！！！");//不用settext 这样会覆盖之前的消息
       return;
    }
    this->ui->textBrowser->append("BeginReceiveData");
    //获取通信套接字的内容
    QByteArray byte=multithread->GetDataSocket()->readAll();
    //在显示编辑区域显示
    this->ui->textBrowser->append("数据板万兆网口："+byte.toHex().toUpper());//不用settext 这样会覆盖之前的消息

}
