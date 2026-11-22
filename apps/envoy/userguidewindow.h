#ifndef GETDAQ_USER_GUIDE_WINDOW_H
#define GETDAQ_USER_GUIDE_WINDOW_H
#include <QDialog>
class QComboBox;
class QListWidget;
class QTextBrowser;
class QLineEdit;
class QLabel;
class UserGuideWindow:public QDialog {
public:
    explicit UserGuideWindow(const QString &language,QWidget *parent=nullptr);
    void SetLanguage(const QString &language);
private:
    QComboBox *fLanguage;
    QListWidget *fChapters;
    QTextBrowser *fText;
    QLineEdit *fSearch;
    QLabel *fStatus;
    QString fSelectedLanguage;
    void FindNext();
    void SavePdf();
};
void OpenUserGuide(QWidget *parent,const QString &language);
#endif
