#ifndef GETDAQ_RECEIVERCATALOG_H
#define GETDAQ_RECEIVERCATALOG_H
#include "datasupervisor.h"
#include "catalogreplacement.h"
#include <functional>

namespace GetDAQ {
struct ReceiverCatalogOptions {
    DataSupervisorOptions receiver;
    DataSupervisorStatus stopped;
    std::string executableSha256,operationId,experiment;
    unsigned int runNumber=0;
    bool stagedForReplacement=false;
};
std::string EncodeReceiverCatalogRequest(const ReceiverCatalogOptions &options);
ReceiverCatalogOptions DecodeReceiverCatalogRequest(const std::string &bytes);
std::string ReceiverCatalogRequestKey(const ReceiverCatalogOptions &options);
std::string ReceiverCatalogPublicationTarget(const std::string &catalogRoot,const std::string &deploymentId,
                                             const std::string &experiment,unsigned int runNumber);
std::string ReceiverCatalogTargetPath(const ReceiverCatalogOptions &options);
std::string ReceiverCatalogReplacementStagePath(const std::string &target,const std::string &replacementOperationId);
std::string ReceiverCatalogReplacementCandidatePath(const std::string &target,const std::string &replacementOperationId);
std::string ReceiverCatalogReplacementStagePath(const ReceiverCatalogOptions &options);
std::string ReceiverCatalogReplacementCandidatePath(const ReceiverCatalogOptions &options);
std::string InspectReceiverCatalog(const std::string &directory); // 只核对归档元数据/文件身份，不读取帧。
// Build a read-only same-Run merge plan from two independently verified
// complete catalogs. No files are moved, removed, or read as raw payload.
std::vector<CatalogReplacementEntry> PlanReceiverCatalogReplacement(
    const std::string &oldCatalogDirectory,const std::string &newCatalogDirectory);
// Build and verify a separate same-filesystem merged catalog candidate from two
// complete catalogs. This never replaces/removes either input catalog.
std::string PrepareReceiverCatalogReplacement(const std::string &oldCatalogDirectory,
                                               const std::string &newCatalogDirectory,
                                               const std::string &candidateDirectory);
// Publish a prepared schema 2 candidate at the old Run's fixed catalog path.
// The old catalog is retained at its operation-bound backup path. Repeating
// the same call observes/reconciles the original local operation only.
std::string PublishReceiverCatalogReplacement(const std::string &oldCatalogDirectory,
                                               const std::string &newCatalogDirectory,
                                               const std::string &candidateDirectory);
// Read-only classification of the durable local replacement transaction. It
// never repairs markers, renames catalogs, or removes raw links. A returned
// phase is evidence for operator review, not authorization to resume a parent
// multi-node transaction by itself.
std::string InspectReceiverCatalogReplacement(const std::string &oldCatalogDirectory,
                                               const std::string &newCatalogDirectory,
                                               const std::string &candidateDirectory,
                                               const std::string &replacementOperationId);
// After the caller has verified every fixed data node's published receipt,
// remove only superseded same-name links from the old backup and its owning
// current-session directory. Other old raw names remain available. The
// operation is durable and idempotent for the same replacement operation ID.
std::string FinalizeReceiverCatalogReplacement(const std::string &publishedCatalogDirectory,
                                                const std::string &replacementOperationId);
// 验证一个扫描点与已完成归档的配置摘要相同，并排他写出独立Run绑定凭据。
std::string BindScanPointToCatalog(const std::string &scanDirectory,unsigned int pointIndex,
                                   const std::string &catalogDirectory,const std::string &outputPath);
// Verify every ordered scan point against its exclusive binding and sealed catalog,
// then publish one complete, immutable JSON index. Raw payload bytes are never read.
std::string SummarizeScanBindings(const std::string &scanDirectory,const std::string &bindingDirectory,
                                  const std::string &outputPath);
// Validate every threshold point/catalog binding, stream the selected MFM
// hit-pattern headers, then publish one immutable report. coboNumber is the
// numeric CoBo ID stored in the MFM frame header; target_cobo remains the
// operator's source ID and is checked against the archived source plan.
std::string AnalyzeThresholdScan(const std::string &scanDirectory,
                                const std::string &bindingDirectory,
                                unsigned int coboNumber,
                                const std::string &outputPath);
// Analyze AutoGain waveform response from the exact Run/catalog bindings.
// Supports flat MFM type-1 rev0..5 and reports waveform metrics only.
std::string AnalyzeBoundAutoGainWaveforms(const std::string &scanDirectory,
                                          const std::string &bindingDirectory,
                                          unsigned int coboNumber,
                                          const std::string &outputPath);
// Analyze a selected channel's archived response for a bound Pulser amplitude scan.
// The Pulser plan does not encode an AsAd/AGET/channel selection, so these are explicit operator inputs.
std::string AnalyzeBoundPulserWaveforms(const std::string &scanDirectory,
                                        const std::string &bindingDirectory,
                                        unsigned int coboNumber,
                                        unsigned int asad,
                                        unsigned int aget,
                                        unsigned int channel,
                                        const std::string &outputPath);
std::string AnalyzeBoundPeakingTimeWaveforms(const std::string &scanDirectory,
                                            const std::string &bindingDirectory,
                                            unsigned int coboNumber,
                                            unsigned int asad,
                                            unsigned int aget,
                                            unsigned int channel,
                                            const std::string &outputPath);

// 已封存数据的同文件系统目录归档：只读元数据，用硬链接保留原会话路径。
// 没有原始帧读入、复制或网络传输；失败保留意图/部分结果，不自动重放。
class ReceiverCatalog {
public:
    typedef std::function<void(const std::string &,const std::string &,std::uint64_t)> Progress;
    explicit ReceiverCatalog(const ReceiverCatalogOptions &options,Progress progress=Progress());
    ~ReceiverCatalog();
    ReceiverCatalog(const ReceiverCatalog &)=delete;
    ReceiverCatalog &operator=(const ReceiverCatalog &)=delete;
    void Validate(); // 核对绑定/EOF/seal/目录/文件并保留源锁，不创建目标或意图。
    std::string Execute(); // 独占目标；提交所有源意图后链接，最后发布完整凭据。
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
