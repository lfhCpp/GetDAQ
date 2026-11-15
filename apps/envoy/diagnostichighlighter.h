#ifndef GETDAQ_DIAGNOSTIC_HIGHLIGHTER_H
#define GETDAQ_DIAGNOSTIC_HIGHLIGHTER_H
#include "diagnostics.h"
#include <QSyntaxHighlighter>
#include <QTextBlockUserData>
struct DiagnosticBlockData : QTextBlockUserData {
    explicit DiagnosticBlockData(int value):severity(value){}
    int severity;
};
class DiagnosticHighlighter : public QSyntaxHighlighter {
public:
    explicit DiagnosticHighlighter(QTextDocument *doc):QSyntaxHighlighter(doc){}
protected:
    void highlightBlock(const QString &text) override;
};
#endif
