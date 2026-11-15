#ifndef GETDAQ_RECEIVERCATALOGFIXTURE_H
#define GETDAQ_RECEIVERCATALOGFIXTURE_H
#include "receivercatalog.h"
#include "owned_test_directory.h"
#include "supervisorio.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <fcntl.h>
#include <algorithm>
#include <unistd.h>
#include <sys/file.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
using namespace GetDAQ;
namespace GetDAQCatalogFixture {
using namespace SupervisorIO;
std::uint64_t &FixtureRawBytes() {static std::uint64_t bytes=0;return bytes;}
void Require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
void Reject(const std::function<void()> &action) { bool rejected=false; try { action(); } catch (const std::exception &) { rejected=true; } Require(rejected,"Expected catalog rejection"); }
bool Exists(const std::string &path) { struct stat info; return !::lstat(path.c_str(),&info); }
std::vector<unsigned char> Type1WaveformFrame(unsigned cobo) {
    std::vector<unsigned char> frame(52,0); frame[0]=0x80; frame[1]=52; frame[4]=1; frame[5]=1; frame[7]=5;
    frame[8]=28; frame[10]=4; frame[12]=6; frame[26]=static_cast<unsigned char>(cobo); frame[27]=0;
    const unsigned buckets[]={197,198,199,200,201,202}; const unsigned values[]={0,10,50,100,60,0};
    for (unsigned sample=0;sample<6;++sample) {
        const std::uint32_t word=(buckets[sample]<<14)|values[sample]; const std::size_t offset=28+sample*4;
        frame[offset]=static_cast<unsigned char>(word>>24); frame[offset+1]=static_cast<unsigned char>(word>>16);
        frame[offset+2]=static_cast<unsigned char>(word>>8); frame[offset+3]=static_cast<unsigned char>(word);
    }
    return frame;
}
struct Fixture {
    static std::string Prefix() {const auto root=std::getenv("GETDAQ_CATALOG_TEST_ROOT");return root ? std::string(root)+"/catalog-" : "/tmp/getdaq-catalog-test-";}
    OwnedTestDirectory owned{Prefix()};
    std::string root,target;
    ReceiverCatalogOptions options;
    explicit Fixture(std::uint64_t large=8192,bool validFrames=false,bool waveformFrames=false,bool compactWaveformFrames=false,bool layeredWaveformFrames=false,char sessionMarker='d') {
        root=owned.Path();
        auto &plan=options.receiver; plan.auditRoot=root+"/audit"; plan.deploymentId="node-0"; plan.receiverExecutable=root+"/receiver-stub";
        plan.configurationSha256=std::string(64,'a'); plan.artifactSha256=std::string(64,'b'); plan.catalogRoot=root+"/catalog"; CreateDirectory(plan.catalogRoot);
        options.operationId="catalog-0"; options.experiment="Test"; options.runNumber=0; options.executableSha256=std::string(64,'c');
        options.stopped.state=DataSupervisorStatus::Stopped; options.stopped.sessionId=std::string(64,sessionMarker);
        for (unsigned int i=0;i<2;++i) {
            ReceiverSourcePlan source; source.id="C"+std::to_string(i); source.bindAddress="127.0.0.1"; source.dataPort=55000+i*3; source.statusPort=source.dataPort+1;
            source.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort); source.outputRoot=root+"/incoming"+std::to_string(i); CreateDirectory(source.outputRoot); plan.sources.push_back(source);
            ManagedReceiverStatus status; status.id=source.id; status.sessionId=options.stopped.sessionId; status.pid=2411+int(i); status.reaped=true; status.exitCode=0; status.sealVerified=true;
            status.outputDirectory=source.outputRoot+"/getdaq-"+status.sessionId+"-source-"+std::to_string(i); CreateDirectory(status.outputDirectory);
            WriteNew(status.outputDirectory+"/.getdaq-receiver.lock","");
            for (unsigned int file=0;file<2;++file) {
                const auto bytes=layeredWaveformFrames ? 68 : compactWaveformFrames ? std::uint64_t(28)+std::uint64_t(2)*4*68*512 : waveformFrames ? 52 : validFrames ? 80 : (file==0 ? large : 4096); FixtureRawBytes()+=bytes; Fd raw(::open((status.outputDirectory+"/data"+std::to_string(file)+".graw").c_str(),O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600));
                Require(raw.value>=0 && !::ftruncate(raw.value,off_t(bytes)),"Create sparse raw fixture");
                if (layeredWaveformFrames) {
                    const auto child=Type1WaveformFrame(i);
                    std::vector<unsigned char> wrapper(68,0); wrapper[0]=0x80; wrapper[1]=68; wrapper[4]=1; wrapper[5]=1; wrapper[6]=0xff; wrapper[7]=1;
                    wrapper[8]=16; wrapper[12]=1;
                    std::copy(child.begin(),child.end(),wrapper.begin()+16);
                    Require(::pwrite(raw.value,wrapper.data(),wrapper.size(),0)==static_cast<ssize_t>(wrapper.size()),"Write tiny layered MFM test frame");
                } else if (validFrames || waveformFrames || compactWaveformFrames) {
                    std::vector<unsigned char> frame(static_cast<std::size_t>(bytes),0); frame[0]=0x80;
                    frame[1]=static_cast<unsigned char>(bytes); frame[2]=static_cast<unsigned char>(bytes>>8); frame[3]=static_cast<unsigned char>(bytes>>16);
                    frame[4]=1; frame[5]=compactWaveformFrames ? 2 : 1; frame[7]=compactWaveformFrames ? 5 : 5;
                    frame[8]=(waveformFrames || compactWaveformFrames) ? 28 : 80;
                    frame[26]=static_cast<unsigned char>(i); frame[27]=0;
                    if (waveformFrames) {
                        frame[10]=4; frame[12]=6;
                        const unsigned buckets[]={197,198,199,200,201,202}; const unsigned values[]={0,10,50,100,60,0};
                        for (unsigned sample=0;sample<6;++sample) {
                            const std::uint32_t word=(buckets[sample]<<14)|values[sample];
                            const std::size_t offset=28+sample*4;
                            frame[offset]=static_cast<unsigned char>(word>>24); frame[offset+1]=static_cast<unsigned char>(word>>16);
                            frame[offset+2]=static_cast<unsigned char>(word>>8); frame[offset+3]=static_cast<unsigned char>(word);
                        }
                    } else if (compactWaveformFrames) {
                        frame[10]=2;
                        const std::uint32_t itemCount=4u*68u*512u;
                        for (unsigned byte=0;byte<4;++byte) frame[12+byte]=static_cast<unsigned char>(itemCount>>(8*byte));
                        const unsigned buckets[]={197,198,199,200,201,202}; const unsigned values[]={0,10,50,100,60,0};
                        std::size_t offset=28;
                        for (unsigned aget=0;aget<4;++aget) for (unsigned bucket=0;bucket<512;++bucket) for (unsigned channel=0;channel<68;++channel) {
                            unsigned value=0;
                            if (aget==0 && channel==0) for (unsigned sample=0;sample<6;++sample) if (buckets[sample]==bucket) value=values[sample];
                            const std::uint16_t word=static_cast<std::uint16_t>((aget<<14)|value);
                            frame[offset++]=static_cast<unsigned char>(word>>8); frame[offset++]=static_cast<unsigned char>(word);
                        }
                    } else frame[31]=0x10;
                    Require(::pwrite(raw.value,frame.data(),frame.size(),0)==static_cast<ssize_t>(frame.size()),"Write tiny valid MFM test frame");
                }
                Require(!::fsync(raw.value),"Sync raw fixture"); status.receivedBytes+=bytes;
            }
            status.frames=3; status.files=2; options.stopped.receivers.push_back(status);
            const auto seal="{\"schema\":1,\"complete\":true,\"control_protocol\":1,\"pid\":"+std::to_string(status.pid)+",\"session_id\":"+EncodeJsonString(status.sessionId)+
                ",\"sender_id\":"+EncodeJsonString(status.id)+",\"configuration_sha256\":"+EncodeJsonString(plan.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(plan.artifactSha256)+
                ",\"output_directory\":"+EncodeJsonString(status.outputDirectory)+",\"connections\":1,\"sender_eof_confirmed\":true,\"durable\":true,\"received_bytes\":"+EncodeJsonString(std::to_string(status.receivedBytes))+
                ",\"files\":[{\"name\":\"data0.graw\",\"bytes\":"+EncodeJsonString(std::to_string(layeredWaveformFrames ? 68 : compactWaveformFrames ? std::uint64_t(28)+std::uint64_t(2)*4*68*512 : waveformFrames ? 52 : validFrames ? 80 : large))+"},{\"name\":\"data1.graw\",\"bytes\":"+EncodeJsonString(std::to_string(layeredWaveformFrames ? 68 : compactWaveformFrames ? std::uint64_t(28)+std::uint64_t(2)*4*68*512 : waveformFrames ? 52 : validFrames ? 80 : 4096))+"}]}\n";
            WriteNew(status.outputDirectory+"/.getdaq-receiver-seal.json",seal);
        }
        options.stopped.planSha256=ConfigurationSha256(EncodeOptions(plan,options.executableSha256)); target=plan.catalogRoot+"/Test/run_0000/node-0";
    }
};
}
#endif
