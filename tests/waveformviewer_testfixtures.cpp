#include "waveformviewer_testfixtures_p.h"
#include <QFile>

namespace GetDAQTestInternal {

void Put16BigEndian(QByteArray *bytes, int offset, quint16 value)
{
    (*bytes)[offset] = char(value >> 8);
    (*bytes)[offset + 1] = char(value);
}

void Put24BigEndian(QByteArray *bytes, int offset, quint32 value)
{
    (*bytes)[offset] = char(value >> 16);
    (*bytes)[offset + 1] = char(value >> 8);
    (*bytes)[offset + 2] = char(value);
}

void Put32BigEndian(QByteArray *bytes, int offset, quint32 value)
{
    (*bytes)[offset] = char(value >> 24);
    (*bytes)[offset + 1] = char(value >> 16);
    (*bytes)[offset + 2] = char(value >> 8);
    (*bytes)[offset + 3] = char(value);
}

QByteArray MakeType1Frame()
{
    const int items = 2 * 512;
    const int headerBytes = 256;
    const int frameBytes = ((headerBytes + items * 4 + 255) / 256) * 256;
    QByteArray bytes(frameBytes, '\0');
    bytes[0] = char(8); Put24BigEndian(&bytes, 1, frameBytes / 256);
    Put16BigEndian(&bytes, 5, 1); bytes[7] = char(5);
    Put16BigEndian(&bytes, 8, 1); Put16BigEndian(&bytes, 10, 4);
    Put32BigEndian(&bytes, 12, items); Put32BigEndian(&bytes, 22, 1234);
    bytes[26] = char(2); bytes[27] = char(1);
    for (int i = 0; i < items; ++i) {
        const int channel = i < 512 ? 7 : 8;
        const int sample = i % 512;
        const quint32 word = (2u << 30) | (quint32(channel) << 23) |
            (quint32((sample + 100) % 512) << 14) | quint32(sample + (channel == 8 ? 1000 : 0));
        Put32BigEndian(&bytes, headerBytes + i * 4, word);
    }
    return bytes;
}

QByteArray MakeType2Frame()
{
    const int items = 4 * 68 * 512;
    const int headerBytes = 256;
    const int blockBytes = 256;
    const int frameBytes = ((headerBytes + items * 2 + blockBytes - 1) / blockBytes) * blockBytes;
    QByteArray bytes(frameBytes, '\0');
    bytes[0] = char(8); Put24BigEndian(&bytes, 1, frameBytes / blockBytes);
    Put16BigEndian(&bytes, 5, 2); bytes[7] = char(5);
    Put16BigEndian(&bytes, 8, 1); Put16BigEndian(&bytes, 10, 2);
    Put32BigEndian(&bytes, 12, items);
    int item = 0;
    for (int bucket = 0; bucket < 512; ++bucket)
        for (int channel = 0; channel < 68; ++channel)
            for (int aget = 0; aget < 4; ++aget) {
                const quint16 word = quint16((aget << 14) | ((aget * 100 + channel * 2 + bucket) & 0xfff));
                Put16BigEndian(&bytes, headerBytes + item * 2, word);
                ++item;
            }
    return bytes;
}

QByteArray MakePulseType1Frame()
{
    const int headerBytes = 256;
    const int items = 512;
    const int frameBytes = headerBytes + items * 4;
    QByteArray bytes(frameBytes, '\0');
    bytes[0] = char(8); Put24BigEndian(&bytes, 1, frameBytes / 256);
    Put16BigEndian(&bytes, 5, 1); bytes[7] = char(5);
    Put16BigEndian(&bytes, 8, 1); Put16BigEndian(&bytes, 10, 4);
    Put32BigEndian(&bytes, 12, items); Put32BigEndian(&bytes, 22, 42);
    bytes[26] = char(2); bytes[27] = char(1);
    for (int bucket = 0; bucket < items; ++bucket) {
        quint32 value = 0;
        if (bucket == 197 || bucket == 205) value = 10;
        if (bucket == 198 || bucket == 202) value = 50;
        if (bucket == 199 || bucket == 201) value = 80;
        if (bucket == 200) value = 100;
        const quint32 word = (2u << 30) | (7u << 23) | (quint32(bucket) << 14) | value;
        Put32BigEndian(&bytes, headerBytes + bucket * 4, word);
    }
    return bytes;
}

bool WriteWaveformFixture(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace GetDAQTestInternal
