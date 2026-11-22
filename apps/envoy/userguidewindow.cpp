#include "userguidewindow.h"
#include "runconfig.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QFont>
#include <QListWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextCursor>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFile>
#include <QFileDialog>
#include <QSaveFile>
#include <QDir>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QPointer>

UserGuideWindow::UserGuideWindow(const QString &language,QWidget *parent):QDialog(parent)
{
    setObjectName("UserGuideWindow");setAttribute(Qt::WA_DeleteOnClose);resize(1100,780);
    auto *layout=new QVBoxLayout(this);auto *toolbar=new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("Language"),this));fLanguage=new QComboBox(this);fLanguage->setObjectName("GuideLanguage");fLanguage->addItem(QString::fromUtf8("中文 (Chinese)"),"zh");fLanguage->addItem("English","en");toolbar->addWidget(fLanguage);
    fSearch=new QLineEdit(this);fSearch->setObjectName("GuideSearch");fSearch->setPlaceholderText(tr("Find in guide…"));toolbar->addWidget(fSearch,1);auto *find=new QPushButton(tr("Find next"),this);find->setObjectName("GuideFindNext");toolbar->addWidget(find);auto *save=new QPushButton(tr("Save PDF…"),this);save->setObjectName("GuideSavePdf");toolbar->addWidget(save);layout->addLayout(toolbar);
    auto *splitter=new QSplitter(this);fChapters=new QListWidget(splitter);fChapters->setObjectName("GuideChapters");fChapters->setMinimumWidth(180);fText=new QTextBrowser(splitter);fText->setObjectName("GuideText");static const int guideFont=QFontDatabase::addApplicationFont(":/getdaq/manual/fonts/guide.otf");const auto families=QFontDatabase::applicationFontFamilies(guideFont);if(!families.isEmpty()){const QFont font(families.first(),11);fText->document()->setDefaultFont(font);fLanguage->setFont(font);fChapters->setFont(font);}fText->setOpenExternalLinks(true);fText->setTextInteractionFlags(Qt::TextBrowserInteraction);splitter->setSizes({260,840});splitter->setStretchFactor(1,1);layout->addWidget(splitter,1);
    auto *footer=new QHBoxLayout;fStatus=new QLabel(this);fStatus->setObjectName("GuideStatus");footer->addWidget(fStatus,1);auto *close=new QPushButton(tr("Close"),this);footer->addWidget(close);layout->addLayout(footer);
    connect(fLanguage,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{SetLanguage(fLanguage->currentData().toString());});
    connect(fChapters,&QListWidget::currentRowChanged,this,[this](int row){if(row>=0){fText->scrollToAnchor(fChapters->item(row)->data(Qt::UserRole).toString());fText->viewport()->update();fChapters->viewport()->update();}});
    connect(find,&QPushButton::clicked,this,&UserGuideWindow::FindNext);connect(fSearch,&QLineEdit::returnPressed,this,&UserGuideWindow::FindNext);connect(save,&QPushButton::clicked,this,&UserGuideWindow::SavePdf);connect(close,&QPushButton::clicked,this,&QDialog::close);SetLanguage(language);
}
void UserGuideWindow::SetLanguage(const QString &language)
{
    fSelectedLanguage=language=="en"?QString("en"):QString("zh");const QSignalBlocker comboBlocker(fLanguage);fLanguage->setCurrentIndex(fLanguage->findData(fSelectedLanguage));
    const QString resource=":/getdaq/manual/guide-"+fSelectedLanguage+".html";QFile file(resource);
    if(!file.open(QIODevice::ReadOnly)){fText->setPlainText(tr("The embedded guide is missing. Reinstall the current GetDAQ program."));return;}
    const auto html=QString::fromUtf8(file.readAll());const QSignalBlocker chaptersBlocker(fChapters);fChapters->clear();QRegularExpression chapter("<h1 id=\"([^\"]+)\">(.*?)</h1>");auto matches=chapter.globalMatch(html);
    while(matches.hasNext()){const auto match=matches.next();QTextDocument title;title.setHtml(match.captured(2));auto *item=new QListWidgetItem(title.toPlainText(),fChapters);item->setData(Qt::UserRole,match.captured(1));item->setToolTip(title.toPlainText());}
    fText->setSource(QUrl("qrc:/getdaq/manual/guide-"+fSelectedLanguage+".html"));if(fChapters->count()){fChapters->setCurrentRow(0);fText->scrollToAnchor(fChapters->item(0)->data(Qt::UserRole).toString());}
    setWindowTitle(fSelectedLanguage=="en"?tr("GetDAQ User Guide"):QString::fromUtf8("GetDAQ 使用说明"));fStatus->setText(tr("Embedded offline guide · %1 chapters · PDF included").arg(fChapters->count()));
}
void UserGuideWindow::FindNext()
{
    if(fSearch->text().isEmpty())return;
    if(fText->find(fSearch->text()))return;
    auto cursor=fText->textCursor();cursor.movePosition(QTextCursor::Start);fText->setTextCursor(cursor);
    if(!fText->find(fSearch->text()))fStatus->setText(tr("No matching text in this language guide."));
}
void UserGuideWindow::SavePdf()
{
    const auto directory=DefaultWorkspaceDirectory()+"/manuals";QDir().mkpath(directory);const auto name=fSelectedLanguage=="en"?QString("GetDAQ-Quick-Guide-en.pdf"):QString("GetDAQ-Quick-Guide.pdf");
    const auto path=QFileDialog::getSaveFileName(this,tr("Save embedded PDF guide"),QDir(directory).filePath(name),tr("PDF documents (*.pdf)"));if(path.isEmpty())return;
    QFile source(":/getdaq/manual/guide-"+fSelectedLanguage+".pdf");QSaveFile output(path);bool saved=false;
    if(source.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly)){const auto bytes=source.readAll();saved=output.write(bytes)==bytes.size() && output.commit();}
    if(!saved)QMessageBox::warning(this,tr("Save guide"),tr("Could not save the PDF guide to %1").arg(path));else fStatus->setText(tr("PDF saved: %1").arg(path));
}
void OpenUserGuide(QWidget *parent,const QString &language)
{
    static QPointer<UserGuideWindow> window;
    if(!window)window=new UserGuideWindow(language,parent);else window->SetLanguage(language);
    window->show();window->raise();window->activateWindow();
}
