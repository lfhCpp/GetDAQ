// Runs against the actual original UI/model/serializer, using temporary files only.
#include "CCfg/Ui/EditorMainWindow.h"
#include "CCfg/Control/Workspace.h"
#include "CCfg/Model/ConfigModel.h"
#include "CCfg/Io/Url.h"
#include "CCfg/Path.h"
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QDebug>
#include <stdexcept>

static void require(bool ok, const char *message) { if(!ok) throw std::runtime_error(message); }
static void write(const QString &name, const QByteArray &data) {
    QFile f(name); require(f.open(QIODevice::WriteOnly),"fixture open");
    require(f.write(data)==data.size(),"fixture write");
}
static QModelIndex valueIndex(CCfg::Model::ConfigModel *model, const char *path) {
    const auto index=model->pathToIndex(CCfg::Path(std::string("Setup.")+path),CCfg::Model::ConfigModel::ValueColumn);
    require(index.isValid(),"configuration path missing");return index;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);Q_INIT_RESOURCE(ConfigWidgets);
    QApplication::setOrganizationName("GetDAQ-Original-Editor-Test");
    QTemporaryDir temp;require(temp.isValid(),"temporary directory");
    try {
        write(temp.path()+"/base.xcfg","<Base><inherited unit=\"ns\">9</inherited></Base>");
        const auto input=temp.path()+"/main.xcfg";
        write(input,"<Setup><gain unit=\"mV\" range=\"[0, 255]\">12</gain><Board include=\"base.xcfg\"><local>7</local></Board></Setup>");
        CCfg::Ui::EditorMainWindow window;window.openFiles({input});window.show();app.processEvents();
        auto *model=window.getWorkspace()->getCurrentModel();require(model,"open original document");
        require(model->data(valueIndex(model,"Board.inherited"),Qt::DisplayRole).toString()=="9","relative include resolution");
        require(model->data(valueIndex(model,"gain"),Qt::DisplayRole).toString()=="12","initial value");
        model->setData(valueIndex(model,"gain"),QString("23"),Qt::EditRole);app.processEvents();
        require(model->data(valueIndex(model,"gain"),Qt::DisplayRole).toString()=="23","edit");
        model->getUndoStack()->undo();app.processEvents();
        require(model->data(valueIndex(model,"gain"),Qt::DisplayRole).toString()=="12","undo");
        model->getUndoStack()->redo();app.processEvents();
        require(model->data(valueIndex(model,"gain"),Qt::DisplayRole).toString()=="23","redo");
        model->saveAs(CCfg::Io::Url((temp.path()+"/saved.xcfg").toStdString()));
        QFile saved(temp.path()+"/saved.xcfg");require(saved.open(QIODevice::ReadOnly),"saved file");const auto xml=saved.readAll();
        require(xml.contains("base.xcfg") && xml.contains("[0, 255]") && xml.contains("mV"),"include, range and unit retained");
        CCfg::Ui::EditorMainWindow reopened;reopened.openFiles({temp.path()+"/saved.xcfg"});
        auto *again=reopened.getWorkspace()->getCurrentModel();require(again,"reopen");
        require(again->data(valueIndex(again,"gain"),Qt::DisplayRole).toString()=="23","saved edited value");
        require(again->data(valueIndex(again,"Board.inherited"),Qt::DisplayRole).toString()=="9","saved include");
        if(argc>1)require(window.grab().save(QString::fromLocal8Bit(argv[1])),"screenshot");
        if(argc>2) {
            CCfg::Ui::EditorMainWindow legacy;
            legacy.openFiles({QString::fromLocal8Bit(argv[2])});
            require(legacy.getWorkspace()->getCurrentModel(),"open historical hardware configuration");
            qInfo()<<"PASS: opened historical hardware configuration";
        }
        qInfo()<<"PASS: original UI open / relative include / edit / undo / redo / save / reopen / metadata";
        return 0;
    } catch(const std::exception &e) { qCritical()<<e.what();return 1; }
}
