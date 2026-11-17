#include "mainwindow.h"
#include "ui_mainwindow.h"
void MainWindow::on_NodeProgress_clicked(){emit ProgressSignal(SelectedId());}
void MainWindow::on_NodeRegress_clicked(){emit RegressSignal(SelectedId());}

int MainWindow::SelectedId() const{const int row=ui->StatusTable->currentRow();return row<0?-1:ui->StatusTable->item(row,0)->data(Qt::UserRole).toInt();}
