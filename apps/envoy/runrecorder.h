#ifndef RUNRECORDER_H
#define RUNRECORDER_H
#include "runconfig.h"
#include <QByteArray>
#include <QJsonObject>

// 运行记录按Run分目录；默认保留失败现场并拒绝覆盖。覆盖必须由独立的显式授权路径调用。
namespace RunRecorder {
bool Reserve(const RunConfig &config, QString *error);
// replaceCompletedRun只能在操作员明确确认同号覆盖后为true。
// 允许保留非Complete旧目录，明确标记retain，不能自动清理旧记录/旧数据。
bool Reserve(const RunConfig &config, bool replaceCompletedRun, QString *error);
bool WriteJournal(const RunConfig &config, const QJsonObject &journal, QString *error);
bool ReadJournal(const RunConfig &config, QJsonObject *journal, QString *error);
QString Finish(const RunConfig &config, double seconds); // 空字符串表示成功
// 调用方必须先确认全体数据节点已完成新Run catalog；失败时保留旧Run副本。
bool FinalizeReplacement(const RunConfig &config, QString *error);
}
#endif
