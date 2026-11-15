#include "configurationsha256.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <cstring>
namespace GetDAQ {
namespace {
// NIST FIPS 180-4 sections 4.2.2, 5.1.1, 5.3.3, 6.2.
// https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
const std::uint32_t K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
std::uint32_t R(std::uint32_t v,unsigned n) { return (v>>n)|(v<<(32-n)); }
}
std::string ConfigurationSha256(const std::string &bytes) {
    if (bytes.size()>std::numeric_limits<std::uint64_t>::max()/8) throw std::invalid_argument("SHA-256 input too large");
    std::array<std::uint32_t,8> h{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    const std::uint64_t bits=std::uint64_t(bytes.size())*8;
    const std::size_t blocks=bytes.size()/64+((bytes.size()%64)<56 ? 1 : 2);
    for (std::size_t block=0; block<blocks; ++block) {
        std::uint32_t w[64]={};
        for (unsigned i=0; i<64; ++i) {
            const std::size_t offset=block*64+i;
            unsigned int b=0;
            if (offset<bytes.size()) b=static_cast<unsigned char>(bytes[offset]);
            else if (offset==bytes.size()) b=128;
            else if (block==blocks-1 && i>=56) b=unsigned((bits>>(8*(63-i)))&255);
            w[i/4] |= std::uint32_t(b)<<(8*(3-i%4));
        }
        for (unsigned i=16; i<64; ++i) w[i]=w[i-16]+(R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10));
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for (unsigned i=0; i<64; ++i) {
            const auto t1=v+(R(e,6)^R(e,11)^R(e,25))+((e&f)^(~e&g))+K[i]+w[i];
            const auto t2=(R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c));
            v=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=v;
    }
    std::string out; const char *hex="0123456789abcdef";
    for (auto word:h) for (int n=7; n>=0; --n) out+=hex[(word>>(n*4))&15];
    return out;
}
Sha256Stream::Sha256Stream() : fState{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}} {}
void Sha256Stream::Compress(const unsigned char *block) {
    std::uint32_t w[64]={};
    for(unsigned i=0;i<64;++i)w[i/4]|=std::uint32_t(block[i])<<(8*(3-i%4));
    for(unsigned i=16;i<64;++i)w[i]=w[i-16]+(R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10));
    auto a=fState[0],b=fState[1],c=fState[2],d=fState[3],e=fState[4],f=fState[5],g=fState[6],v=fState[7];
    for(unsigned i=0;i<64;++i) {
        const auto first=v+(R(e,6)^R(e,11)^R(e,25))+((e&f)^(~e&g))+K[i]+w[i];
        const auto second=(R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c));
        v=g;g=f;f=e;e=d+first;d=c;c=b;b=a;a=first+second;
    }
    fState[0]+=a;fState[1]+=b;fState[2]+=c;fState[3]+=d;fState[4]+=e;fState[5]+=f;fState[6]+=g;fState[7]+=v;
}
void Sha256Stream::Update(const void *bytes,std::size_t size) {
    if(fFinished || (size && !bytes) || size>std::numeric_limits<std::uint64_t>::max()/8-fBytes)
        throw std::invalid_argument("Invalid SHA-256 stream update");
    fBytes+=size;auto input=static_cast<const unsigned char *>(bytes);
    if(fUsed) {
        const auto count=std::min(size,64-fUsed);
        if(count)std::memcpy(fBlock.data()+fUsed,input,count);
        fUsed+=count;if(count)input+=count;size-=count;
        if(fUsed==64) {Compress(fBlock.data());fUsed=0;}
    }
    while(size>=64) {Compress(input);input+=64;size-=64;}
    if(size) {std::memcpy(fBlock.data(),input,size);fUsed=size;}
}
std::string Sha256Stream::Finish() {
    if(fFinished)throw std::logic_error("SHA-256 stream already finished");
    fFinished=true;const auto bits=fBytes*8;fBlock[fUsed++]=128;
    if(fUsed>56) {std::memset(fBlock.data()+fUsed,0,64-fUsed);Compress(fBlock.data());fUsed=0;}
    std::memset(fBlock.data()+fUsed,0,56-fUsed);
    for(unsigned i=0;i<8;++i)fBlock[56+i]=static_cast<unsigned char>(bits>>(8*(7-i)));
    Compress(fBlock.data());std::string output;const char *hex="0123456789abcdef";
    for(auto word:fState)for(int n=7;n>=0;--n)output+=hex[(word>>(n*4))&15];
    return output;
}
}
