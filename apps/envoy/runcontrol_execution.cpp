#include "runcontrol.h"
#include "diagnostics.h"
#include <QTimer>

void RunControl::Begin(const QString &operation, const QVector<Step> &plan)
{
    ++fSequence;
    fOperation = operation; fPlan = plan; fStep = -1; fBusy = true;
    // 丢弃只读轮询；不会撤销已发出的硬件命令。
    for (auto ecc : fEcc) ecc->Cancel();
    NextStep();
}

void RunControl::NextStep()
{
    if (fShuttingDown || !fBusy) return;
    fAwaitAck.clear(); fAwaitState.clear();
    ++fStep;
    if (fStep >= fPlan.size()) { FinishPlan(); return; }
    const Step step = fPlan[fStep];
    for (auto ecc : fEcc) ecc->Cancel(); // 清理其他节点尚未完成的只读轮询。
    fPhase = fOperation + ": " + step.operation;
    if (fActive && !Journal(fPhase)) { Fail("Cannot persist transition; no new command sent."); return; }
    fDeadline->start(fConfig.stepTimeoutMs);
    for (int id : step.ids) {
        // 恢复部分启动时，已 Ready 的节点不再发送 Stop。
        if (step.operation == "Stop" && AllAt({id}, 4)) continue;
        fAwaitAck.insert(id);
        fAwaitState.insert(id);
    }
    const auto pending = fAwaitAck;
    for (int id : pending) {
        emit LogSignal(QString("Node %1 -> %2").arg(id).arg(step.operation));
        if (!fEcc[id]->Send(step.operation)) { Fail("Node busy while submitting transition."); return; }
    }
    Publish();
    if (fAwaitState.isEmpty()) {
        const int sequence = fSequence;
        QTimer::singleShot(0, this, [this, sequence]() { if (sequence == fSequence) NextStep(); });
    }
}

void RunControl::EccReplySlot(int id, QString operation, int state, int transition, QString error)
{
    if (!fConnected || fShuttingDown || !fStatus.contains(id)) return;
    NodeStatus &s = fStatus[id];
    if (operation == "GetState") {
        s.eccOnline = error.isEmpty(); s.error = error;
        if (error.isEmpty()) { s.state = state; s.transition = transition; s.eccTime = fClock.elapsed(); }
        if (fBusy && fStep >= 0 && fStep < fPlan.size() && !fAwaitAck.contains(id) && fAwaitState.contains(id)
                && error.isEmpty() && transition == 0 && state == fPlan[fStep].target) {
            fAwaitState.remove(id);
            if (fAwaitState.isEmpty()) NextStep();
        }
    } else if (fAwaitAck.contains(id)) {
        if (!error.isEmpty()) { s.error = error; s.eccOnline = false; Fail(QString("Node %1 %2: %3").arg(id).arg(operation, error)); }
        else {
            fAwaitAck.remove(id);
            // 必须取得命令应答后的 GetState，不使用命令之前的状态。
            s.eccOnline = false;
            fEcc[id]->Send("GetState");
        }
    }
    Publish();
}

void RunControl::FinishPlan()
{
    fDeadline->stop(); fStep = -1;
    for(const auto &step:fPlan)if(step.operation=="Configure" && !fCandidateConfigurationSnapshot.isEmpty()){fConfig.configurationSnapshot=fCandidateConfigurationSnapshot;fConfig.configurationBackupWarning=fCandidateBackupWarning;emit ConfigurationSnapshotSignal(fConfig.configurationSnapshot);}
    if (fOperation == "Start") {
        fPhase = "Running"; fBusy = false;
        if (!Journal(fPhase)) { Fail("Running, but journal could not be saved. Stop the run."); return; }
        fRunClock.restart();
        emit LogSignal("All configured nodes confirmed Running.");
    } else if (fOperation == "Stop") {
        emit LogSignal("All configured nodes confirmed stopped (Ready). Finalizing run records.");
        if (fRunClock.isValid()) fDuration += fRunClock.elapsed() / 1000.0;
        fRunClock.invalidate();
        fJournal["duration_seconds"] = fDuration;
        if (fActive && !fJournal.value("untracked").toBool()) {
            fPhase = "Settling";
            if (!Journal(fPhase)) { Fail("Stopped but journal could not be saved."); return; }
            const int sequence = fSequence;
            QTimer::singleShot(fConfig.settleMs, this, [this, sequence]() { if (sequence == fSequence && !fShuttingDown) StartCatalog(); });
        } else { fActive=false;fBusy = false; fPhase = "Stopped (untracked run)";emit LogSignal("Observed acquisition stopped. No local Run was reserved; no catalog or completion record was fabricated."); }
    } else if (fOperation == "Pause") {
        if (fRunClock.isValid()) fDuration += fRunClock.elapsed() / 1000.0;
        fRunClock.invalidate();
        if (!Journal("Paused")) { Fail("Hardware paused, but the run journal could not be saved."); return; }
        fBusy = false; fPhase = "Paused";
        emit LogSignal("All configured nodes confirmed Paused.");
    } else if (fOperation == "Resume") {
        fRunClock.start();
        if (!Journal("Running")) { Fail("Hardware resumed, but the run journal could not be saved."); return; }
        fBusy = false; fPhase = "Running";
        emit LogSignal("All configured nodes confirmed Running.");
    } else { fBusy = false; fPhase = "Monitoring"; }
    Publish();
}

void RunControl::Fail(const QString &error)
{
    CancelAutoRun(error);
    ++fSequence;
    fDeadline->stop();
    fBusy = false; fStep = -1; fPhase = "Error";
    fAwaitAck.clear(); fAwaitState.clear(); fAwaitCatalog.clear();
    // 网络命令可能已在远端生效；取消本地等待后继续轮询实际状态。
    for (auto ecc : fEcc) ecc->Cancel();
    for (auto sentry : fSentry) sentry->Cancel();
    if (fActive) { fJournal["error"] = error; Journal(fPhase); }
    ReportLog(error,Diagnostics::Error);
    Publish();
}
