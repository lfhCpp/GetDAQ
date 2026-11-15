#include "fits_writer_support_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace FitsInternal {

std::string FitsError(int status) {
    char message[FLEN_STATUS] = {};
    fits_get_errstatus(status, message);
    return std::string(message);
}

void CheckFits(int status,const char *operation) {
    if (!status) return;
    std::string message=std::string(operation)+": "+FitsError(status);
    char detail[FLEN_ERRMSG];
    while(fits_read_errmsg(detail))message+="; "+std::string(detail);
    throw std::runtime_error(message);
}

FitsFile::FitsFile(const std::string &path) : fFile(nullptr) {
    int status=0;
    fits_create_file(&fFile,path.c_str(),&status);
    CheckFits(status,"create FITS file");
}
FitsFile::~FitsFile() {
    if(fFile) { int status=0; fits_close_file(fFile,&status); }
}
fitsfile *FitsFile::Get() const { return fFile; }
void FitsFile::Close() {
    if(!fFile)return;
    int status=0;
    fits_close_file(fFile,&status);
    fFile=nullptr;
    CheckFits(status,"close FITS file");
}

void Primary(fitsfile *file) {
    int status=0; fits_create_img(file,BYTE_IMG,0,nullptr,&status);
    CheckFits(status,"create primary FITS HDU");
}
void KeyString(fitsfile *file,const char *key,const std::string &value,const char *comment) {
    int status=0; char *text=const_cast<char *>(value.c_str());
    fits_write_key(file,TSTRING,key,text,const_cast<char *>(comment),&status);
    CheckFits(status,"write FITS string keyword");
}
std::string Leaf(const std::string &path) {
    const auto slash=path.find_last_of('/');
    return slash==std::string::npos ? path : path.substr(slash+1);
}

} // namespace FitsInternal
} // namespace GetDAQ
