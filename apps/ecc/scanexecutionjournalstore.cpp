#include "scanexecutionjournalstore.h"
#include "scanexecutionjournalstore_publish_p.h"
#include "scanexecutionjournalstore_internal_p.h"

#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {

const std::size_t MaximumJournalBytes = 1024U * 1024U;

void Require(bool okay, const char *message)
{
    if (!okay) throw std::runtime_error(message);
}

bool Same(const struct stat &left, const struct stat &right)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

}

class ScanExecutionJournalStore::Impl {
public:
    explicit Impl(const std::string &path)
        : fPath(path), fDirectoryPath(), fLeaf(), fDirectory(-1), fLock(-1),
          fDirectoryIdentity(), fLockIdentity(), fFileIdentity(), fHasFile(false),
          fFaulted(false), fCurrent("initial", std::string(64, '0'), 1)
    {
        if (path.empty() || path[0] != '/' || path.find('\0') != std::string::npos)
            throw std::invalid_argument("Scan journal path must be an absolute POSIX path");
        const std::size_t slash = path.find_last_of('/');
        if (slash == std::string::npos || slash + 1 == path.size())
            throw std::invalid_argument("Scan journal path must include a filename");
        fDirectoryPath = slash == 0 ? "/" : path.substr(0, slash);
        fLeaf = path.substr(slash + 1);
        if (!SafeLeaf(fLeaf)) throw std::invalid_argument("Unsafe scan journal filename");
        char resolved[PATH_MAX];
        if (!::realpath(path.c_str(), resolved) && errno != ENOENT)
            throw std::runtime_error("Cannot resolve scan journal path");
        if (::realpath(fDirectoryPath.c_str(), resolved) == nullptr ||
            fDirectoryPath != resolved)
            throw std::runtime_error("Scan journal directory must be a canonical path without symlinks");
        fDirectory = ::open(fDirectoryPath.c_str(), O_RDONLY | O_DIRECTORY |
                            O_NOFOLLOW | O_CLOEXEC);
        if (fDirectory < 0) throw std::runtime_error("Cannot open scan journal directory safely");
        try {
            struct stat named;
            Require(::fstat(fDirectory, &fDirectoryIdentity) == 0 &&
                    ::lstat(fDirectoryPath.c_str(), &named) == 0 &&
                    S_ISDIR(named.st_mode) && Same(fDirectoryIdentity, named) &&
                    named.st_uid == ::geteuid() && (named.st_mode & 0777) == 0700,
                    "Scan journal directory must be private, owned, and unchanged");
            const std::string lockName = fLeaf + ".lock";
            fLock = ::openat(fDirectory, lockName.c_str(),
                             O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (fLock < 0) throw std::runtime_error("Cannot safely open scan journal lock");
            struct stat lockNamed;
            Require(::fstat(fLock, &fLockIdentity) == 0 &&
                    ::fstatat(fDirectory, lockName.c_str(), &lockNamed,
                              AT_SYMLINK_NOFOLLOW) == 0 &&
                    S_ISREG(lockNamed.st_mode) && Same(fLockIdentity, lockNamed) &&
                    lockNamed.st_uid == ::geteuid() && (lockNamed.st_mode & 0777) == 0600 &&
                    lockNamed.st_nlink == 1,
                    "Scan journal lock identity or permissions are unsafe");
            if (::flock(fLock, LOCK_EX | LOCK_NB) != 0)
                throw std::runtime_error("Scan execution journal is already owned by another process");
            CheckDirectory();
            CheckLock();
        } catch (...) {
            Close();
            throw;
        }
    }

    ~Impl() { Close(); }

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
        ScanExecutionJournalStoreInternal::PublishNew(fDirectory, fLeaf, bytes);
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
        CheckDirectory();
        CheckLock();
        return fCurrent;
    }

