#include "rootstatistics.h"
#include "roottreereader.h"
#include "GDataFrame.h"

#include <TFile.h>
#include <TROOT.h>
#include <TTree.h>

#include <atomic>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void Check(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

void RemoveTree(const std::string &path)
{
    DIR *directory = ::opendir(path.c_str());
    if (!directory) { ::unlink(path.c_str()); return; }
    while (dirent *entry = ::readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string child = path + '/' + name;
        struct stat info;
        if (!::lstat(child.c_str(), &info) && S_ISDIR(info.st_mode)) RemoveTree(child);
        else ::unlink(child.c_str());
    }
    ::closedir(directory);
    ::rmdir(path.c_str());
}

void CreateFixture(const std::string &path)
{
    TFile file(path.c_str(), "RECREATE");
    Check(!file.IsZombie(), "cannot create ROOT statistics fixture");
    TTree tree("tree", "three legacy events");
    GET::GDataFrame frame;
    Check(tree.Branch("GDataFrame", &frame) != nullptr, "cannot create fixture branch");

    frame.fHeader.fEventIdx = 0;
    frame.fHeader.fEventTime = 100000000;
    frame.AddSample(0, 0, 10, 100);
    frame.AddSample(0, 1, 11, 101);
    frame.AddSample(3, 67, 511, 4095);
    Check(tree.Fill() >= 0, "cannot fill first fixture event");

    frame.Clear();
    frame.fHeader.fEventIdx = 1;
    frame.fHeader.fEventTime = 150000000;
    frame.AddSample(2, 8, 35, 200);
    Check(tree.Fill() >= 0, "cannot fill second fixture event");

    frame.Clear();
    frame.fHeader.fEventIdx = 2;
    frame.fHeader.fEventTime = 150000000;
    Check(tree.Fill() >= 0, "cannot fill third fixture event");
    file.cd();
    Check(tree.Write() > 0, "cannot write ROOT statistics fixture");
    file.Close();
}

unsigned long long Sum(const GetDAQ::RootStatistics::Histogram &histogram)
{
    unsigned long long result = 0;
    for (std::size_t i = 0; i < histogram.bins.size(); ++i) result += histogram.bins[i];
    return result;
}

void CheckScan(const std::string &path, GetDAQ::RootStatistics::Measure measure,
               std::uint64_t expectedValues, const char *message)
{
    std::atomic<bool> cancelled(false);
    std::atomic<int> progress(0);
    GetDAQ::RootStatistics::Histogram histogram;
    std::string error;
    Check(GetDAQ::RootStatistics::ScanFile(path, measure, &cancelled, &progress, &histogram, &error),
          error.empty() ? message : error.c_str());
    Check(histogram.values == expectedValues && Sum(histogram) == expectedValues, message);
    Check(progress.load() == 1000, "statistics progress did not reach completion");
}
}

int main()
{
    ROOT::EnableThreadSafety();
    const char *temp = std::getenv("TMPDIR");
    if (!temp || !*temp || ::access(temp, W_OK | X_OK)) temp = "/tmp";
    std::string writable = std::string(temp) + "/getdaq-root-stat-test-XXXXXX";
    writable.push_back('\0');
    if (!::mkdtemp(&writable[0])) { std::cerr << "cannot create ROOT statistics test directory\n"; return 1; }
    writable.resize(writable.find('\0'));
    try {
        const std::string path = writable + "/events.root";
        CreateFixture(path);
        CheckScan(path, GetDAQ::RootStatistics::Measure::EventTime, 3, "event time scan differs");
        CheckScan(path, GetDAQ::RootStatistics::Measure::EventFrequency, 2, "frequency scan differs");
        CheckScan(path, GetDAQ::RootStatistics::Measure::EventPeriod, 2, "period scan differs");

        GetDAQ::RootStatistics::Histogram threadedHistogram;
        std::atomic<bool> threadCancelled(false);
        std::atomic<int> threadProgress(0);
        std::string threadError;
        bool threadSucceeded = false;
        std::thread scanner([&]() {
            threadSucceeded = GetDAQ::RootStatistics::ScanFile(
                path, GetDAQ::RootStatistics::Measure::EventFrequency, &threadCancelled,
                &threadProgress, &threadedHistogram, &threadError);
        });
        scanner.join();
        Check(threadSucceeded && threadedHistogram.values == 2 && threadProgress.load() == 1000,
              threadError.empty() ? "background ROOT statistics scan failed" : threadError.c_str());

        GetDAQ::RootStatistics::Histogram buckets;
        std::atomic<bool> cancelled(false);
        std::atomic<int> progress(0);
        std::string error;
        Check(GetDAQ::RootStatistics::ScanFile(path, GetDAQ::RootStatistics::Measure::Bucket,
                                               &cancelled, &progress, &buckets, &error), error.c_str());
        Check(buckets.bins.size() == 512 && Sum(buckets) == 4 && buckets.bins[10] == 1 &&
              buckets.bins[11] == 1 && buckets.bins[35] == 1 && buckets.bins[511] == 1,
              "bucket distribution scan differs");

        GetDAQ::RootStatistics::Histogram channels;
        progress.store(0);
        Check(GetDAQ::RootStatistics::ScanFile(path, GetDAQ::RootStatistics::Measure::ChannelCount,
                                               &cancelled, &progress, &channels, &error), error.c_str());
        Check(channels.bins.size() == 273 && Sum(channels) == 3 && channels.bins[3] == 1 &&
              channels.bins[1] == 1 && channels.bins[0] == 1,
              "channel count distribution scan differs");

        cancelled.store(true);
        progress.store(0);
        Check(!GetDAQ::RootStatistics::ScanFile(path, GetDAQ::RootStatistics::Measure::EventTime,
                                                &cancelled, &progress, &channels, &error) && error.empty() &&
              progress.load() == 0, "pre-cancelled ROOT statistics scan did not stop cleanly");
        RemoveTree(writable);
        std::cout << "PASS ROOT statistics scans five legacy fields with bounded state and cancellation; temporary ROOT fixture removed\n";
        return 0;
    } catch (const std::exception &exception) {
        RemoveTree(writable);
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
