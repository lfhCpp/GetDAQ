#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "autorunwidget.h"

#include <QResizeEvent>
#include <QTimer>

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    ArrangeConsole();
    // Child widths and cached layout hints settle after the main resize event.
    QTimer::singleShot(0, this, [this] { ArrangeConsole(); });
}

void MainWindow::ArrangeConsole()
{
    if (!ui->StartRun || !ui->MainContentSplitter) return;
    const bool compact = height() < 920;
    const bool embedded = ui->MainContentSplitter->width() >= 1420;
    fAutoRunWidget->setVisible(embedded);
    ui->AutoRunSettingsButton->setVisible(!embedded);
    ui->SetupInputsLayout->setSpacing(compact ? 6 : 12);
    ui->SetupPanelLayout->setContentsMargins(12, (compact || embedded) ? 8 : 14,
                                               12, (compact || embedded) ? 8 : 14);
    ui->SetupPanelLayout->setSpacing((compact || embedded) ? 6 : 16);
    ui->RunPanelLayout->setContentsMargins(12, compact ? 8 : 12,
                                            12, compact ? 8 : 12);
    ui->RunPanelLayout->setSpacing(compact ? 8 : 12);
    ui->SetupPanelTitle->setFixedHeight((compact || embedded) ? 20 : 24);
    ui->RunPanelTitle->setFixedHeight(compact ? 20 : 24);

    const QList<QPushButton *> buttons = {
        ui->StartRun, ui->StopRun, ui->Connect, ui->Disconnect,
        ui->Regress, ui->Progress, ui->Finalize, ui->Recover
    };
    int required = 24 + 7 * ui->Controls->horizontalSpacing();
    for (auto *button : buttons) required += button->sizeHint().width();
    const int columns = ui->RunPanel->width() >= required ? 8 : 4;
    const int previous = ui->RunPanel->property("controlColumns").toInt();
    if (previous != columns) {
        while (QLayoutItem *item = ui->Controls->takeAt(0)) delete item;
        for (int i = 0; i < 8; ++i)
            ui->Controls->setColumnStretch(i, i < columns ? 1 : 0);
        for (int i = 0; i < buttons.size(); ++i)
            ui->Controls->addWidget(buttons[i], i / columns, i % columns);
        ui->RunPanel->setProperty("controlColumns", columns);
    }
    for (auto *button : buttons) button->setFixedHeight(38);

    ui->SetupPanelLayout->invalidate();
    ui->SetupPanel->updateGeometry();
    ui->RunPanelLayout->invalidate();
    ui->RunPanel->setFixedHeight(ui->RunPanelLayout->sizeHint().height());
    ui->LeftContentLayout->invalidate();
    // Eleven parameter rows at normal size; allow local scrolling on smaller screens.
    const int topHeight = qMax(height() >= 920 ? 322 : 286,
                               ui->LeftContentLayout->minimumSize().height());
    ui->MainContentSplitter->setMinimumHeight(topHeight);
    ui->MainContentSplitter->setMaximumHeight(topHeight);
    ui->PlotPanel->setMinimumHeight(height() >= 920 ? 230 : 200);
    ResizeStatusTableToContents();
}