    void Persist(const ScanExecutionJournal &next)
    {
        CheckWritable();
        Require(fHasFile && next.IsImmediateSuccessorOf(fCurrent),
                "Scan journal update is not the immediate successor of current state");
        const std::string bytes = next.Encode();
        const ScanExecutionJournal validated = ScanExecutionJournal::Decode(bytes);
        Require(validated.EventCount() == next.EventCount(), "Scan journal update failed round-trip validation");
        try {
            CheckDirectory();
            CheckLock();
            struct stat currentIdentity;
            const std::string current = ReadCurrent(&currentIdentity);
            Require(current == fFileBytes && Same(currentIdentity, fFileIdentity),
                    "Scan journal changed outside the active writer; reconcile before continuing");
            ScanExecutionJournalStoreInternal::PublishReplace(
                fDirectory, fLeaf, fFileIdentity, bytes);
            struct stat newIdentity;
            const std::string observed = ReadCurrent(&newIdentity);
            Require(observed == bytes, "Published scan journal differs from requested update");
            // Keep the active candidate as-is. Decode deliberately quarantines
            // an unresolved intent, which is correct on reopen but would block
            // its still-live writer from recording the command result.
            fCurrent = next;
            fFileBytes = observed;
            fFileIdentity = newIdentity;
        } catch (...) {
            fFaulted = true;
            throw;
        }
    }

    const std::string &Path() const { return fPath; }

private:
    static bool SafeLeaf(const std::string &leaf)
    {
        if (leaf.empty() || leaf.size() > 128 || leaf == "." || leaf == "..") return false;
        for (unsigned char c : leaf)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
                return false;
        return true;
    }

    void CheckWritable() const
    {
        if (fFaulted) throw std::runtime_error("Scan journal storage fault; stop and reconcile, do not retry writes");
    }

    void CheckDirectory() const
    {
        struct stat held, named;
        Require(::fstat(fDirectory, &held) == 0 &&
                ::lstat(fDirectoryPath.c_str(), &named) == 0 &&
                S_ISDIR(named.st_mode) && Same(fDirectoryIdentity, held) &&
                Same(fDirectoryIdentity, named) && named.st_uid == ::geteuid() &&
                (named.st_mode & 0777) == 0700,
                "Scan journal directory identity/permissions changed");
    }

    void CheckLock() const
    {
        struct stat held, named;
        const std::string lockName = fLeaf + ".lock";
        Require(::fstat(fLock, &held) == 0 &&
                ::fstatat(fDirectory, lockName.c_str(), &named,
                          AT_SYMLINK_NOFOLLOW) == 0 &&
                S_ISREG(named.st_mode) && Same(fLockIdentity, held) &&
                Same(fLockIdentity, named) && named.st_uid == ::geteuid() &&
                (named.st_mode & 0777) == 0600 && named.st_nlink == 1,
                "Scan journal lock identity or permissions changed");
    }

    std::string ReadCurrent(struct stat *identity) const
    {
        CheckDirectory();
        return ScanExecutionJournalStoreInternal::ReadCurrent(
            fDirectory,fLeaf,identity,MaximumJournalBytes);
    }

    void Close()
    {
        if (fLock >= 0) { ::close(fLock); fLock = -1; }
        if (fDirectory >= 0) { ::close(fDirectory); fDirectory = -1; }
    }

    std::string fPath, fDirectoryPath, fLeaf;
    int fDirectory, fLock;
    struct stat fDirectoryIdentity, fLockIdentity, fFileIdentity;
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
    std::unique_ptr<Impl> impl(new Impl(path));
    impl->Create(initial);
    return std::unique_ptr<ScanExecutionJournalStore>(
        new ScanExecutionJournalStore(std::move(impl)));
}

std::unique_ptr<ScanExecutionJournalStore> ScanExecutionJournalStore::Open(
    const std::string &path)
{
    std::unique_ptr<Impl> impl(new Impl(path));
    impl->Open();
    return std::unique_ptr<ScanExecutionJournalStore>(
        new ScanExecutionJournalStore(std::move(impl)));
}

ScanExecutionJournal ScanExecutionJournalStore::Current() const
{
    return fImpl->Current();
}

void ScanExecutionJournalStore::Persist(const ScanExecutionJournal &next)
{
    fImpl->Persist(next);
}

const std::string &ScanExecutionJournalStore::Path() const
{
    return fImpl->Path();
}

}
