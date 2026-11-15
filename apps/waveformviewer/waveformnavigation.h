#ifndef GETDAQ_WAVEFORMNAVIGATION_H
#define GETDAQ_WAVEFORMNAVIGATION_H

namespace GetDAQ {

inline long long NextWaveformFrameIndex(long long current, long long frameCount)
{
    if (frameCount <= 0 || current < 0 || current >= frameCount) return -1;
    return current + 1 == frameCount ? 0 : current + 1;
}

inline long long PreviousWaveformFrameIndex(long long current, long long frameCount)
{
    if (frameCount <= 0 || current < 0 || current >= frameCount) return -1;
    return current == 0 ? frameCount - 1 : current - 1;
}

} // namespace GetDAQ

#endif // GETDAQ_WAVEFORMNAVIGATION_H
