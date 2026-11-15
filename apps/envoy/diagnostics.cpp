#include "diagnostics.h"
#include <QRegularExpression>
namespace Diagnostics {
Severity Classify(const QString &message) {
    static const QRegularExpression uncertainAction("(write outcome\\s+UNKNOWN|Hardware plan (stopped|expectation mismatch)|native state/unknown result)",QRegularExpression::CaseInsensitiveOption);
    if(message.contains(uncertainAction))return Error;
    static const QRegularExpression error("(^|\\b)(error|failed|failure|fault|cannot|invalid|refused|denied|differs|exceeded|lost)(\\b|:)",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression warning("(^|\\b)(warn(ing)?|unavailable|unknown|stale|requires?|canceled|incomplete|preserv(e|ed)|fallback)(\\b|:)",QRegularExpression::CaseInsensitiveOption);
    // Legacy text has no explicit level. Recognize successful summaries before
    // vocabulary matching; newly generated diagnostics use explicit severity.
    if(message.contains(QRegularExpression("^(No errors|0 errors|Recovered incomplete journal|Run already complete|New Native Run is complete)",QRegularExpression::CaseInsensitiveOption)))return Information;
    if(message.contains(error))return Error;
    if(message.contains(warning))return Warning;
    return Information;
}
}
