#ifndef GETDAQ_SERVICE_LOG_PANEL_H
#define GETDAQ_SERVICE_LOG_PANEL_H
#include <QWidget>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVector>
#include "logtimeline.h"
class QPlainTextEdit;class QStackedWidget;class QComboBox;class QToolButton;
class ServiceLogPanel:public QWidget {
 Q_OBJECT
public:
 explicit ServiceLogPanel(QPlainTextEdit *events,QWidget *parent=nullptr);
 void SetProject(const QString &path);
 QString VisibleText()const;
 QString CombinedText()const;
 void RecordEvent(const QString &line);
 static QString PresentLog(QString text);
private:
 QPlainTextEdit *fEvents,*fService;QStackedWidget *fPages;QComboBox *fInstances;QToolButton *fData;
 QString fProject,fProjectRevision,fSelection;int fKind=0,fNextStream=0;bool fProjectSet=false;
 QVector<QJsonObject> fStreams;QSet<QString> fInFlight;
 QMap<QString,QString> fErrors;QMap<QString,int> fClearEpoch;LogTimeline fTimeline;
 QMap<QString,QJsonObject> fCursors;QMap<QString,QString> fBuffers;
 QString ProjectFile()const;
 QString StreamKey(const QJsonObject &entry)const;
 void Select(int kind);void ReloadInstances();void ReadStream(const QJsonObject &entry);void ShowSelected();
private slots:
 void Poll();
 void ClearVisible();
};
#endif
