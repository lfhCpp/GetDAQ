#ifndef GETDAQ_DIAGNOSTICS_H
#define GETDAQ_DIAGNOSTICS_H
#include <QString>
namespace Diagnostics {
enum Severity { Information, Warning, Error };
Severity Classify(const QString &message);
}
#endif
