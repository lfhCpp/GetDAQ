#ifndef GETDAQ_FITS_WRITER_SUPPORT_P_H
#define GETDAQ_FITS_WRITER_SUPPORT_P_H

#include <fitsio.h>
#include <string>
#include <vector>

namespace GetDAQ {
namespace FitsInternal {

struct EventData;

class FitsFile {
public:
    explicit FitsFile(const std::string &path);
    ~FitsFile();
    fitsfile *Get() const;
    void Close();
    FitsFile(const FitsFile &) = delete;
    FitsFile &operator=(const FitsFile &) = delete;
private:
    fitsfile *fFile;
};

struct FitsIndexRow {
    unsigned int event = 0, version = 0, position = 0;
    unsigned short cobo = 0, asad = 0, aget = 0, channel = 0;
    std::string location;
};

std::string FitsError(int status);
void CheckFits(int status,const char *operation);
void Primary(fitsfile *file);
void KeyString(fitsfile *file,const char *key,const std::string &value,const char *comment="");
std::string Leaf(const std::string &path);
void WriteEventFile(const std::string &path,const std::string &indexName,
                    const EventData &event,std::size_t &indexRows,
                    std::vector<FitsIndexRow> &rows);

template <typename T>
void KeyInteger(fitsfile *file,int type,const char *key,T value,const char *comment="") {
    int status=0;
    fits_write_key(file,type,key,&value,const_cast<char *>(comment),&status);
    CheckFits(status,"write FITS integer keyword");
}

} // namespace FitsInternal
} // namespace GetDAQ

#endif
