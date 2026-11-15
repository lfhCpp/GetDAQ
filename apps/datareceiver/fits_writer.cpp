#include "fits_writer_p.h"
#include "fits_writer_support_p.h"
#include <fitsio.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace GetDAQ {
namespace FitsInternal {
namespace {
void IndexColumns(fitsfile *file) {
    char *names[] = {const_cast<char *>("EVENT"), const_cast<char *>("COBO"),
        const_cast<char *>("ASAD"), const_cast<char *>("AGET"), const_cast<char *>("CHAN"),
        const_cast<char *>("MEMBER_XTENSION"), const_cast<char *>("MEMBER_NAME"),
        const_cast<char *>("MEMBER_VERSION"), const_cast<char *>("MEMBER_POSITION"),
        const_cast<char *>("MEMBER_URI_TYPE"), const_cast<char *>("MEMBER_LOCATION")};
    char *forms[] = {const_cast<char *>("1V"), const_cast<char *>("1U"),
        const_cast<char *>("1U"), const_cast<char *>("1U"), const_cast<char *>("1U"),
        const_cast<char *>("8A"), const_cast<char *>("32A"), const_cast<char *>("1J"),
        const_cast<char *>("1J"), const_cast<char *>("3A"), const_cast<char *>("256A")};
    char *units[] = {const_cast<char *>(""), const_cast<char *>(""), const_cast<char *>(""),
        const_cast<char *>(""), const_cast<char *>(""), const_cast<char *>(""),
        const_cast<char *>(""), const_cast<char *>(""), const_cast<char *>(""),
        const_cast<char *>(""), const_cast<char *>("")};
    int status = 0;
    fits_create_tbl(file, BINARY_TBL, 0, 11, names, forms, units,
                    const_cast<char *>("GROUPING"), &status);
    CheckFits(status, "create FITS event index table");
    KeyString(file, "GRPNAME", "GET-EVTS-CHAN-IDX", "Name of the group table");
}

void WriteString(fitsfile *file, int column, LONGLONG row, const std::string &value) {
    char *text = const_cast<char *>(value.c_str());
    char *values[] = {text};
    int status = 0;
    fits_write_col(file, TSTRING, column, row, 1, 1, values, &status);
    CheckFits(status, "write FITS index string");
}

void WriteIndexRow(fitsfile *file, LONGLONG row, const FitsIndexRow &entry) {
    int status = 0;
    fits_write_col(file, TUINT, 1, row, 1, 1, const_cast<unsigned int *>(&entry.event), &status);
    CheckFits(status, "write event index number");
    fits_write_col(file, TUSHORT, 2, row, 1, 1, const_cast<unsigned short *>(&entry.cobo), &status);
    CheckFits(status, "write CoBo index");
    fits_write_col(file, TUSHORT, 3, row, 1, 1, const_cast<unsigned short *>(&entry.asad), &status);
    CheckFits(status, "write AsAd index");
    fits_write_col(file, TUSHORT, 4, row, 1, 1, const_cast<unsigned short *>(&entry.aget), &status);
    CheckFits(status, "write AGET index");
    fits_write_col(file, TUSHORT, 5, row, 1, 1, const_cast<unsigned short *>(&entry.channel), &status);
    CheckFits(status, "write channel index");
    WriteString(file, 6, row, "BINTABLE");
    WriteString(file, 7, row, "GET-EVTS-CHAN");
    fits_write_col(file, TUINT, 8, row, 1, 1, const_cast<unsigned int *>(&entry.version), &status);
    CheckFits(status, "write FITS extension version");
    fits_write_col(file, TUINT, 9, row, 1, 1, const_cast<unsigned int *>(&entry.position), &status);
    CheckFits(status, "write FITS member position");
    WriteString(file, 10, row, "URL");
    WriteString(file, 11, row, entry.location);
}


} // namespace

class Writer::Impl {
public:
    Impl(const std::string &stagingDirectory, const std::string &baseName)
        : indexName(baseName + "_index.fits.gz"),
          indexPath(stagingDirectory + '/' + indexName), index(indexPath),
          indexRows(0), finished(false) {
        Primary(index.Get());
        IndexColumns(index.Get());
    }

    std::string indexName;
    std::string indexPath;
    FitsFile index;
    std::size_t indexRows;
    bool finished;
};

Writer::Writer(const std::string &stagingDirectory, const std::string &baseName)
    : fImpl(new Impl(stagingDirectory, baseName)) {}

Writer::~Writer() = default;

const std::string &Writer::IndexPath() const { return fImpl->indexPath; }
std::size_t Writer::IndexRows() const { return fImpl->indexRows; }

void Writer::WriteEvent(const std::string &path, const EventData &event) {
    if (fImpl->finished) throw std::logic_error("cannot write to a finished FITS conversion");
    std::vector<FitsIndexRow> rows;
    rows.reserve(event.channels.size());
    WriteEventFile(path, fImpl->indexName, event, fImpl->indexRows, rows);
    const std::size_t firstRow = fImpl->indexRows - rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i)
        WriteIndexRow(fImpl->index.Get(), static_cast<LONGLONG>(firstRow + i + 1), rows[i]);
}

void Writer::Finish() {
    if (fImpl->finished) return;
    int status = 0;
    fits_write_chksum(fImpl->index.Get(), &status);
    CheckFits(status, "write FITS grouping checksum");
    fImpl->index.Close();
    fImpl->finished = true;
}

} // namespace FitsInternal
} // namespace GetDAQ
