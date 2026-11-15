#ifndef GETDAQ_XCFGBINARY_CODEC_P_H
#define GETDAQ_XCFGBINARY_CODEC_P_H
#include <QByteArray>

namespace XcfgBinaryCodec {
QByteArray Decode(const QByteArray &binary);
QByteArray Encode(const QByteArray &xml);
}
#endif
