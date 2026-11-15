#ifndef GETDAQ_SENTRY_SERVICE_TESTS_P_H
#define GETDAQ_SENTRY_SERVICE_TESTS_P_H

namespace GetDAQTestInternal {
void RunSentryCatalogFiles();
void RunSentryCatalogExplicitReplacementDefersOldDataCleanup();
void RunSentryCatalogReplacementRejectsIncompleteOldRun();
void RunSentryReplacementResumesAfterMergedManifestBeforeBackupCleanup();
void RunSentryConfigStrictFields();
void RunSentryHttpEndpoints();
 }

#endif
