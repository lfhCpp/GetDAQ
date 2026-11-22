#include "mainwindow.h"
#include "applicationinstance.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#include <QUuid>
#include <QSettings>
#include <QFile>
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GetDAQ");
    QCoreApplication::setApplicationName("GetDAQ");
    QCoreApplication::setApplicationVersion("1.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    QCommandLineParser parser;
    parser.setApplicationDescription("AT-TPC acquisition control console.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({{"c", "config"}, "Load a YAML configuration.", "path"});
    parser.addOption({"screenshot", "Save initial window screenshot and exit (no connection).", "path"});
    parser.process(app);
    QString siteError;
    if(!LoadLocalSiteEnvironment(LocalInstallationDirectory(),&siteError)){QMessageBox::critical(nullptr,"GetDAQ startup",siteError);return 1;}
    QString configuration=parser.value("config");
    if(!parser.isSet("config")) {
        configuration=QSettings().value(StartupConfigurationKey()).toString();
        if(configuration.isEmpty()) {
            QFile defaultConfig(LocalInstallationDirectory()+"/site/gui-config.txt");
            if(defaultConfig.open(QIODevice::ReadOnly))configuration=QString::fromUtf8(defaultConfig.readAll()).trimmed();
        }
    }

    ApplicationInstance instance(LocalInstallationDirectory());
    const auto scope=configuration;
    if(!instance.Acquire(scope)) {
        if(instance.Error().isEmpty()){qInfo("Existing GetDAQ window activated");return 0;}
        QMessageBox::critical(nullptr,"GetDAQ startup",instance.Error());return 1;
    }
    MainWindow window;instance.SetWindow(&window);window.SetApplicationInstance(&instance);
    if (!configuration.isEmpty() && !window.LoadConfig(configuration)) {
        qWarning("Saved configuration was not loaded. Select or create an ECC configuration in the open window.");
    }
    window.show();
    if (parser.isSet("screenshot")) QTimer::singleShot(300, &window, [&window, &parser]() {
        window.grab().save(parser.value("screenshot")); QCoreApplication::quit();
    });
    return app.exec();
}
