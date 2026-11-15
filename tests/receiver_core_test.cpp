#include "mfmstream.h"
#include "framestore.h"
#include "bytestore.h"
#include "rawfile.h"
#include "blockqueue.h"
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>

using namespace GetDAQ;
static void Check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
class TestDirectory {
public:
    TestDirectory() {char name[]="/tmp/getdaq-get-naming-XXXXXX";Check(::mkdtemp(name)!=nullptr,"mkdtemp GET naming");path=name;}
    ~TestDirectory() {
        DIR *directory=::opendir(path.c_str());if(!directory) return;
        while(dirent *entry=::readdir(directory)) {
            const std::string name=entry->d_name;if(name!="." && name!="..") ::unlink((path+"/"+name).c_str());
        }
        ::closedir(directory);::rmdir(path.c_str());
    }
    std::string path;
};
static void Put(std::vector<uint8_t> &b, unsigned offset, uint64_t v, unsigned n, bool little) {
    for (unsigned i = 0; i < n; ++i) b[offset + (little ? i : n - 1 - i)] = uint8_t(v >> (8 * i));
}
static std::vector<uint8_t> Frame(bool little, uint32_t event, unsigned asad = 0) {
    std::vector<uint8_t> b(256, 0x5a);
    b[0] = uint8_t(6 | (little ? 0x80 : 0)); Put(b, 1, 4, 3, little); b[4] = 9;
    Put(b, 5, 1, 2, little); b[7] = 4; Put(b, 8, 2, 2, little);
    Put(b, 10, 4, 2, little); Put(b, 12, 32, 4, little); Put(b, 22, event, 4, little);
    b[26] = 2; b[27] = uint8_t(asad); return b;
}
int main() {
    try {
        {
            TestDirectory directory;
            const std::string path=directory.path+"/same-name.raw";
            const int first=OpenRawStorageFile(path); const char oldBytes[]="older contents";
            Check(::write(first,oldBytes,sizeof(oldBytes)-1)==ssize_t(sizeof(oldBytes)-1),"write initial raw collision fixture");
            bool activeRejected=false;try { OpenRawStorageFile(path); } catch(const std::exception &) { activeRejected=true; }
            std::ifstream activePreserved(path.c_str(),std::ios::binary);
            const std::string activeBytes((std::istreambuf_iterator<char>(activePreserved)),std::istreambuf_iterator<char>());
            Check(activeRejected && activeBytes==oldBytes,"active same-name raw is rejected without truncating its contents");
            Check(!::close(first),"close initial raw collision fixture");
            const int second=OpenRawStorageFile(path); const char newBytes[]="new";
            Check(::write(second,newBytes,sizeof(newBytes)-1)==ssize_t(sizeof(newBytes)-1),"write replacement raw collision fixture");
            Check(!::close(second),"close replacement raw collision fixture");
            std::ifstream replaced(path.c_str(),std::ios::binary); const std::string actual((std::istreambuf_iterator<char>(replaced)),std::istreambuf_iterator<char>());
            Check(actual=="new","same filename collision truncates old contents");
            const std::string hardlinked=directory.path+"/hardlinked.raw",alias=directory.path+"/hardlinked-alias";
            const int source=::open(hardlinked.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
            Check(source>=0 && ::write(source,oldBytes,sizeof(oldBytes)-1)==ssize_t(sizeof(oldBytes)-1) && !::close(source) && !::link(hardlinked.c_str(),alias.c_str()),"create hardlink protection fixture");
            bool hardlinkRejected=false;try { OpenRawStorageFile(hardlinked); } catch(const std::exception &) { hardlinkRejected=true; }
            std::ifstream preserved(hardlinked.c_str(),std::ios::binary); const std::string preservedBytes((std::istreambuf_iterator<char>(preserved)),std::istreambuf_iterator<char>());
            Check(hardlinkRejected && preservedBytes==oldBytes,"same-name overwrite preserves multiply-linked archive data");
            const std::string symlink=directory.path+"/symbolic.raw"; Check(!::symlink(alias.c_str(),symlink.c_str()),"create raw storage symlink fixture");
            bool symlinkRejected=false;try { OpenRawStorageFile(symlink); } catch(const std::exception &) { symlinkRejected=true; }
            Check(symlinkRejected && preservedBytes==oldBytes,"same-name overwrite rejects symbolic links");
            const std::string fifo=directory.path+"/fifo.raw"; Check(!::mkfifo(fifo.c_str(),0600),"create raw storage FIFO fixture");
            bool fifoRejected=false;try { OpenRawStorageFile(fifo); } catch(const std::exception &) { fifoRejected=true; }
            struct stat fifoInfo;
            Check(fifoRejected && !::lstat(fifo.c_str(),&fifoInfo) && S_ISFIFO(fifoInfo.st_mode),
                  "same-name overwrite rejects a FIFO without blocking or replacing it");
        }
        Check(!DiskReserveAllows(1024*1024-1,0,0,0),"Disk reserve accepted space below the metadata margin");
        Check(DiskReserveAllows(2*1024*1024,0,0,0),"Disk reserve rejected the exact check-window and metadata margin");
        Check(!DiskReserveAllows(UINT64_MAX,UINT64_MAX,1,1),"Disk reserve arithmetic overflow was accepted");
        Check(DiskReserveAllows(2*1024*1024+512,0,128,128),"Disk reserve did not account for buffered, incoming and check-window bytes");
        Check(!DiskReserveCheckNeeded(0,1) && !DiskReserveCheckNeeded(0,1024*1024) &&
              DiskReserveCheckNeeded(1,1024*1024) && DiskReserveCheckNeeded(1024*1024,1),
              "Disk reserve checks were not amortized across the configured byte window");
        {
            TestDirectory directory;
            ByteStore store(directory.path, 4, true, "get");
            const uint8_t first[] = {0x00, 0x7f, 0xff};
            const uint8_t second[] = {0x01, 0x02, 0x03};
            const uint8_t third[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
            store.Write(first, sizeof(first));
            store.Write(second, sizeof(second));
            store.Write(third, sizeof(third));
            store.Seal();
            Check(store.Bytes() == sizeof(first)+sizeof(second)+sizeof(third) && store.Files() == 2,
                  "ByteStorage byte accounting or chunk-boundary rotation differs");
            DIR *directoryHandle = ::opendir(directory.path.c_str());
            Check(directoryHandle != nullptr, "Inspect ByteStorage output");
            std::map<std::string,std::vector<uint8_t>> files;
            while (dirent *entry = ::readdir(directoryHandle)) {
                const std::string name = entry->d_name;
                if (name.size() < 5 || name.substr(name.size()-5) != ".graw") continue;
                std::ifstream input(directory.path + "/" + name, std::ios::binary);
                files[name] = std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
            }
            ::closedir(directoryHandle);
            Check(files.size() == 2, "ByteStorage did not create the expected number of rotated files");
            std::vector<uint8_t> joined;
            for (const auto &file : files) joined.insert(joined.end(), file.second.begin(), file.second.end());
            std::vector<uint8_t> expected(first, first+sizeof(first));
            expected.insert(expected.end(), second, second+sizeof(second));
            expected.insert(expected.end(), third, third+sizeof(third));
            Check(joined == expected, "ByteStorage changed or reordered incoming bytes");
        }
        {
            TestDirectory directory;bool rejected=false;
            try {FrameStore store(directory.path,1024,true,"native",UINT64_MAX);}
            catch(const DiskReserveExceeded &) {rejected=true;}
            Check(rejected,"Insufficient free-space reserve did not reject receiver preflight");
            DIR *contents=::opendir(directory.path.c_str());Check(contents!=nullptr,"Inspect disk preflight output");
            unsigned int entries=0;while(dirent *entry=::readdir(contents))if(std::string(entry->d_name)!="." && std::string(entry->d_name)!="..")++entries;
            ::closedir(contents);Check(entries==0,"Disk preflight created output artifacts");
        }
        {
            TestDirectory directory;struct statvfs space={};
            Check(::statvfs(directory.path.c_str(),&space)==0,"Read free space for runtime reserve test");
            const uint64_t unit=uint64_t(space.f_frsize ? space.f_frsize : space.f_bsize);
            const uint64_t blocks=uint64_t(space.f_bavail);
            Check(unit && blocks<=UINT64_MAX/unit,"Runtime reserve fixture free-space range");
            const uint64_t available=blocks*unit,margin=8ull*1024*1024;
            Check(available>margin,"Runtime reserve fixture requires at least 8 MiB free");
            FrameStore store(directory.path,64ull*1024*1024,true,"native",available-margin);
            const auto buffered=Frame(false,1);store.Write(buffered.data(),MfmStream::Inspect(buffered.data(),buffered.size()));
            std::vector<uint8_t> oversized(16ull*1024*1024,0x5a);FrameInfo incoming;incoming.bytes=oversized.size();
            bool rejected=false;
            try {store.Write(oversized.data(),incoming);} catch(const DiskReserveExceeded &) {rejected=true;store.Abandon();}
            Check(rejected,"Runtime free-space crossing did not stop the writer");
            Check(store.Bytes()==buffered.size(),"Rejected frame changed accepted-byte accounting");
            DIR *contents=::opendir(directory.path.c_str());Check(contents!=nullptr,"Inspect runtime reserve output");
            unsigned int rawFiles=0;bool sealFound=false;
            while(dirent *entry=::readdir(contents)) {
                const std::string name=entry->d_name;if(name=="." || name=="..")continue;
                if(name.size()>=5 && name.substr(name.size()-5)==".graw") {
                    ++rawFiles;struct stat output={};
                    Check(::stat((directory.path+"/"+name).c_str(),&output)==0 && output.st_size==0,"Disk reserve failure flushed a pending frame buffer");
                }
                if(name.find("seal")!=std::string::npos)sealFound=true;
            }
            ::closedir(contents);Check(rawFiles==1 && !sealFound,"Disk reserve failure sealed or lost the incomplete output");
        }
        {
            TestDirectory directory;FrameStore store(directory.path,256,true,"get");
            const auto a=Frame(false,10),b=Frame(true,11),c=Frame(false,1),d=Frame(true,8,1);
            for(const auto &frame:{a,b,c,d}) store.Write(frame.data(),MfmStream::Inspect(frame.data(),frame.size()));
            store.Seal();store.Write(a.data(),MfmStream::Inspect(a.data(),a.size()));store.Seal();
            std::map<std::string,std::vector<uint8_t>> saved;DIR *dir=::opendir(directory.path.c_str());Check(dir!=nullptr,"Read GET names");
            const std::regex format(R"((CoBo[0-9]+_AsAd[0-3])_([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3})_([0-9]{4,})\.graw)");
            std::string timestamp;
            while(dirent *entry=::readdir(dir)) {
                const std::string name=entry->d_name;if(name=="." || name=="..") continue;
                std::smatch match;Check(std::regex_match(name,match,format),"GET name format differs");
                if(timestamp.empty()) timestamp=match[2];else Check(timestamp==match[2],"GET timestamp changed within store");
                std::ifstream in(directory.path+"/"+name,std::ios::binary);
                saved[match[1].str()+"_"+match[3].str()]=std::vector<uint8_t>(std::istreambuf_iterator<char>(in),{});
            }
            ::closedir(dir);Check(saved.size()==5 && store.Files()==5,"GET per-source file count differs");
            Check(saved["CoBo2_AsAd0_0000"]==a && saved["CoBo2_AsAd0_0001"]==b && saved["CoBo2_AsAd0_0002"]==c &&
                  saved["CoBo2_AsAd1_0000"]==d && saved["CoBo2_AsAd0_0003"]==a,"GET sequence/bytes/reset/reconnect differs");
        }
        {
            TestDirectory directory;FrameStore store(directory.path,256,true,"get");const auto a=Frame(false,10);
            store.Write(a.data(),MfmStream::Inspect(a.data(),a.size()));store.Seal();
            DIR *dir=::opendir(directory.path.c_str());Check(dir!=nullptr,"GET collision scan");std::string next;
            while(dirent *entry=::readdir(dir)) {const std::string name=entry->d_name;if(name.find("_0000.graw")!=std::string::npos) next=name.substr(0,name.size()-10)+"_0001.graw";}
            ::closedir(dir);Check(!next.empty(),"GET first file missing");
            const std::string guard="do not overwrite";{std::ofstream file(directory.path+"/"+next,std::ios::binary);file<<guard;}
            store.Write(a.data(),MfmStream::Inspect(a.data(),a.size()));store.Seal();std::ifstream file(directory.path+"/"+next,std::ios::binary);
            const std::vector<uint8_t> replaced(std::istreambuf_iterator<char>(file),{});
            Check(replaced==a,"GET exact filename collision truncates and replaces only that file");
        }
        for (bool little : {false, true}) {
            const auto frame = Frame(little, 0x12345678);
            for (std::size_t split = 1; split < frame.size(); ++split) {
                int count = 0;
                MfmStream decoder(4096, [&](const uint8_t *p, const FrameInfo &info) {
                    Check(info.event == 0x12345678 && info.cobo == 2 && info.asad == 0, "Incorrect GET fields");
                    Check(info.littleEndian == little && std::equal(frame.begin(), frame.end(), p), "Frame bytes altered"); ++count;
                });
                decoder.Feed(frame.data(), split); decoder.Feed(frame.data() + split, frame.size() - split);
                decoder.Feed(frame.data(), frame.size()); decoder.Finish(); Check(count == 2, "Lost concatenated frame");
            }
        }
        auto bad = Frame(false, 1); Put(bad, 1, 0xffffff, 3, false);
        // 旧FDT只在MFM帧间插入4字节控制字，不是额外长度前缀。
        for (bool little : {false, true}) {
            auto raw = Frame(little, 3);
            std::fill(raw.begin()+128, raw.begin()+132, 0);
            std::fill(raw.begin()+132, raw.begin()+136, 0xff);
            std::vector<uint8_t> wire(4, 0);
            wire.insert(wire.end(), raw.begin(), raw.end());
            wire.insert(wire.end(), 4, 0);
            wire.insert(wire.end(), raw.begin(), raw.end());
            wire.insert(wire.end(), 4, 0xff);
            for (std::size_t chunk = 1; chunk <= wire.size(); ++chunk) {
                unsigned frames = 0, heartbeats = 0, goodbyes = 0;
                MfmStream stream(4096, [&](const uint8_t *p, const FrameInfo &info) {
                    Check(info.bytes == raw.size() && std::equal(raw.begin(), raw.end(), p), "FDT changed frame payload");
                    ++frames;
                }, true, [&](MfmStream::Control c) { if(c == MfmStream::Heartbeat) ++heartbeats; else ++goodbyes; });
                for(std::size_t pos=0; pos<wire.size(); pos+=chunk)
                    stream.Feed(wire.data()+pos, std::min(chunk, wire.size()-pos));
                stream.EndConnection();
                Check(frames==2 && heartbeats==2 && goodbyes==1, "FDT boundary controls lost");
                stream.Feed(raw.data(),raw.size()); stream.EndConnection();
                Check(frames==3, "FDT next connection retained previous GOODBYE");
            }
        }
        for (unsigned size : {1u, 2u, 3u}) {
            bool failed=false; const uint8_t control[4]={0,0,0,0};
            try { MfmStream stream(4096, [](const uint8_t *, const FrameInfo &){}, true);
                stream.Feed(control,size); stream.Finish(); } catch(const std::runtime_error &) { failed=true; }
            Check(failed,"Partial FDT control accepted at EOF");
        }
        {
            const uint8_t goodbye[4]={255,255,255,255}, extra=0;
            MfmStream stream(4096, [](const uint8_t *, const FrameInfo &){}, true);
            stream.Feed(goodbye,4); bool failed=false;
            try { stream.Feed(&extra,1); } catch(const std::runtime_error &) { failed=true; }
            Check(failed,"FDT bytes after GOODBYE accepted");
            MfmStream plain(4096, [](const uint8_t *, const FrameInfo &){});
            const uint8_t heartbeat[8]={0,0,0,0,0,0,0,0}; failed=false;
            try { plain.Feed(heartbeat,8); } catch(const std::runtime_error &) { failed=true; }
            Check(failed,"Raw TCP implicitly detected FDT");
        }
        {
            const auto a=Frame(false,7),b=Frame(true,8);auto joined=a;joined.insert(joined.end(),b.begin(),b.end());
            unsigned calls=0;
            MfmStream direct(4096,[&](const uint8_t *p,const FrameInfo &info) {
                Check(p==joined.data()+calls*256,"Complete frame was copied instead of synchronously borrowed");
                Check(info.event==7+calls,"Borrowed frame fields differ");++calls;
            });
            direct.Feed(joined.data(),joined.size());direct.Finish();Check(calls==2,"Borrowed concatenated frames lost");
            std::vector<uint8_t> actual;
            MfmStream split(4096,[&](const uint8_t *p,const FrameInfo &info) {actual.insert(actual.end(),p,p+info.bytes);});
            split.Feed(joined.data(),17);split.Feed(joined.data()+17,joined.size()-17);split.Finish();Check(actual==joined,"Mixed buffered/borrowed data altered");
        }
        bool rejected = false;
        try { MfmStream d(1024, [](const uint8_t *, const FrameInfo &) {}); d.Feed(bad.data(), bad.size()); }
        catch (const std::exception &) { rejected = true; } Check(rejected, "Oversized frame accepted");
        rejected = false;
        try { auto f = Frame(false, 1); MfmStream d(1024, [](const uint8_t *, const FrameInfo &) {}); d.Feed(f.data(), 20); d.Finish(); }
        catch (const std::exception &) { rejected = true; } Check(rejected, "Truncated frame accepted");
        bad = Frame(false, 1); Put(bad, 12, 0xffffffff, 4, false);
        rejected = false;
        try { MfmStream::Inspect(bad.data(), bad.size()); } catch (...) { rejected = true; }
        Check(rejected, "Invalid item bounds accepted");
        // 同一坏头既可能完整到达，也可能跨TCP块；两条路径均须先拒绝再调用Sink。
        for (bool little : {false, true}) {
            for (unsigned invalid = 0; invalid < 5; ++invalid) {
                auto malformed = Frame(little, 9);
                if (invalid == 0) Put(malformed, 1, 0, 3, little);
                if (invalid == 1) Put(malformed, 1, 0xffffff, 3, little);
                if (invalid == 2) Put(malformed, 8, 0, 2, little);
                if (invalid == 3) Put(malformed, 12, 0xffffffff, 4, little);
                if (invalid == 4) malformed[27] = 4;
                for (std::size_t split : {std::size_t(0), std::size_t(3), std::size_t(8), std::size_t(128)}) {
                    unsigned calls = 0; bool failed = false;
                    MfmStream decoder(256, [&](const uint8_t *, const FrameInfo &) { ++calls; });
                    try {
                        decoder.Feed(malformed.data(), split);
                        decoder.Feed(malformed.data() + split, malformed.size() - split);
                        decoder.Finish();
                    } catch (const std::runtime_error &) { failed = true; }
                    Check(failed && calls == 0, "Malformed borrowed/buffered frame reached Sink");
                }
            }
            const auto valid = Frame(little, 10);
            auto joined = valid; auto invalid = valid; invalid[27] = 4;
            joined.insert(joined.end(), invalid.begin(), invalid.end());
            unsigned calls = 0; bool failed = false;
            MfmStream decoder(256, [&](const uint8_t *p, const FrameInfo &info) {
                Check(info.bytes == 256 && std::equal(valid.begin(), valid.end(), p), "Valid prefix frame changed");
                ++calls;
            });
            try { decoder.Feed(joined.data(), joined.size()); } catch (const std::runtime_error &) { failed = true; }
            Check(failed && calls == 1, "Malformed next frame lost the valid prefix or reached Sink");
        }
        char directory[] = "/tmp/getdaq-receiver-test-XXXXXX";
        Check(::mkdtemp(directory) != nullptr, "mkdtemp");
        const auto a = Frame(false, 10), b = Frame(false, 11), c = Frame(false, 1), d = Frame(true, 8, 1);
        FrameStore store(directory, 1024 * 1024, true);
        for (const auto &f : {a,b,c,d}) store.Write(f.data(), MfmStream::Inspect(f.data(), f.size()));
        store.Seal(); Check(store.Files() == 3 && store.Bytes() == 1024, "Source grouping / event reset rotation failed");
        std::vector<std::vector<uint8_t>> actual, expected = {a,c,d}; expected[0].insert(expected[0].end(), b.begin(), b.end());
        DIR *dir = ::opendir(directory); Check(dir != nullptr, "opendir");
        while (dirent *e = ::readdir(dir)) {
            const std::string name = e->d_name;
            if (name.size() < 5 || name.substr(name.size()-5) != ".graw") continue;
            const std::string path = std::string(directory) + "/" + name;
            std::ifstream in(path, std::ios::binary);
            actual.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); in.close(); ::unlink(path.c_str());
        }
        ::closedir(dir); ::rmdir(directory);
        std::sort(actual.begin(), actual.end()); std::sort(expected.begin(), expected.end());
        Check(actual == expected, "Stored payload/order differs from input");
        BlockQueue queue(2, 64); std::atomic<bool> stop(false); uint64_t sum = 0;
        std::thread reader([&]() { while (auto block = queue.Pop()) { sum += block->used; queue.Release(std::move(block)); } });
        for (unsigned i = 0; i < 10000; ++i) { auto block = queue.Acquire(stop); block->used = i; queue.Push(std::move(block)); }
        queue.Close(); reader.join(); Check(sum == 49995000, "Queue ordering/loss");
        std::cout << "PASS: both endian modes, every TCP split point, concatenation, bounds, truncation, exact-file collisions, FIFO rejection, rotation, bounded queue\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
