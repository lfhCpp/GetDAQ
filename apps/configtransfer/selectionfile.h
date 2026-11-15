#ifndef GETDAQ_CONFIGTRANSFER_SELECTIONFILE_H
#define GETDAQ_CONFIGTRANSFER_SELECTIONFILE_H

#include <QByteArray>
#include <QString>

bool PublishNewSelectionFile(const QString &path, const QByteArray &bytes,
                             QString *error = nullptr);

#endif
