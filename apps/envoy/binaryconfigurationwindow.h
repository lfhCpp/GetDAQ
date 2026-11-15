#ifndef GETDAQ_BINARYCONFIGURATIONWINDOW_H
#define GETDAQ_BINARYCONFIGURATIONWINDOW_H
#include "xcfgbinary.h"
#include <QDialog>
#include <memory>
namespace Ui { class BinaryConfigurationWindow; }
class BinaryConfigurationWindow : public QDialog {
    Q_OBJECT
public:
    explicit BinaryConfigurationWindow(const XcfgDocument &document,const QString &directory,QWidget *parent=nullptr);
    ~BinaryConfigurationWindow();
private:
    void ClearPreview();
    void SetBusy(bool busy);
    void Preview();
    void Save();
    std::unique_ptr<Ui::BinaryConfigurationWindow> fUi;
    XcfgDocument fDocument;
    XcfgBinary::Preview fPreview;
    QString fDirectory;
    bool fBusy=false,fReviewed=false;
};
#endif
