#ifndef PLANTRANSITIONWINDOW_H
#define PLANTRANSITIONWINDOW_H
#include "eccprotocol.h"
#include <QDialog>
namespace Ui { class PlanTransitionWindow; }
// 只审阅控制元数据、提交明确意图和导出原收据；不启动远程进程或读取raw。
class PlanTransitionWindow : public QDialog
{
    Q_OBJECT
public:
    explicit PlanTransitionWindow(QWidget *parent=nullptr);
    ~PlanTransitionWindow() override;
    bool LoadReview(const QString &successorFile,const QString &targetsFile,QString *error=nullptr);
    bool SaveReceipt(const QString &file,QString *error=nullptr);
public slots:
    void SetControlState(bool canSubmit,bool canInspect,QByteArray status,QByteArray identity,QString reason);
    void ShowResult(QByteArray receipt,QByteArray request,QString error);
    void SetCompletionState(bool canComplete,bool canObserve,QByteArray originalRequest,QByteArray originalStatus,QByteArray completionRequest,QString completionPhase,QString reason);
signals:
    void SubmitSignal(QByteArray successorBytes,QByteArray targetsBytes);
    void InspectSignal();
    void CompleteSignal(QString originalOperationId);
    void InspectCompletionSignal();
private:
    Ui::PlanTransitionWindow *ui;
    GetDAQ::EccPlanTransitionReview fReview;
    GetDAQ::EccControlSystemStatus fOriginal;
    GetDAQ::EccControlIdentity fIdentity;
    QString fReviewedInstance,fSuccessorFile,fTargetsFile;
    QByteArray fReceipt;
    bool fCanSubmit=false,fCanInspect=false,fReviewed=false,fSubmitted=false;
    bool fCanComplete=false,fCanObserveCompletion=false,fCompletionPending=false;
    QByteArray fSavedOriginalRequest,fSavedOriginalStatus,fCompletionRequest,fCompletionClickedAgainst;
    void UpdateControls();
    void Submit();
    void Complete();
};
#endif
