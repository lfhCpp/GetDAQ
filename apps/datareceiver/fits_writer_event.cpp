#include "fits_writer_p.h"
#include "fits_writer_support_p.h"
#include <stdexcept>
#include <vector>

namespace GetDAQ {
namespace FitsInternal {
namespace {
const std::size_t kMaximumIndexRows=2000000u;
}
void WriteEventFile(const std::string &path, const std::string &indexName,
                    const EventData &event, std::size_t &indexRows,
                    std::vector<FitsIndexRow> &rows) {
    if (event.channels.empty()) throw std::runtime_error("CoBo event has no waveform samples");
    FitsFile output(path);
    Primary(output.Get());
    unsigned version = 0;
    for (const auto &entry : event.channels) {
        ++version;
        const auto &samples = entry.second;
        if (samples.values.empty() || samples.values.size() != samples.buckets.size())
            throw std::runtime_error("Invalid channel sample arrays");
        if (indexRows >= kMaximumIndexRows)
            throw std::runtime_error("FITS event index row limit reached");
        char *names[] = {const_cast<char *>("BUCKET"), const_cast<char *>("SAMPLE")};
        char *forms[] = {const_cast<char *>("1U"), const_cast<char *>("1U")};
        char *units[] = {const_cast<char *>(""), const_cast<char *>("count")};
        int status = 0;
        fits_create_tbl(output.Get(), BINARY_TBL, 0, 2, names, forms, units,
                        const_cast<char *>("GET-EVTS-CHAN"), &status);
        CheckFits(status, "create FITS channel table");
        KeyInteger(output.Get(), TUINT, "EXTVER", version);
        KeyInteger(output.Get(), TUINT, "EVENT", event.info.event, "Event identifier");
        const unsigned short cobo = static_cast<unsigned short>(event.info.cobo);
        const unsigned short asad = static_cast<unsigned short>(event.info.asad);
        KeyInteger(output.Get(), TUSHORT, "COBO", cobo, "CoBo board identifier");
        KeyInteger(output.Get(), TUSHORT, "ASAD", asad, "AsAd board index");
        KeyInteger(output.Get(), TUSHORT, "AGET", static_cast<unsigned short>(entry.first.second), "AGET chip index");
        KeyInteger(output.Get(), TUSHORT, "CHAN", static_cast<unsigned short>(entry.first.first), "AGET channel index");
        const LONGLONG count = static_cast<LONGLONG>(samples.values.size());
        fits_write_col(output.Get(), TUSHORT, 1, 1, 1, count,
                       const_cast<unsigned short *>(samples.buckets.data()), &status);
        CheckFits(status, "write FITS bucket values");
        fits_write_col(output.Get(), TUSHORT, 2, 1, 1, count,
                       const_cast<unsigned short *>(samples.values.data()), &status);
        CheckFits(status, "write FITS ADC samples");
        KeyInteger(output.Get(), TINT, "GRPID1", -1, "EXTVER of the grouping table");
        KeyString(output.Get(), "GRPLC1", indexName, "URI of the grouping table");
        fits_write_chksum(output.Get(), &status);
        CheckFits(status, "write FITS channel checksum");
        FitsIndexRow row;
        row.event = event.info.event;
        row.cobo = cobo;
        row.asad = asad;
        row.aget = static_cast<unsigned short>(entry.first.second);
        row.channel = static_cast<unsigned short>(entry.first.first);
        row.version = version;
        row.position = version;
        row.location = Leaf(path);
        rows.push_back(row);
        ++indexRows;
    }
    int status = 0;
    int hduType = 0;
    fits_movabs_hdu(output.Get(), 1, &hduType, &status);
    CheckFits(status, "select FITS primary HDU");
    fits_write_chksum(output.Get(), &status);
    CheckFits(status, "write FITS primary checksum");
    output.Close();
}

} // namespace FitsInternal
} // namespace GetDAQ
