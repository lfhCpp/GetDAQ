#ifndef GETDAQ_TEST_SUPPORT_P_H
#define GETDAQ_TEST_SUPPORT_P_H

#include <QString>
#include "runconfig.h"

class QSignalSpy;

namespace GetDAQTestSupport {

RunConfig Config(const QString &directory);
void AttachMockNodes(RunConfig &config);
bool At(const QSignalSpy &spy, int state);
QString Phase(const QSignalSpy &spy);
void MakeFiles(const RunConfig &config);

} // namespace GetDAQTestSupport

#endif
