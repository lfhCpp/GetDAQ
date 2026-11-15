#include "diagnostichighlighter.h"
#include <QTextBlock>
void DiagnosticHighlighter::highlightBlock(const QString &text) {
    const auto *data=dynamic_cast<DiagnosticBlockData *>(currentBlockUserData());
    const int severity=data ? data->severity : Diagnostics::Classify(text.mid(24));
    const QColor color=severity==Diagnostics::Error ? QColor("#c74349") : severity==Diagnostics::Warning ? QColor("#b86b00") : QColor("#213547");
    setFormat(0,text.size(),color);
}
