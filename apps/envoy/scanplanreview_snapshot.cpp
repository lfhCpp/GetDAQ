#include "scanplanreview_p.h"

#include "scanexecutionjournalstore.h"
#include "../../common/experiment/scanexecutionsnapshot.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <stdexcept>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#else
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ScanPlanReviewInternal {

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
}

#ifdef Q_OS_WIN

void EnsurePrivateDirectory(const QString &path)
{
    const std::wstring widePath = QDir::toNativeSeparators(path).toStdWString();
    HANDLE rawToken = nullptr;
    Require(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken) != 0,
            "Cannot inspect current Windows user for the private scan review directory");
    DWORD tokenBytes = 0;
    ::GetTokenInformation(rawToken, TokenUser, nullptr, 0, &tokenBytes);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || tokenBytes == 0) {
        ::CloseHandle(rawToken);
        throw std::runtime_error("Cannot size current Windows user identity");
    }
    std::vector<unsigned char> tokenBuffer(tokenBytes);
    const BOOL tokenRead = ::GetTokenInformation(rawToken, TokenUser,
        tokenBuffer.data(), tokenBytes, &tokenBytes);
    ::CloseHandle(rawToken);
    Require(tokenRead != 0, "Cannot read current Windows user identity");

    const TOKEN_USER *tokenUser = reinterpret_cast<const TOKEN_USER *>(tokenBuffer.data());
    const DWORD sidBytes = ::GetLengthSid(tokenUser->User.Sid);
    std::vector<unsigned char> sid(sidBytes);
    Require(::CopySid(sidBytes, sid.data(), tokenUser->User.Sid) != 0,
            "Cannot copy current Windows user identity");

    EXPLICIT_ACCESSW access = {};
    access.grfAccessPermissions = GENERIC_ALL;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
    ::BuildTrusteeWithSidW(&access.Trustee, sid.data());
    PACL dacl = nullptr;
    const DWORD aclStatus = ::SetEntriesInAclW(1, &access, nullptr, &dacl);
    Require(aclStatus == ERROR_SUCCESS, "Cannot build private scan review ACL");

    SECURITY_DESCRIPTOR descriptor;
    if (!::InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !::SetSecurityDescriptorDacl(&descriptor, TRUE, dacl, FALSE) ||
        !::SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
        ::LocalFree(dacl);
        throw std::runtime_error("Cannot prepare private scan review security descriptor");
    }
    SECURITY_ATTRIBUTES security = {};
    security.nLength = sizeof(security);
    security.lpSecurityDescriptor = &descriptor;
    security.bInheritHandle = FALSE;
    const BOOL created = ::CreateDirectoryW(widePath.c_str(), &security);
    const DWORD error = created ? ERROR_SUCCESS : ::GetLastError();
    ::LocalFree(dacl);
    Require(created != 0 || error == ERROR_ALREADY_EXISTS,
            "Cannot create private scan review directory");
    // Existing directories are never loosened or silently repaired. The store
    // validates their owner and ACL before opening any journal file.
}

#else

void EnsurePrivateDirectory(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    const char *name = encoded.constData();
    bool created = false;
    if (::mkdir(name, 0700) == 0) {
        created = true;
    } else if (errno != EEXIST) {
        throw std::runtime_error("Cannot create private scan review directory");
    }
    if (created && ::chmod(name, 0700) != 0)
        throw std::runtime_error("Cannot restrict scan review directory permissions");
    struct stat info;
    Require(::lstat(name, &info) == 0 && S_ISDIR(info.st_mode) &&
            !S_ISLNK(info.st_mode) && info.st_uid == ::geteuid() &&
            (info.st_mode & 0777) == 0700,
            "Scan review directory must be a real current-user-owned 0700 directory");
}

#endif

QString ExecutionSnapshotPath(const QString &applicationDataDirectory,
                              const QString &planSha256)
{
    return QDir(applicationDataDirectory).filePath(
        QStringLiteral("scan-journals/snapshot-%1.json").arg(planSha256.left(56)));
}

