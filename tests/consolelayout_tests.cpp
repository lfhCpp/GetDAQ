#include "consolelayout_tests_p.h"
#include "getdaq_test_support_p.h"

#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QWidget>
#include <limits>

#include "consolepresentation.h"
#include "mainwindow.h"
#include "widgetplot2d.h"

namespace GetDAQTestInternal {

void RunConsoleOverviewUsesConfiguredFreshCoBos()
{
    using namespace ConsolePresentation;
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());
    auto second=config.nodes.first();second.id=42;second.name="CoBo 42";
    config.nodes.insert(1,second);
    NodeStatus a;a.id=config.nodes[0].id;a.eccOnline=a.sentryOnline=true;a.state=4;
    a.rateMB=12.5;a.totalGB=100;a.availableGB=80;
    auto b=a;b.id=42;b.state=5;b.rateMB=2.5;b.availableGB=50;
    auto mutant=a;mutant.id=config.nodes.last().id;mutant.rateMB=99999;mutant.availableGB=0;
    auto alien=mutant;alien.id=999;alien.error="not a configured node";
    const auto overview=Summarize(config,{a,b,mutant,alien});
    QCOMPARE(overview.total,2);QCOMPARE(overview.ready,1);QCOMPARE(overview.rateMB,15.0);QCOMPARE(overview.maxDiskPercent,50.0);
    QVERIFY(overview.rateKnown && overview.diskKnown && overview.errorsKnown);QCOMPARE(overview.errors,0);
    const auto partial=Summarize(config,{a,mutant});
    QVERIFY(!partial.rateKnown && !partial.diskKnown && !partial.errorsKnown);QCOMPARE(partial.total,2);
    QCOMPARE(NodeHealth(nullptr,false,config),Unknown);
    a.eccOnline=false;QCOMPARE(NodeHealth(&a,false,config),Unknown);
    a.eccOnline=true;a.transition=3;QCOMPARE(NodeHealth(&a,false,config),Transitioning);
    a.error="control fault";QCOMPARE(NodeHealth(&a,false,config),Fault);
    a.error.clear();a.transition=0;QCOMPARE(NodeHealth(&a,false,config),Healthy);
    b.rateMB=std::numeric_limits<double>::quiet_NaN();b.availableGB=200;
    mutant.error="MuTaNT fault";
    const auto invalid=Summarize(config,{a,b,mutant});
    QVERIFY(!invalid.rateKnown && !invalid.diskKnown);QCOMPARE(invalid.errors,1);
    config.nativeEcc=true;
    const auto native=Summarize(config,{a,b,mutant});QVERIFY(!native.rateKnown && !native.diskKnown);
    const auto empty=Summarize(config,{});QVERIFY(!empty.errorsKnown);QCOMPARE(empty.ready,0);
}
void RunConsoleReferenceLayoutAndLiveSimulation()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());auto config=GetDAQTestSupport::Config(dir.path());
    config.nodes=RunConfig::Default().nodes;
    MainWindow window;window.SetConfig(config);window.show();
    auto *table=window.findChild<QTableWidget *>("StatusTable");
    auto *selector=window.findChild<QComboBox *>("NodeSelector");
    auto *start=window.findChild<QPushButton *>("StartRun");
    auto *stop=window.findChild<QPushButton *>("StopRun");
    QVERIFY(table && selector && start && stop);QCOMPARE(table->rowCount(),config.nodes.size());
    QCOMPARE(table->horizontalHeader()->logicalIndex(0),table->columnCount()-1);
    QCOMPARE(table->columnWidth(table->columnCount()-1),int(ConsolePresentation::LampWidth));
    QVERIFY(window.findChild<QLabel *>("ErrorOverview")->text().contains("unknown"));
    for(const auto size:{QSize(1536,1024),QSize(1280,800)}) {
        window.resize(size);QApplication::processEvents();
        QCOMPARE(window.size(),size);
        const QList<const char *> names={"StartRun","StopRun","Connect","Disconnect","Regress","Progress","Finalize","Recover"};
        for(int i=0;i<names.size();++i) {
            auto *button=window.findChild<QPushButton *>(names[i]);QVERIFY(button && button->isVisible());
            QVERIFY(!button->icon().isNull());QVERIFY(button->height()>=36);
            QTRY_VERIFY(button->parentWidget()->rect().contains(button->geometry()));
            const auto origin=button->mapTo(window.centralWidget(),QPoint());
            QVERIFY(origin.x()>=0 && origin.y()>=0);
            QVERIFY(origin.x()+button->width()<=window.centralWidget()->width());
            for(int j=i+1;j<names.size();++j) {
                auto *other=window.findChild<QPushButton *>(names[j]);
                const QRect a(origin,button->size()),b(other->mapTo(window.centralWidget(),QPoint()),other->size());
                QVERIFY(!a.intersects(b));
            }
        }
        auto *plot=window.findChild<WidgetPlot2D *>();QVERIFY(plot && plot->height()>=118);
        auto *log=window.findChild<QPlainTextEdit *>("Log");QVERIFY(log && log->isVisible());
        auto *plotPanel=window.findChild<QWidget *>("PlotPanel");auto *logPanel=window.findChild<QWidget *>("LogPanel");
        QVERIFY(qAbs(plotPanel->width()-logPanel->width())<=1);
        QCOMPARE(plotPanel->parentWidget(),logPanel->parentWidget());
        QCOMPARE(plotPanel->geometry().top(),logPanel->geometry().top());
        QVERIFY(plotPanel->geometry().right()<logPanel->geometry().left());
        QVERIFY(!window.findChild<QLabel *>("DiskOverview"));
        QVERIFY(log->mapTo(window.centralWidget(),QPoint(0,log->height())).y()<=window.centralWidget()->height());
        if(size.width()==1536) {
            QTRY_COMPARE(window.findChild<QFrame *>("RunPanel")->property("controlColumns").toInt(),8);
            QTRY_COMPARE(table->verticalScrollBar()->maximum(),0);
        }
    }
    window.resize(1536,1024);QApplication::processEvents();
    window.findChild<QPushButton *>("Connect")->click();QTRY_COMPARE(table->item(0,1)->text(),QString("Idle"));
    for(const auto state:{"Described","Prepared","Ready"}) {
        window.findChild<QPushButton *>("Progress")->click();QTRY_COMPARE(table->item(0,1)->text(),QString(state));QTRY_VERIFY(start->isEnabled());
    }
    QTRY_COMPARE(window.findChild<QLabel *>("CoBoOverview")->text(),QString("11 CoBos ready"));
    QCOMPARE(table->item(0,table->columnCount()-1)->data(ConsolePresentation::LampRole).toInt(),int(ConsolePresentation::Healthy));
    selector->setCurrentIndex(3);QCOMPARE(table->currentRow(),3);QCOMPARE(table->item(3,0)->data(Qt::UserRole).toInt(),config.nodes[3].id);
    table->selectRow(5);QCOMPARE(selector->currentIndex(),5);
    const auto screenshotDir=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    if(!screenshotDir.isEmpty()) {QDir().mkpath(screenshotDir);QVERIFY(window.grab().save(QDir(screenshotDir).filePath("ready-1536x1024.png")));}
    start->click();QTRY_VERIFY(stop->isEnabled());QTRY_COMPARE(window.findChild<QLabel *>("Phase")->text(),QString("Running"));
    QTest::qWait(1100);
    for(const auto size:{QSize(1536,1024),QSize(1280,800)}) {
        window.resize(size);QApplication::processEvents();
        QTRY_VERIFY(start->parentWidget()->rect().contains(window.findChild<QPushButton *>("Recover")->geometry()));
        if(!screenshotDir.isEmpty())QVERIFY(window.grab().save(QDir(screenshotDir).filePath(QString("running-%1x%2.png").arg(size.width()).arg(size.height()))));
    }
    stop->click();QTRY_COMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),config.runNumber+1);
    window.close();QTRY_VERIFY(!window.isVisible());
}
void RunCompactConsoleLayoutKeepsParametersAndNodeRowsVisible()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    MainWindow window; window.SetConfig(GetDAQTestSupport::Config(dir.path())); window.show();
    window.resize(1044,760); QApplication::processEvents();
    auto *fields=window.findChild<QWidget *>("FieldsPanel");
    auto *fieldsTable=window.findChild<QTableWidget *>("Fields");
    auto *table=window.findChild<QTableWidget *>("StatusTable");
    QVERIFY(fields && fieldsTable && table);
    QVERIFY(window.minimumWidth() <= 1044);
    QVERIFY(fields->isVisible());
    QVERIFY(fields->width() >= 300);
    QVERIFY(fieldsTable->columnWidth(1) >= 80);
    QVERIFY(fields->mapTo(window.centralWidget(),QPoint(fields->width(),0)).x() <= window.centralWidget()->width());
    QCOMPARE(table->verticalHeader()->defaultSectionSize(),int(ConsolePresentation::RowHeight));
    QCOMPARE(table->rowCount(),2);
    QVERIFY(table->viewport()->height() >= 40);
}
void RunStatusTableUsesOnlyConfiguredNodeHeight()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    RunConfig config=GetDAQTestSupport::Config(dir.path());
    for(int i=2;i<12;++i) {
        auto node=config.nodes.first();
        node.id=i;
        node.name=QStringLiteral("CoBo %1").arg(i);
        config.nodes.push_back(node);
    }
    MainWindow window; window.SetConfig(config); window.show();
    window.resize(1500,1000); QApplication::processEvents();
    auto *table=window.findChild<QTableWidget *>("StatusTable");
    auto *fieldsTable=window.findChild<QTableWidget *>("Fields");
    auto *fields=window.findChild<QWidget *>("FieldsPanel");
    auto *status=window.findChild<QWidget *>("StatusPanel");
    QVERIFY(table && fieldsTable && fields && status);
    QCOMPARE(table->rowCount(),12);
    QVERIFY(table->verticalScrollBar()->maximum()==0);
    QVERIFY(table->height() <= table->horizontalHeader()->sizeHint().height()+12*ConsolePresentation::RowHeight
            +table->horizontalScrollBar()->sizeHint().height()+2*table->frameWidth()+2);
    QVERIFY(status->height() <= table->height()+90);
    QVERIFY(fields->height() >= 300);
    QCOMPARE(fieldsTable->rowCount(),11);
    QVERIFY(fieldsTable->verticalScrollBar()->maximum()==0);
}
void RunConsoleReflowsWhenConfiguredNodesShrink()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    auto config=GetDAQTestSupport::Config(dir.path());config.nodes=RunConfig::Default().nodes;
    MainWindow window;window.SetConfig(config);window.show();
    window.resize(1536,1024);QApplication::processEvents();
    auto *table=window.findChild<QTableWidget *>("StatusTable");
    auto *status=window.findChild<QWidget *>("StatusPanel");
    auto *bottom=window.findChild<QSplitter *>("BottomPanels");
    auto *selector=window.findChild<QComboBox *>("NodeSelector");
    auto *regress=window.findChild<QPushButton *>("NodeRegress");
    auto *progress=window.findChild<QPushButton *>("NodeProgress");
    auto *runPanel=window.findChild<QFrame *>("RunPanel");
    auto *controls=window.findChild<QGridLayout *>("Controls");
    QVERIFY(table && status && bottom && selector && regress && progress && runPanel && controls);
    const int originalTop=bottom->mapTo(window.centralWidget(),QPoint()).y();
    auto *vertical=window.findChild<QSplitter *>("VerticalSplitter");QVERIFY(vertical);
    vertical->setSizes({status->height()-80,bottom->height()+80});QApplication::processEvents();
    QVERIFY(bottom->mapTo(window.centralWidget(),QPoint()).y()<originalTop-40);
    for(const int count:{2,1}) {
        config.nodes.resize(count);window.SetConfig(config);QApplication::processEvents();
        QCOMPARE(table->rowCount(),count);QCOMPARE(selector->count(),count);
        QVERIFY(!selector->isHidden());
        QCOMPARE(table->verticalScrollBar()->maximum(),0);
        QTRY_VERIFY2(bottom->mapTo(window.centralWidget(),QPoint()).y()<originalTop-150,qPrintable(QString("rows=%1 oldTop=%2 top=%3 status=%4 table=%5").arg(count).arg(originalTop).arg(bottom->mapTo(window.centralWidget(),QPoint()).y()).arg(status->height()).arg(table->height())));
        QVERIFY(status->height()-table->height()<=64);
        QVERIFY(selector->geometry().left()-regress->geometry().right()<=10);
        QVERIFY(progress->geometry().left()-selector->geometry().right()<=10);
        QVERIFY(runPanel->height()-controls->geometry().bottom()<=14);
        auto *run=window.findChild<QSpinBox *>("RunNumber");
        auto *experiment=window.findChild<QLineEdit *>("Experiment");
        auto *description=window.findChild<QLineEdit *>("Description");
        QCOMPARE(run->height(),experiment->height());QCOMPARE(run->height(),description->height());
        QVERIFY(run->height()>=32);QVERIFY(run->font().pixelSize()>=14);
        const auto screenshotDir=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
        if(!screenshotDir.isEmpty())QVERIFY(window.grab().save(QDir(screenshotDir).filePath(QString("compact-%1-node-1536x1024.png").arg(count))));
    }
    // The initial lower split is equal and the user can adjust it independently.
    const int left=bottom->sizes().at(0);QVERIFY(qAbs(left-bottom->sizes().at(1))<=1);
    bottom->setSizes({left-100,left+100});QApplication::processEvents();
    QVERIFY(bottom->sizes().at(0)<bottom->sizes().at(1));
}


