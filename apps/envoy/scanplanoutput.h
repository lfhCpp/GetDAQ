#ifndef GETDAQ_SCANPLANOUTPUT_H
#define GETDAQ_SCANPLANOUTPUT_H

#include <QString>

struct ScanPlanOutputSummary
{
    QString kind;
    QString targetCoBo;
    int pointCount = 0;
    int presentFileCount = 0;
    QString planSha256;
    QString error;

    bool isValid() const { return error.isEmpty(); }
};

// Checks the manifest, complete scan grid, point artifacts, digests, and that
// each artifact's actual settings agree with the declared scan point. This is
// offline integrity validation; it does not prove manifest provenance or that
// any setting has been loaded into hardware.
ScanPlanOutputSummary InspectScanPlanOutput(const QString &directory);

#endif
