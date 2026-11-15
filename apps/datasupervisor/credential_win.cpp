#include "credential.h"
#include "supervisorcommon.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <atomic>
#include <stdexcept>
#include <vector>

namespace GetDAQ {
namespace {
std::wstring Wide(const std::string &value) {
    if (value.empty() || value.size()>32760 || value.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid supervisor credential path");
    const int count=::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if (!count) throw std::invalid_argument("Supervisor credential path must be UTF-8");
    std::wstring result(std::size_t(count),L'\0');
    if (!::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),&result[0],count))
        throw std::runtime_error("Cannot decode supervisor credential path");
    return result;
}
void SystemError(const char *message) { throw std::runtime_error(message); }
struct FileIdentity {
    DWORD volume=0,indexHigh=0,indexLow=0,links=0,attributes=0;
    unsigned long long size=0;
    FILETIME writeTime={};
};
FileIdentity Identity(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION info={};
    if (!::GetFileInformationByHandle(handle,&info)) SystemError("Cannot inspect supervisor credential file");
    FileIdentity result;
    result.volume=info.dwVolumeSerialNumber; result.indexHigh=info.nFileIndexHigh; result.indexLow=info.nFileIndexLow;
    result.links=info.nNumberOfLinks; result.attributes=info.dwFileAttributes;
    result.size=(static_cast<unsigned long long>(info.nFileSizeHigh)<<32)|info.nFileSizeLow; result.writeTime=info.ftLastWriteTime;
    return result;
}
bool Same(const FileIdentity &a,const FileIdentity &b) {
    return a.volume==b.volume && a.indexHigh==b.indexHigh && a.indexLow==b.indexLow;
}
std::vector<unsigned char> CurrentUserSid() {
    HANDLE raw=nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(),TOKEN_QUERY,&raw)) SystemError("Cannot inspect current Windows user");
    struct TokenGuard { HANDLE value; ~TokenGuard(){ if(value) ::CloseHandle(value); } } guard={raw};
    DWORD size=0; ::GetTokenInformation(raw,TokenUser,nullptr,0,&size);
    if (!size) SystemError("Cannot inspect current Windows user");
    std::vector<unsigned char> buffer(size);
    if (!::GetTokenInformation(raw,TokenUser,buffer.data(),size,&size)) SystemError("Cannot inspect current Windows user");
    const auto *user=reinterpret_cast<const TOKEN_USER *>(buffer.data());
    const DWORD sidSize=::GetLengthSid(user->User.Sid);
    std::vector<unsigned char> sid(sidSize);
    if (!::CopySid(sidSize,sid.data(),user->User.Sid)) SystemError("Cannot copy current Windows user identity");
    return sid;
}
bool AllowedSid(PSID sid,const std::vector<unsigned char> &user,const std::vector<unsigned char> &system) {
    return ::EqualSid(sid,const_cast<unsigned char *>(user.data())) || ::EqualSid(sid,const_cast<unsigned char *>(system.data()));
}
void CheckAcl(HANDLE handle,const std::vector<unsigned char> &user) {
    PSID owner=nullptr; PACL dacl=nullptr; PSECURITY_DESCRIPTOR descriptor=nullptr;
    const DWORD result=::GetSecurityInfo(handle,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
                                         &owner,nullptr,&dacl,nullptr,&descriptor);
    if (result!=ERROR_SUCCESS || !descriptor) SystemError("Cannot inspect supervisor credential permissions");
    struct DescriptorGuard { PSECURITY_DESCRIPTOR value; ~DescriptorGuard(){ if(value) ::LocalFree(value); } } guard={descriptor};
    SECURITY_DESCRIPTOR_CONTROL control=0; DWORD revision=0;
    if (!owner || !::EqualSid(owner,const_cast<unsigned char *>(user.data())) || !dacl ||
        !::GetSecurityDescriptorControl(descriptor,&control,&revision) || !(control&SE_DACL_PROTECTED))
        throw std::runtime_error("Supervisor credential must have a protected owner-only Windows ACL");
    unsigned char systemBytes[SECURITY_MAX_SID_SIZE]; DWORD systemSize=sizeof(systemBytes);
    if (!::CreateWellKnownSid(WinLocalSystemSid,nullptr,systemBytes,&systemSize)) SystemError("Cannot inspect Windows system identity");
    const auto system=std::vector<unsigned char>(systemBytes,systemBytes+systemSize);
    ACL_SIZE_INFORMATION aclInfo={};
    if (!::GetAclInformation(dacl,&aclInfo,sizeof(aclInfo),AclSizeInformation)) SystemError("Cannot inspect supervisor credential ACL");
    bool userRead=false;
    for (DWORD i=0;i<aclInfo.AceCount;++i) {
        void *rawAce=nullptr;
        if (!::GetAce(dacl,i,&rawAce)) SystemError("Cannot inspect supervisor credential ACL");
        const auto *header=static_cast<const ACE_HEADER *>(rawAce);
        if (header->AceFlags&INHERITED_ACE || header->AceType!=ACCESS_ALLOWED_ACE_TYPE)
            throw std::runtime_error("Supervisor credential ACL contains an inherited or non-allow entry");
        const auto *ace=static_cast<const ACCESS_ALLOWED_ACE *>(rawAce);
        PSID sid=const_cast<DWORD *>(&ace->SidStart);
        if (!AllowedSid(sid,user,system)) throw std::runtime_error("Supervisor credential ACL grants access to another Windows principal");
        if (::EqualSid(sid,const_cast<unsigned char *>(user.data())) && (ace->Mask&FILE_GENERIC_READ)) userRead=true;
    }
    if (!userRead) throw std::runtime_error("Supervisor credential ACL does not grant its owner read access");
}
std::string ReadHandle(HANDLE handle) {
    LARGE_INTEGER zero={};
    if (!::SetFilePointerEx(handle,zero,nullptr,FILE_BEGIN)) SystemError("Cannot read supervisor credential file");
    char bytes[67]; DWORD count=0;
    if (!::ReadFile(handle,bytes,sizeof(bytes),&count,nullptr) || count>66) throw std::runtime_error("Supervisor credential must be at most 66 bytes");
    return std::string(bytes,count);
}
}
class SupervisorCredential::Impl {
public:
    std::wstring path;
    HANDLE handle=INVALID_HANDLE_VALUE;
    FileIdentity identity;
    std::vector<unsigned char> user;
    std::string bytes,token;
    mutable std::atomic<bool> fault{false};
    ~Impl() { if(handle!=INVALID_HANDLE_VALUE) ::CloseHandle(handle); }
};
SupervisorCredential::SupervisorCredential(const std::string &path) : fImpl(new Impl) {
    fImpl->path=Wide(path); fImpl->user=CurrentUserSid();
    fImpl->handle=::CreateFileW(fImpl->path.c_str(),GENERIC_READ|READ_CONTROL,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if (fImpl->handle==INVALID_HANDLE_VALUE) SystemError("Cannot open supervisor credential file");
    fImpl->identity=Identity(fImpl->handle);
    if ((fImpl->identity.attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || fImpl->identity.links!=1 || fImpl->identity.size>66)
        throw std::runtime_error("Supervisor credential must be a single-link regular file, not a reparse point");
    CheckAcl(fImpl->handle,fImpl->user);
    fImpl->bytes=ReadHandle(fImpl->handle); fImpl->token=fImpl->bytes;
    if (!fImpl->token.empty() && fImpl->token.back()=='\n') { fImpl->token.pop_back(); if (!fImpl->token.empty() && fImpl->token.back()=='\r') fImpl->token.pop_back(); }
    SupervisorIO::Digest(fImpl->token); Check();
}
SupervisorCredential::~SupervisorCredential() = default;
void SupervisorCredential::Check() const {
    const auto &p=*fImpl;
    if (p.fault) throw std::runtime_error("Supervisor credential fault is latched; restart requires reconciliation");
    try {
        const auto held=Identity(p.handle);
        if (!Same(held,p.identity) || held.size!=p.identity.size || CompareFileTime(&held.writeTime,&p.identity.writeTime)!=0 ||
            (held.attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || held.links!=1)
            throw std::runtime_error("Supervisor credential identity changed");
        CheckAcl(p.handle,p.user);
        if (ReadHandle(p.handle)!=p.bytes) throw std::runtime_error("Supervisor credential content changed");
    } catch (...) { p.fault=true; throw; }
}
bool SupervisorCredential::Authorize(const std::string &header) const {
    Check(); if (header.size()!=71 || header.compare(0,7,"Bearer ")) return false;
    unsigned int different=0; for (std::size_t i=0;i<fImpl->token.size();++i) different|=static_cast<unsigned char>(fImpl->token[i])^static_cast<unsigned char>(header[i+7]); return different==0;
}
std::string SupervisorCredential::Token() const { Check(); return fImpl->token; }
}