void RunManualRunNumberRollbackRemainsAvailableUntilRunStarts()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    RunConfig config=GetDAQTestSupport::Config(dir.path()); config.runNumber=12;
    MainWindow window; window.SetConfig(config);
    auto run=window.findChild<QSpinBox *>("RunNumber"); QVERIFY(run);
    auto *description=window.findChild<QLineEdit *>("Description");
    auto *fields=window.findChild<QTableWidget *>("Fields");
    auto *add=window.findChild<QPushButton *>("AddField");
    auto *remove=window.findChild<QPushButton *>("RemoveField");
    QVERIFY(description && fields && add && remove);
    auto *regress=window.findChild<QPushButton *>("Regress");
    auto *progress=window.findChild<QPushButton *>("Progress");
    auto *nodeRegress=window.findChild<QPushButton *>("NodeRegress");
    auto *nodeProgress=window.findChild<QPushButton *>("NodeProgress");
    QVERIFY(regress && progress && nodeRegress && nodeProgress);
    window.show();QApplication::processEvents();
    QVERIFY(regress->mapToGlobal(QPoint(0,0)).x() < progress->mapToGlobal(QPoint(0,0)).x());
    QVERIFY(nodeRegress->mapToGlobal(QPoint(0,0)).x() < nodeProgress->mapToGlobal(QPoint(0,0)).x());
    QCOMPARE(progress->text(), QString("Progress all"));
    QCOMPARE(regress->text(), QString("Regress all"));
    QCOMPARE(nodeProgress->text(), QString("Progress selected node"));
    QCOMPARE(nodeRegress->text(), QString("Regress selected node"));
    auto *startRun=window.findChild<QPushButton *>("StartRun");
    auto *stopRun=window.findChild<QPushButton *>("StopRun");
    QCOMPARE(startRun->text(), QString("Run start"));
    QCOMPARE(stopRun->text(), QString("Run stop"));
    QVERIFY(startRun->mapToGlobal(QPoint(0,0)).x() < stopRun->mapToGlobal(QPoint(0,0)).x());
    QVERIFY(window.findChild<QTableWidget *>("Fields")->columnWidth(0) <= 200);
    QCOMPARE(window.findChild<QTableWidget *>("Fields")->rowHeight(0),20);
    auto *top=window.findChild<QSplitter *>("MainContentSplitter");
    QVERIFY(top);
    auto *leftColumn=window.findChild<QWidget *>("LeftColumn");
    auto *fieldsColumn=window.findChild<QWidget *>("FieldsColumn");
    auto *fieldsPanel=window.findChild<QWidget *>("FieldsPanel");
    auto *lower=window.findChild<QSplitter *>("VerticalSplitter");
    auto *statusPanel=window.findChild<QWidget *>("StatusPanel");
    auto *plotPanel=window.findChild<QWidget *>("PlotPanel");
    auto *logPanel=window.findChild<QWidget *>("LogPanel");
    auto *connectionControls=window.findChild<QLayout *>("Controls");
    auto *runControls=window.findChild<QLayout *>("Controls");
    QVERIFY(leftColumn && fieldsColumn && fieldsPanel && lower && statusPanel && plotPanel && logPanel);
    QVERIFY(connectionControls && runControls);
    QVERIFY(fieldsPanel->width() >= 300);
    QVERIFY(top->height() >= 180);
    QVERIFY(top->height() <= window.centralWidget()->height() * 0.58);
    QVERIFY(fieldsPanel->height() >= top->height() - 4);
    QVERIFY(fieldsColumn->mapToGlobal(QPoint(0,0)).x()>leftColumn->mapToGlobal(QPoint(0,0)).x());
    QVERIFY(fieldsPanel->mapToGlobal(QPoint(0,0)).y()<=leftColumn->mapToGlobal(QPoint(0,0)).y());
    QVERIFY(lower->mapToGlobal(QPoint(0,0)).y()>top->mapToGlobal(QPoint(0,0)).y());
    QVERIFY(lower->width()>fieldsPanel->width());
    QCOMPARE(lower->width(), top->width());
    QCOMPARE(statusPanel->width(), lower->width());
    QVERIFY(qAbs(plotPanel->width()-logPanel->width())<=1);
    QCOMPARE(plotPanel->parentWidget(),logPanel->parentWidget());
    QVERIFY(plotPanel->mapToGlobal(QPoint()).x()<logPanel->mapToGlobal(QPoint()).x());
    QCOMPARE(plotPanel->mapToGlobal(QPoint()).y(),logPanel->mapToGlobal(QPoint()).y());
    QVERIFY(fieldsPanel->mapToGlobal(QPoint(0,0)).y()<logPanel->mapToGlobal(QPoint(0,0)).y());
    auto *connectButton=window.findChild<QPushButton *>("Connect");
    QVERIFY(connectButton);
    QVERIFY(connectButton->mapToGlobal(QPoint(0,0)).y()>=startRun->mapToGlobal(QPoint(0,0)).y());
    QCOMPARE(run->value(),12);
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,
        Q_ARG(QString,QString("Monitoring")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
    QVERIFY(run->isEnabled());
    QVERIFY(description->isEnabled() && fields->isEnabled() && add->isEnabled() && remove->isEnabled());
    run->setValue(3);
    QCOMPARE(run->value(),3);
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,
        Q_ARG(QString,QString("Running")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,true)));
    QVERIFY(!run->isEnabled());
    QVERIFY(!description->isEnabled() && !fields->isEnabled() && !add->isEnabled() && !remove->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,
        Q_ARG(QString,QString("Disconnected")),Q_ARG(bool,false),Q_ARG(bool,false),Q_ARG(bool,false)));
    QVERIFY(run->isEnabled());
    QCOMPARE(run->value(),3);
}

void RunElapsedShowsHoursMinutesSeconds()
{
    using ConsolePresentation::ElapsedText;
    QCOMPARE(ElapsedText(-1),QString("Elapsed: 0 h 00 min 00 s"));
    QCOMPARE(ElapsedText(59999),QString("Elapsed: 0 h 00 min 59 s"));
    QCOMPARE(ElapsedText(60000),QString("Elapsed: 0 h 01 min 00 s"));
    QCOMPARE(ElapsedText(3599999),QString("Elapsed: 0 h 59 min 59 s"));
    QCOMPARE(ElapsedText(3600000),QString("Elapsed: 1 h 00 min 00 s"));
    QCOMPARE(ElapsedText((25LL*3600+62)*1000),QString("Elapsed: 25 h 01 min 02 s"));
    MainWindow window;QCOMPARE(window.findChild<QLabel *>("Elapsed")->text(),ElapsedText(0));
}
} // namespace GetDAQTestInternal
