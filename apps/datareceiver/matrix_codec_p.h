#ifndef GETDAQ_MATRIX_CODEC_P_H
#define GETDAQ_MATRIX_CODEC_P_H

#include "mfmstream.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GetDAQ {
namespace MatrixInternal {
std::size_t SampleCount();
std::size_t MatrixBytes();
std::size_t MaximumFrameBytes();
void DecodeMatrix(const std::uint8_t *frame, const FrameInfo &info,
                  std::vector<std::uint16_t> &samples);
void EncodeMatrix(const std::vector<std::uint16_t> &samples,
                  std::vector<std::uint8_t> &bytes);
}
}

#endif
