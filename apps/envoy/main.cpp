#include "mainwindow.h"
#include "applicationinstance.h"
#include "earlywarningrecovery.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#include <QUuid>
#include <QSettings>
#include <QFile>
#include "coboconnectionwindow.h"
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
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    QCommandLineParser parser;
    parser.setApplicationDescription("AT-TPC envoy control console; simulation by default.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({{"c", "config"}, "Load a YAML configuration.", "path"});
    parser.addOption({"inspect-warning-recovery", "Read-only verification of the bounded early delay warning recovery; does not reset or write hardware."});
    parser.addOption({"new-instance", "Open a separate local deployment setup, using --config only as an electronics template."});
    parser.addOption({"cobo-readonly", "Open and verify a direct read-only CoBo connection; requires GETDAQ_COBO_IPV4."});
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
    if(parser.isSet("inspect-warning-recovery")) {
        RunConfig config;QString error;if(!config.Load(configuration,&error)){qWarning("%s",qPrintable(error));return 2;}
        EarlyWarningRecovery probe(config,QCoreApplication::applicationDirPath());
        QObject::connect(&probe,&EarlyWarningRecovery::Log,&app,[](const QString &message){qInfo("%s",qPrintable(message));});
        QObject::connect(&probe,&EarlyWarningRecovery::Failed,&app,[&app](const QString &message){qWarning("%s",qPrintable(message));app.exit(2);});
        QObject::connect(&probe,&EarlyWarningRecovery::Verified,&app,[&app](const QJsonObject &proof){qInfo("Early warning recovery verified read-only: %s",qPrintable(proof["failure_field"].toString()));app.exit(0);});
        QTimer::singleShot(0,&probe,&EarlyWarningRecovery::Start);return app.exec();
    }
    ApplicationInstance instance(LocalInstallationDirectory());
    const auto scope=parser.isSet("new-instance")?QDir::tempPath()+"/getdaq-new-"+QUuid::createUuid().toString(QUuid::WithoutBraces):configuration;
    if(!instance.Acquire(scope)) {
        if(instance.Error().isEmpty()){qInfo("Existing GetDAQ window activated");return 0;}
        QMessageBox::critical(nullptr,"GetDAQ startup",instance.Error());return 1;
    }
    MainWindow window;instance.SetWindow(&window);window.SetApplicationInstance(&instance);
    if (!parser.isSet("new-instance") && !configuration.isEmpty() && !window.LoadConfig(configuration)) {
        QTimer::singleShot(0, &window, &QWidget::close);
    }
    window.show();
    if(parser.isSet("new-instance"))QTimer::singleShot(0,&window,[&window,configuration]{window.CreateParallelInstance(configuration);});
    if(parser.isSet("cobo-readonly"))QTimer::singleShot(0,&window,[&window]{CoBoConnectionWindow connection(&window,true);connection.exec();});
    if (parser.isSet("screenshot")) QTimer::singleShot(300, &window, [&window, &parser]() {
        window.grab().save(parser.value("screenshot")); QCoreApplication::quit();
    });
    return app.exec();
}
