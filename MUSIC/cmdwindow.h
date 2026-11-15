#ifndef CMDWINDOW_H
#define CMDWINDOW_H

#include <QWidget>

namespace Ui {
class CmdWindow;
}

class CmdWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CmdWindow(QWidget *parent = 0);
    ~CmdWindow();
    static CmdWindow* GetInstance();//静态成员函数只能访问静态成员变量

private slots:
    void on_Send_clicked();

    void on_ClearButton_clicked();

    void on_ReceiveData_clicked();

    void ShowCmdSlot(QByteArray byte);

private:
    Ui::CmdWindow *ui;
    static CmdWindow *fCmdWindow;
};

#endif // CMDWINDOW_H
