#ifndef GETDAQ_SERVICE_LOG_PANEL_H
#define GETDAQ_SERVICE_LOG_PANEL_H
#include <QWidget>
#include <QJsonObject>
#include <QMap>
class QPlainTextEdit;class QStackedWidget;class QComboBox;class QToolButton;
class ServiceLogPanel:public QWidget {
 Q_OBJECT
public:
 explicit ServiceLogPanel(QPlainTextEdit *events,QWidget *parent=nullptr);
 void SetProject(const QString &path);
 QString VisibleText()const;
private:
 QPlainTextEdit *fEvents,*fService;QStackedWidget *fPages;QComboBox *fInstances;QToolButton *fData;
 QString fProject,fSelection,fLastError;int fKind=0;bool fBusy=false;
 QMap<QString,QJsonObject> fCursors;QMap<QString,QString> fBuffers;
 void Select(int kind);void ReloadInstances();void Poll();
};
#endif
