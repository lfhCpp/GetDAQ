#include "scanexecutionjournalstore.h"
#include "scanexecutionjournalstore_windows_io_p.h"
#include "scanexecutionjournalstore_windows_support_p.h"

#include <stdexcept>
#include <vector>

namespace GetDAQ {
using namespace ScanExecutionJournalWindows;
namespace {

const std::size_t MaximumJournalBytes = 1024U * 1024U;

void Require(bool okay, const char *message)
{
    if (!okay) throw std::runtime_error(message);
}


}

class ScanExecutionJournalStore::Impl {
public:
    explicit Impl(const std::string &utf8Path)
        : fPath(), fDirectoryPath(), fLeaf(), fDirectory(), fLock(), fUserSid(CurrentUserSid()),
          fDirectoryIdentity(), fLockIdentity(), fFileIdentity(), fHasFile(false),
          fFaulted(false), fCurrent("initial", std::string(64, '0'), 1)
    {
        const std::wstring input = Utf8ToWide(utf8Path);
        if (input.size() < 3 || input[1] != L':' ||
            (input[2] != L'\\' && input[2] != L'/') ||
            input.compare(0, 4, L"\\\\?\\") == 0 || input.compare(0, 2, L"\\\\") == 0)
            throw std::invalid_argument("Scan journal path must be an absolute local-drive path");
        const std::wstring full = FullPath(input);
        if (full.size() < 4 || full[1] != L':' || full[2] != L'\\' ||
            full.compare(0, 4, L"\\\\?\\") == 0 || full.compare(0, 2, L"\\\\") == 0)
            throw std::invalid_argument("Scan journal path must be an absolute local-drive path");
        const std::size_t slash = full.find_last_of(L'\\');
        if (slash == std::wstring::npos || slash < 2 || slash + 1 == full.size())
            throw std::invalid_argument("Scan journal path must include a filename");
        fDirectoryPath = LongPath(slash == 2 ? full.substr(0, 3) : full.substr(0, slash));
        const std::wstring leafWide = full.substr(slash + 1);
        if (!SafeLeaf(leafWide)) throw std::invalid_argument("Unsafe scan journal filename");
        fLeaf = leafWide;
        fPath = WideToUtf8(fDirectoryPath + L"\\" + leafWide);

        fDirectory = Handle(::CreateFileW(fDirectoryPath.c_str(),
            FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (fDirectory.Get() == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot safely open scan journal directory");
        CheckDirectoryHandle(fDirectory.Get());
        fDirectoryIdentity = FileIdentity(fDirectory.Get());

        const std::wstring lockName = fDirectoryPath + L"\\" + leafWide + L".lock";
        fLock = Handle(::CreateFileW(lockName.c_str(),
            GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
            0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (fLock.Get() == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Scan execution journal is already owned or its lock is unsafe");
        CheckRegularFile(fLock.Get(), "Scan journal lock is not a regular local file");
        CheckPrivateAcl(fLock.Get(), fUserSid);
        fLockIdentity = FileIdentity(fLock.Get());
        CheckDirectory();
        CheckLock();
    }

    void Create(const ScanExecutionJournal &initial)
    {
        CheckWritable();
        const bool emptyInitial = initial.EventCount() == 0 &&
            initial.SourceSnapshotSha256().empty();
        const bool snapshotBoundInitial = initial.EventCount() == 1 &&
            !initial.SourceSnapshotSha256().empty();
        Require(!fHasFile && initial.PointIndex() == 0 &&
                initial.State() == ScanPointState::Planned &&
                (emptyInitial || snapshotBoundInitial) && !initial.HasPendingOperation(),
                "Scan journal creation requires an empty initial execution");
        const std::string bytes = initial.Encode();
        PublishNew(bytes);
        fCurrent = ScanExecutionJournal::Decode(bytes);
        fFileBytes = ReadCurrent(&fFileIdentity);
        Require(fFileBytes == bytes, "Created scan journal differs from initial state");
        fHasFile = true;
    }

    void Open()
    {
        CheckWritable();
        fFileBytes = ReadCurrent(&fFileIdentity);
        fCurrent = ScanExecutionJournal::Decode(fFileBytes);
        fHasFile = true;
    }

    ScanExecutionJournal Current() const
    {
        CheckWritable();
        Require(fHasFile, "Scan journal is not open");
        CheckDirectory(); CheckLock();
        return fCurrent;
    }

    void Persist(const ScanExecutionJournal &next)
    {
        CheckWritable();
        Require(fHasFile && next.IsImmediateSuccessorOf(fCurrent),
                "Scan journal update is not the immediate successor of current state");
        const std::string bytes = next.Encode();
        const ScanExecutionJournal validated = ScanExecutionJournal::Decode(bytes);
        Require(validated.EventCount() == next.EventCount(),
                "Scan journal update failed round-trip validation");
        try {
            CheckDirectory(); CheckLock();
            Identity currentIdentity;
            const std::string current = ReadCurrent(&currentIdentity);
            Require(current == fFileBytes && Same(currentIdentity, fFileIdentity),
                    "Scan journal changed outside the active writer; reconcile before continuing");
            PublishReplace(bytes);
            Identity newIdentity;
            const std::string observed = ReadCurrent(&newIdentity);
            Require(observed == bytes, "Published scan journal differs from requested update");
            fCurrent = next; fFileBytes = observed; fFileIdentity = newIdentity;
        } catch (...) { fFaulted = true; throw; }
    }

    const std::string &Path() const { return fPath; }

private:
    static bool SafeLeaf(const std::wstring &leaf)
    {
        if (leaf.empty() || leaf.size() > 128 || leaf == L"." || leaf == L"..") return false;
        for (wchar_t c : leaf)
            if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                  (c >= L'0' && c <= L'9') || c == L'.' || c == L'_' || c == L'-'))
                return false;
        return true;
    }

    void CheckWritable() const
    {
        if (fFaulted) throw std::runtime_error("Scan journal storage fault; stop and reconcile, do not retry writes");
    }

    void CheckDirectoryHandle(HANDLE handle) const
    {
        BY_HANDLE_FILE_INFORMATION info;
        Require(::GetFileType(handle) == FILE_TYPE_DISK &&
                ::GetFileInformationByHandle(handle, &info) != 0 &&
                (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT),
                "Scan journal parent must be a real directory, not a reparse point");
        CheckPrivateAcl(handle, fUserSid);
    }

    void CheckDirectory() const
    {
        CheckDirectoryHandle(fDirectory.Get());
        Require(Same(FileIdentity(fDirectory.Get()), fDirectoryIdentity),
                "Scan journal directory identity changed");
        Handle named(::CreateFileW(fDirectoryPath.c_str(),
            FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        Require(named.Get() != INVALID_HANDLE_VALUE,
                "Scan journal directory path changed");
        CheckDirectoryHandle(named.Get());
        Require(Same(FileIdentity(named.Get()), fDirectoryIdentity),
                "Scan journal directory file identity changed");
        Require(EqualPath(FinalPath(named.Get()), fDirectoryPath),
                "Scan journal directory canonical path changed");
    }

    void CheckLock() const
    {
        CheckRegularFile(fLock.Get(), "Scan journal lock is no longer a regular file");
        CheckPrivateAcl(fLock.Get(), fUserSid);
        Require(Same(FileIdentity(fLock.Get()), fLockIdentity),
                "Scan journal lock identity changed");
    }

    std::string ReadCurrent(Identity *identity) const
    {
        CheckDirectory();
        return ReadJournalFile(fDirectoryPath, fLeaf, fUserSid, identity,
                               MaximumJournalBytes);
    }

    void PublishNew(const std::string &bytes)
    {
        PublishNewJournalFile(fDirectoryPath, fLeaf, fUserSid, bytes);
    }

    void PublishReplace(const std::string &bytes)
    {
        ReplaceJournalFile(fDirectoryPath, fLeaf, fUserSid, fFileIdentity, bytes);
    }

    std::string fPath;
    std::wstring fDirectoryPath, fLeaf;
    Handle fDirectory, fLock;
    std::vector<unsigned char> fUserSid;
    Identity fDirectoryIdentity, fLockIdentity, fFileIdentity;
    bool fHasFile, fFaulted;
    std::string fFileBytes;
    ScanExecutionJournal fCurrent;
};

ScanExecutionJournalStore::ScanExecutionJournalStore(std::unique_ptr<Impl> impl)
    : fImpl(std::move(impl)) {}
ScanExecutionJournalStore::~ScanExecutionJournalStore() = default;
std::unique_ptr<ScanExecutionJournalStore> ScanExecutionJournalStore::Create(
    const std::string &path, const ScanExecutionJournal &initial)
{
    std::unique_ptr<Impl> impl(new Impl(path)); impl->Create(initial);
    return std::unique_ptr<ScanExecutionJournalStore>(new ScanExecutionJournalStore(std::move(impl)));
}
std::unique_ptr<ScanExecutionJournalStore> ScanExecutionJournalStore::Open(const std::string &path)
{
    std::unique_ptr<Impl> impl(new Impl(path)); impl->Open();
    return std::unique_ptr<ScanExecutionJournalStore>(new ScanExecutionJournalStore(std::move(impl)));
}
ScanExecutionJournal ScanExecutionJournalStore::Current() const { return fImpl->Current(); }
void ScanExecutionJournalStore::Persist(const ScanExecutionJournal &next) { fImpl->Persist(next); }
const std::string &ScanExecutionJournalStore::Path() const { return fImpl->Path(); }

}