std::string ReadBoundedRegularFile(const QString &path, qint64 maximumBytes,
                                   const char *description)
{
    const QFileInfo info(path);
    Require(info.isFile() && !info.isSymLink() && info.size() >= 0 &&
            info.size() <= maximumBytes, description);
    QFile file(path);
    Require(file.open(QIODevice::ReadOnly), description);
    const QByteArray bytes = file.readAll();
    Require(file.error() == QFileDevice::NoError && bytes.size() <= maximumBytes,
            description);
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}



SnapshotIdentity LoadOrCreateExecutionSnapshot(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const ScanPlanOutputSummary &summary, bool createIfMissing)
{
    const QString manifestPath = QDir(planDirectory).filePath(
        QStringLiteral("scan-manifest.json"));
    const QString policyPath = QDir(planDirectory).filePath(
        QStringLiteral("scan-execution-policy.json"));
    const std::string planBytes = ReadBoundedRegularFile(
        manifestPath, 1024 * 1024, "Scan manifest is not a bounded regular file");
    const std::string policyBytes = ReadBoundedRegularFile(
        policyPath, 4096, "A valid finite stopping policy is required before review");
    const QString absolutePlanDirectory = QFileInfo(planDirectory).absoluteFilePath();
    const std::string executionId = std::string("review-") +
        summary.planSha256.left(56).toStdString();
    const auto loadArtifact = [absolutePlanDirectory](const std::string &name) {
        const QString artifactName = QString::fromUtf8(name.data(),
            static_cast<int>(name.size()));
        const QString path = QDir(absolutePlanDirectory).filePath(artifactName);
        const QFileInfo info(path);
        Require(info.fileName() == artifactName &&
                info.absolutePath() == absolutePlanDirectory &&
                info.isFile() && !info.isSymLink(),
                "Scan point artifact must be a direct regular file in the plan directory");
        return ReadBoundedRegularFile(path, 1024 * 1024,
            "Scan point artifact is not a bounded regular file");
    };

    const GetDAQ::ScanExecutionSnapshot expected =
        GetDAQ::ScanExecutionSnapshot::Create(executionId, planBytes, policyBytes,
                                               loadArtifact);
    const std::string encoded = expected.Encode();
    const QString path = ExecutionSnapshotPath(applicationDataDirectory,
                                               summary.planSha256);
    std::string bytes;
    QFile output(path);
    if (output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        const QByteArray payload(encoded.data(), static_cast<int>(encoded.size()));
        const bool complete = output.write(payload) == payload.size() && output.flush() &&
            output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        output.close();
        Require(complete, "Cannot persist the scan execution snapshot");
        bytes = encoded;
    } else {
        Require(QFileInfo::exists(path), createIfMissing
            ? "Cannot exclusively create the scan execution snapshot"
            : "The scan execution snapshot is missing");
        bytes = ReadBoundedRegularFile(path, 1024 * 1024,
            "Stored scan execution snapshot is not a bounded regular file");
        const GetDAQ::ScanExecutionSnapshot stored =
            GetDAQ::ScanExecutionSnapshot::Decode(bytes, planBytes, policyBytes,
                                                  loadArtifact);
        Require(stored.ExecutionId() == executionId &&
                stored.SourcePlanSha256() == expected.SourcePlanSha256() &&
                stored.SourcePolicySha256() == expected.SourcePolicySha256(),
                "Stored scan execution snapshot belongs to different inputs");
    }
    const QByteArray snapshotBytes(bytes.data(), static_cast<int>(bytes.size()));
    SnapshotIdentity identity;
    identity.path = path;
    identity.bytes = bytes;
    identity.sha256 = QCryptographicHash::hash(
        snapshotBytes, QCryptographicHash::Sha256).toHex().toStdString();
    return identity;
}


}
