#include "../apps/datareceiver/hitpattern.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

void PutInteger(std::string *frame, std::size_t offset, unsigned bytes,
                std::uint64_t value, bool little)
{
    for (unsigned i = 0; i < bytes; ++i) {
        const unsigned shiftIndex = little ? i : bytes - 1 - i;
        (*frame)[offset + i] = char((value >> (8 * shiftIndex)) & 0xffu);
    }
}

std::string Frame(bool little, unsigned cobo, unsigned asad,
                  const std::vector<unsigned> &channels, unsigned aget = 2,
                  unsigned type = 1, unsigned revision = 5)
{
    std::string frame(80, '\0');
    frame[0] = char(little ? 0x80 : 0x00);
    PutInteger(&frame, 1, 3, frame.size(), little);
    frame[4] = 1;
    PutInteger(&frame, 5, 2, type, little);
    frame[7] = char(revision);
    PutInteger(&frame, 8, 2, 80, little);
    PutInteger(&frame, 10, 2, 0, little);
    PutInteger(&frame, 12, 4, 0, little);
    frame[26] = char(cobo);
    frame[27] = char(asad);
    const std::size_t pattern = 31 + std::size_t(aget) * 9;
    for (unsigned channel : channels) {
        const unsigned byteInPattern = channel / 8;
        const std::size_t byteOffset = little ? pattern + byteInPattern : pattern + 8 - byteInPattern;
        frame[byteOffset] = char(static_cast<unsigned char>(frame[byteOffset]) |
                                 (1u << (channel % 8)));
    }
    return frame;
}

void CheckEndianAndSelection(bool little)
{
    std::string input = Frame(little, 2, 1, {0, 7, 8, 67});
    input += Frame(little, 2, 1, {0, 31});
    input += Frame(little, 2, 0, {2});
    input += Frame(little, 3, 1, {3});
    std::istringstream stream(input);
    const GetDAQ::HitPatternCounts result = GetDAQ::CountHitPatternFrames(stream, 2, 1, 2);
    Require(result.frames == 2, "matching CoBo/AsAd frames were not selected");
    Require(result.channelFrames[0] == 2, "channel 0 count differs");
    Require(result.channelFrames[7] == 1, "channel 7 count differs");
    Require(result.channelFrames[8] == 1, "channel 8 count differs");
    Require(result.channelFrames[31] == 1, "channel 31 count differs");
    Require(result.channelFrames[67] == 1, "channel 67 count differs");
    Require(result.channelFrames[1] == 0, "unset channel was counted");
}

template <typename Function>
void MustThrow(Function function, const char *message)
{
    bool threw = false;
    try { function(); } catch (const std::exception &) { threw = true; }
    Require(threw, message);
}

void CheckRejections()
{
    const std::string valid = Frame(true, 2, 1, {0});
    MustThrow([&] { std::istringstream in(valid); GetDAQ::CountHitPatternFrames(in, 2, 1, 4); },
              "invalid AGET identifier was accepted");
    MustThrow([&] { std::istringstream in(valid); GetDAQ::CountHitPatternFrames(in, 2, 2, 2); },
              "missing target AsAd was accepted");
    MustThrow([&] { std::istringstream in(valid.substr(0, 40)); GetDAQ::CountHitPatternFrames(in, 2, 1, 2); },
              "truncated frame was accepted");
    const std::string unsupported = Frame(true, 2, 1, {0}, 2, 2, 5);
    MustThrow([&] { std::istringstream in(unsupported); GetDAQ::CountHitPatternFrames(in, 2, 1, 2); },
              "unsupported CoBo encoding was accepted");
}

void CheckCountMerge()
{
    GetDAQ::HitPatternCounts total{};
    GetDAQ::HitPatternCounts first{};
    first.frames = 2;
    first.channelFrames[0] = 2;
    first.channelFrames[67] = 1;
    GetDAQ::MergeHitPatternCounts(&total, first);
    GetDAQ::HitPatternCounts second{};
    second.frames = 3;
    second.channelFrames[0] = 1;
    second.channelFrames[8] = 3;
    GetDAQ::MergeHitPatternCounts(&total, second);
    Require(total.frames == 5 && total.channelFrames[0] == 3 &&
            total.channelFrames[8] == 3 && total.channelFrames[67] == 1,
            "independent file counts were not combined exactly");

    total.frames = UINT64_MAX;
    MustThrow([&] { GetDAQ::MergeHitPatternCounts(&total, second); },
              "frame counter overflow was accepted");
    total.frames = 1;
    total.channelFrames[8] = UINT64_MAX;
    MustThrow([&] { GetDAQ::MergeHitPatternCounts(&total, second); },
              "channel counter overflow was accepted");
    MustThrow([&] { GetDAQ::MergeHitPatternCounts(nullptr, second); },
              "null accumulation target was accepted");
}
}

int main()
{
    try {
        CheckEndianAndSelection(true);
        CheckEndianAndSelection(false);
        CheckRejections();
        CheckCountMerge();
        std::cout << "PASS bounded CoBo hit-pattern streaming, endian order, target filters, malformed-input rejection, and overflow-safe aggregation\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
