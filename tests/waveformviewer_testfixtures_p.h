#ifndef GETDAQ_WAVEFORMVIEWER_TESTFIXTURES_P_H
#define GETDAQ_WAVEFORMVIEWER_TESTFIXTURES_P_H

#include <QByteArray>
#include <QString>

namespace GetDAQTestInternal {

void Put16BigEndian(QByteArray *bytes, int offset, quint16 value);
void Put24BigEndian(QByteArray *bytes, int offset, quint32 value);
void Put32BigEndian(QByteArray *bytes, int offset, quint32 value);
QByteArray MakeType1Frame();
QByteArray MakeType2Frame();
QByteArray MakePulseType1Frame();
bool WriteWaveformFixture(const QString &path, const QByteArray &bytes);

} // namespace GetDAQTestInternal

#endif
