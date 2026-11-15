#ifndef GETDAQ_CONFIGURATIONSHA256_H
#define GETDAQ_CONFIGURATIONSHA256_H
#include <string>
#include <array>
#include <cstddef>
#include <cstdint>
namespace GetDAQ {
// 配置字节的SHA-256摘要，无额外库；不是签名、授权或实机加载证明。
std::string ConfigurationSha256(const std::string &bytes);
// Incremental SHA-256 for native bulk jobs. Retains one 64-byte block, no Qt/OpenSSL.
class Sha256Stream {
public:
    Sha256Stream();
    void Update(const void *bytes,std::size_t size);
    std::string Finish();
private:
    std::array<std::uint32_t,8> fState;
    std::array<unsigned char,64> fBlock;
    std::uint64_t fBytes=0;
    std::size_t fUsed=0;
    bool fFinished=false;
    void Compress(const unsigned char *block);
};
}
#endif
