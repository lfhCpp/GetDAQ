#include "mainwindow.h"
#include "applicationinstance.h"
#include <QSettings>
void MainWindow::RememberStartupConfiguration(){if(!fConfig.filePath.isEmpty())QSettings().setValue(StartupConfigurationKey(),fConfig.filePath);}
