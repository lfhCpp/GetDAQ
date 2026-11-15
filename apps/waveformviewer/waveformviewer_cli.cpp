#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "waveformscananalysis.h"

namespace {

int ParseDecimal(const char *text, int minimum, int maximum, const char *name)
{
    if (!text || !*text) throw std::invalid_argument(std::string(name) + " must be decimal");
    errno = 0;
    char *end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (errno || !end || *end || value < minimum || value > maximum || std::to_string(value) != text)
        throw std::invalid_argument(std::string(name) + " is outside its decimal range");
    return int(value);
}

int AnalyzePeakingScanCommand(int argc, char **argv)
{
    try {
        if (argc < 8 || (argc - 6) % 2 != 0)
            throw std::invalid_argument("Provide COBO ASAD AGET CHANNEL and one or more POINT_INDEX FILE pairs");
        const int cobo = ParseDecimal(argv[2], 0, 255, "CoBo ID");
        const int asad = ParseDecimal(argv[3], 0, 3, "AsAd ID");
        const int aget = ParseDecimal(argv[4], 0, 3, "AGET ID");
        const int channel = ParseDecimal(argv[5], 0, 67, "channel");
        QVector<GetDAQ::PeakingScanInput> inputs;
        for (int i = 6; i < argc; i += 2) {
            GetDAQ::PeakingScanInput input;
            input.pointIndex = ParseDecimal(argv[i], 0, 15, "peaking-time point index");
            input.fileName = QString::fromLocal8Bit(argv[i + 1]);
            inputs.append(input);
        }
        QString error;
        const QVector<GetDAQ::PeakingScanSummary> summaries =
            GetDAQ::AnalyzePeakingScan(inputs, cobo, asad, aget, channel, &error);
        if (!error.isEmpty()) throw std::runtime_error(error.toLocal8Bit().constData());
        std::cout << "# kind=getdaq.peaking-time-waveform-analysis\n"
                  << "# mapping=operator-supplied; scan plan and run/catalog binding were not verified\n"
                  << "# target=cobo:" << cobo << ",asad:" << asad << ",aget:" << aget << ",channel:" << channel << "\n"
                  << "point_index,frames_seen,channel_present,channel_missing,valid_waveforms,invalid_waveforms,mean_peak_adc,mean_peaking_buckets,mean_fwhm_buckets\n";
        for (const GetDAQ::PeakingScanSummary &summary : summaries) {
            std::cout << summary.pointIndex << ',' << summary.framesSeen << ',' << summary.channelPresent << ','
                      << summary.channelMissing << ',' << summary.validWaveforms << ',' << summary.invalidWaveforms << ',';
            if (summary.validWaveforms == 0) {
                std::cout << ",,\n";
            } else {
                std::cout << std::fixed << std::setprecision(6) << summary.meanPeakAdc << ','
                          << summary.meanPeakingBuckets << ',' << summary.meanFwhmBuckets << '\n';
            }
        }
        if (!std::cout) throw std::runtime_error("Cannot write peaking-time analysis summary");
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "Cannot analyze peaking-time scan: " << exception.what() << '\n';
        return 2;
    }
}

int AnalyzePulserResponseCommand(int argc, char **argv)
{
    try {
        if (argc < 8 || (argc - 5) % 3 != 0)
            throw std::invalid_argument("Provide COBO ASAD AGET and one or more CHANNEL PULSER_MV FILE groups");
        const int cobo = ParseDecimal(argv[2], 0, 255, "CoBo ID");
        const int asad = ParseDecimal(argv[3], 0, 3, "AsAd ID");
        const int aget = ParseDecimal(argv[4], 0, 3, "AGET ID");
        QVector<GetDAQ::PulserResponseInput> inputs;
        for (int i = 5; i < argc; i += 3) {
            GetDAQ::PulserResponseInput input;
            input.channel = ParseDecimal(argv[i], 0, 67, "channel");
            input.pulserMillivolts = ParseDecimal(argv[i + 1], std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max(), "Pulser amplitude in mV");
            input.fileName = QString::fromLocal8Bit(argv[i + 2]);
            inputs.append(input);
        }
        QString error;
        const QVector<GetDAQ::PulserResponseSummary> summaries =
            GetDAQ::AnalyzePulserResponse(inputs, cobo, asad, aget, &error);
        if (!error.isEmpty()) throw std::runtime_error(error.toLocal8Bit().constData());
        std::cout << "# kind=getdaq.pulser-waveform-response\n"
                  << "# mapping=operator-supplied; gain formula, scan plan and run/catalog binding were not verified\n"
                  << "# target=cobo:" << cobo << ",asad:" << asad << ",aget:" << aget << "\n"
                  << "channel,pulser_millivolts,frames_seen,channel_present,channel_missing,valid_waveforms,invalid_waveforms,mean_peak_adc,mean_peaking_buckets,mean_fwhm_buckets\n";
        for (const GetDAQ::PulserResponseSummary &summary : summaries) {
            std::cout << summary.channel << ',' << summary.pulserMillivolts << ',' << summary.framesSeen << ','
                      << summary.channelPresent << ',' << summary.channelMissing << ',' << summary.validWaveforms << ','
                      << summary.invalidWaveforms << ',';
            if (summary.validWaveforms == 0) {
                std::cout << ",,\n";
            } else {
                std::cout << std::fixed << std::setprecision(6) << summary.meanPeakAdc << ','
                          << summary.meanPeakingBuckets << ',' << summary.meanFwhmBuckets << '\n';
            }
        }
        if (!std::cout) throw std::runtime_error("Cannot write pulser-response summary");
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "Cannot analyze pulser response: " << exception.what() << '\n';
        return 2;
    }
}

}

bool RunWaveformViewerCommandLine(int argc, char **argv, int *exitCode)
{
    if (!exitCode) return false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "GetDAQ Waveform Viewer\n"
                         "Usage: getdaq-waveform-viewer [--help]\n"
                         "       getdaq-waveform-viewer --analyze-peaking-scan COBO ASAD AGET CHANNEL POINT_INDEX FILE [POINT_INDEX FILE ...]\n"
                         "       getdaq-waveform-viewer --analyze-pulser-response COBO ASAD AGET CHANNEL PULSER_MV FILE [CHANNEL PULSER_MV FILE ...]\n"
                         "Open a .graw or .gmat file from the window to inspect selected AGET channels.\n"
                         "The scan command reports legacy discrete waveform metrics for an operator-supplied file mapping.\n"
                         "The pulser-response command reports offline waveform summaries, not gain or pass/fail decisions.\n"
                         "Use the 3D surface checkbox to view bucket × channel × ADC; drag the plot to rotate.\n";
            *exitCode = 0;
            return true;
        }
        if (std::strcmp(argv[i], "--analyze-peaking-scan") == 0) {
            *exitCode = AnalyzePeakingScanCommand(argc, argv);
            return true;
        }
        if (std::strcmp(argv[i], "--analyze-pulser-response") == 0) {
            *exitCode = AnalyzePulserResponseCommand(argc, argv);
            return true;
        }
    }
    return false;
}
