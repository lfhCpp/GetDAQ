#include "binaryconfigurationwindow.h"
#include "ui_binaryconfigurationwindow.h"
#include <QFileDialog>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QtConcurrent>
namespace {struct ConversionResult {XcfgBinary::Preview preview;QString error;QByteArray xml;};}

BinaryConfigurationWindow::BinaryConfigurationWindow(const XcfgDocument &document,const QString &directory,QWidget *parent)
    :QDialog(parent),fUi(new Ui::BinaryConfigurationWindow),fDocument(document),fDirectory(directory)
{
    fUi->setupUi(this);
    if(!document.RootFile().isEmpty())fUi->objectPath->setText(document.Nodes()[document.Sources().value(document.RootFile()).root].path);
    connect(fUi->source,&QLineEdit::textChanged,this,&BinaryConfigurationWindow::ClearPreview);
    connect(fUi->objectPath,&QLineEdit::textChanged,this,&BinaryConfigurationWindow::ClearPreview);
    connect(fUi->destination,&QLineEdit::textChanged,this,&BinaryConfigurationWindow::ClearPreview);
    connect(fUi->direction,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{ClearPreview();SetBusy(false);});
    connect(fUi->mode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&BinaryConfigurationWindow::ClearPreview);
    connect(fUi->browseSource,&QPushButton::clicked,this,[this]{const auto p=QFileDialog::getOpenFileName(this,tr("Choose legacy binary configuration"),fDirectory,tr("Binary configuration (*.bcfg)"));if(!p.isEmpty())fUi->source->setText(p);});
    connect(fUi->browseDestination,&QPushButton::clicked,this,[this]{const auto p=QFileDialog::getSaveFileName(this,tr("Choose a NEW conversion output"),fDirectory, fUi->direction->currentIndex()==0 ? tr("XML configuration (*.xcfg)") : tr("Binary configuration (*.bcfg)"),nullptr,QFileDialog::DontConfirmOverwrite);if(!p.isEmpty())fUi->destination->setText(p);});
    connect(fUi->preview,&QPushButton::clicked,this,&BinaryConfigurationWindow::Preview);
    connect(fUi->save,&QPushButton::clicked,this,&BinaryConfigurationWindow::Save);
    connect(fUi->close,&QPushButton::clicked,this,&QDialog::close);SetBusy(false);
}
BinaryConfigurationWindow::~BinaryConfigurationWindow()=default;
void BinaryConfigurationWindow::ClearPreview(){fReviewed=false;fUi->save->setEnabled(false);fUi->review->clear();}
void BinaryConfigurationWindow::SetBusy(bool busy){fBusy=busy;const bool importing=fUi->direction->currentIndex()==0;
    fUi->direction->setEnabled(!busy);fUi->source->setEnabled(!busy && importing);fUi->browseSource->setEnabled(!busy && importing);
    fUi->objectPath->setEnabled(!busy && !importing);fUi->mode->setEnabled(!busy && !importing);fUi->destination->setEnabled(!busy);fUi->browseDestination->setEnabled(!busy);
    fUi->preview->setEnabled(!busy);fUi->save->setEnabled(!busy && fReviewed);
}
void BinaryConfigurationWindow::Preview(){if(fBusy)return;ClearPreview();SetBusy(true);fUi->status->setText(tr("Building and checking conversion preview..."));
    typedef ConversionResult Result;auto watcher=new QFutureWatcher<Result>(this);
    const bool importing=fUi->direction->currentIndex()==0;const auto source=fUi->source->text(),destination=fUi->destination->text();XcfgExporter::Request request;request.destination=destination;request.path=fUi->objectPath->text();request.mode=fUi->mode->currentIndex()==0 ? XcfgExporter::FlattenDeclarations : XcfgExporter::EffectiveConcreteObject;const auto document=fDocument;
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher]{const auto result=watcher->result();watcher->deleteLater();
        if(!result.error.isEmpty()){SetBusy(false);fUi->status->setText(result.error);return;}fPreview=result.preview;fReviewed=true;
        fUi->review->setPlainText(QString::fromUtf8(result.xml));fUi->status->setText(tr("Reviewed %1 objects and %2 scalars; output %3 bytes. SHA-256: %4\nSave creates a new file after rechecking all sources.").arg(fPreview.objects).arg(fPreview.scalars).arg(fPreview.bytes.size()).arg(QString::fromLatin1(QCryptographicHash::hash(fPreview.bytes,QCryptographicHash::Sha256).toHex())));SetBusy(false);
    });
    watcher->setFuture(QtConcurrent::run([importing,source,destination,request,document]()->Result {Result result;const bool ok=importing ? XcfgBinary::PreviewImport(source,destination,&result.preview,&result.error) : XcfgBinary::PreviewExport(document,request,&result.preview,&result.error);
        if(!ok && result.error.isEmpty())result.error="Conversion preview failed";
        if(ok){if(importing)result.xml=result.preview.bytes;else XcfgBinary::Decode(result.preview.bytes,&result.xml,&result.error);}return result;}));
}
void BinaryConfigurationWindow::Save(){if(fBusy || !fReviewed)return;const auto preview=fPreview;SetBusy(true);fUi->status->setText(tr("Rechecking sources and creating the new output..."));auto watcher=new QFutureWatcher<QString>(this);
    connect(watcher,&QFutureWatcher<QString>::finished,this,[this,watcher,preview]{const auto error=watcher->result();watcher->deleteLater();ClearPreview();SetBusy(false);fUi->status->setText(error.isEmpty() ? tr("Saved %1. Open the converted XML explicitly in the configuration editor. Original configuration and hardware are unchanged.").arg(preview.destination) : error);});
    watcher->setFuture(QtConcurrent::run([preview]{QString error;XcfgBinary::Save(preview,&error);return error;}));
}
