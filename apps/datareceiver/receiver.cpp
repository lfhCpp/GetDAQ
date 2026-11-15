#include "receiver.h"
#include <stdexcept>
#include "framestore.h"
#include "netutil.h"
#include <iostream>
#include <sys/file.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace GetDAQ {
Receiver::Receiver(const ReceiverConfig &config)
    : fConfig(config), fQueue(config.blocks, config.blockBytes), fExporter(config.bind, config.exportPort, config.exportBudget)
{
    ValidateReceiverIdentity(config);
    // 在开始收数前检查输出路径，并取得目录锁，避免两个接收器共用目录。
    FrameStore check(config.output, config.rotateBytes, config.durable, config.fileNaming, config.minimumFreeBytes);
    fDirectoryLease.reset(new DirectoryLease(config.output));
    try {
        DIR *existing = ::opendir(config.output.c_str());
        if (!existing) throw std::runtime_error("Cannot inspect incoming directory");
        bool hasRaw = false;
        while (dirent *entry = ::readdir(existing)) {
            const std::string name = entry->d_name;
            if (name.size() >= 5 && name.substr(name.size() - 5) == ".graw") hasRaw = true;
        }
        ::closedir(existing);
        if (hasRaw) throw std::runtime_error("Incoming directory contains .graw files; archive or recover them before starting another receiver");
        // 标记一直保留：归档代理遇到此目录时必须验证本机锁和封存凭据。
        const int marker = ::open((config.output + "/.getdaq-native-receiver").c_str(), O_CREAT | O_WRONLY, 0640);
        if (marker < 0) throw std::runtime_error("Cannot create native receiver marker");
        ::close(marker);
        const std::string seal = config.output + "/.getdaq-receiver-seal.json";
        if (::unlink(seal.c_str()) != 0 && errno != ENOENT) throw std::runtime_error("Cannot invalidate previous receiver seal");
        fListen = Listen(config.bind, config.port);
        if (config.statusPort) fStatus = Listen(config.statusBind, config.statusPort);
        if (config.cycleControlStdin) {
            struct stat descriptor={};
            if (::fstat(STDIN_FILENO,&descriptor) || !S_ISSOCK(descriptor.st_mode)) throw std::runtime_error("Cycle control stdin must be a private socket");
            const int flags=::fcntl(STDIN_FILENO,F_GETFL),fdFlags=::fcntl(STDIN_FILENO,F_GETFD);
            if (flags<0 || fdFlags<0 || ::fcntl(STDIN_FILENO,F_SETFL,flags|O_NONBLOCK) || ::fcntl(STDIN_FILENO,F_SETFD,fdFlags|FD_CLOEXEC))
                throw std::runtime_error("Cannot protect cycle control socket");
        }
    } catch (...) { if (fListen >= 0) ::close(fListen); fListen = -1; if (fStatus>=0) ::close(fStatus); fStatus=-1; throw; }
}
Receiver::~Receiver()
{
    fStop = true;
    if (fReceiveThread.joinable()) fReceiveThread.join();
    fQueue.Close();
    if (fWriteThread.joinable()) fWriteThread.join();
    if (fListen >= 0) ::close(fListen);
    if (fStatus >= 0) ::close(fStatus);
}
void Receiver::Fail(const std::string &message)
{
    std::lock_guard<std::mutex> lock(fErrorMutex);
    if (fError.empty()) { fError = message; std::cerr << "ERROR: " << message << '\n'; }
    fStop = true; fSealed = false;
}
void Receiver::Receive()
{
    int client = -1;
    try {
        while (!fStop) {
            ReadCycleControl();
            if (fConfig.cycleControlStdin && fCycleFinish) {
                if ((!fConnections && !fCycleUnused) || fEofs!=fConnections) throw std::runtime_error("Cycle finish requires an actual sender EOF");
                ::close(fListen); fListen=-1; break; // 不再接受下一段，写盘线程排空已有真实EOF。
            }
            if (!Readable(fListen, 100)) continue;
            ReadCycleControl();
            if (fConfig.cycleControlStdin && fCycleFinish) continue;
            client = ::accept(fListen, nullptr, nullptr);
            if (client < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; throw std::runtime_error("Input accept failed"); }
            Nonblock(client); fConnected = true; fSealed = false; ++fConnections;
            bool eof = false;
            while (!fStop) {
                ReadCycleControl(); // 只记录结束请求，继续等当前发送连接真实EOF。
                if (!Readable(client, 100)) continue;
                BlockQueue::Ptr block = fQueue.Acquire(fStop);
                if (!block) break;
                const ssize_t n = ::recv(client, block->data.data(), block->data.size(), 0);
                if (n > 0) { block->used = std::size_t(n); fReceived += uint64_t(n); fQueue.Push(std::move(block)); }
                else if (n == 0) { ++fEofs; block->end = true; fQueue.Push(std::move(block)); eof = true; break; }
                else {
                    fQueue.Release(std::move(block));
                    if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) throw std::runtime_error("Input recv failed: " + std::string(std::strerror(errno)));
                }
            }
            ::close(client); client = -1; fConnected = false;
            if (!eof) { Fail("Input interrupted before sender EOF; acquisition completeness is unknown"); break; }
            if (fConfig.once) break;
        }
    } catch (const std::exception &e) { Fail(e.what()); }
    if (client >= 0) ::close(client);
    fConnected = false; fQueue.Close();
}
void Receiver::ReadCycleControl()
{
    if (!fConfig.cycleControlStdin || fCycleFinish || fCycleControlLost) return;
    char request=0; const auto count=::read(STDIN_FILENO,&request,1);
    if (count==1) {
        if (request=='F') fCycleFinish=true;
        else if (request=='E') {
            // 未使用来源的结束声明，不生成发送器EOF。待接纳连接也拒绝。
            if (fConnections || fReceived || fConnected || Readable(fListen,0)) {fCycleUnusedRefused=true;return;}
            fCycleUnused=true;fCycleFinish=true;
        }
        else if (request=='A') {
            if (fConnections || fReceived || fConnected) {fCycleAbortRefused=true;return;}
            fCycleAborted=true;throw std::runtime_error("Cycle startup aborted before sender connection");
        } else throw std::runtime_error("Invalid private cycle control request");
    } else if (!count) fCycleControlLost=true; // 保留录盘，不把控制器消失当发送器EOF。
    else if (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) throw std::runtime_error("Cycle control socket read failed");
}
int Receiver::Run(const volatile std::sig_atomic_t &interrupted)
{
    fWriteThread = std::thread(&Receiver::Write, this);
    fReceiveThread = std::thread(&Receiver::Receive, this);
    std::cout << "Listening data " << fConfig.bind << ':' << fConfig.port << "; status " << fConfig.statusBind << ':' << fConfig.statusPort << std::endl;
    while (!fWriterDone) {
        if (interrupted) fStop = true;
        ServeStatus();
    }
    fStop = true; fReceiveThread.join(); fWriteThread.join();
    if (fError.empty()) {
        try { SaveSeal(); } catch (const std::exception &e) { Fail(e.what()); }
    }
    std::cout << Status();
    std::lock_guard<std::mutex> lock(fErrorMutex);
    return fError.empty() ? 0 : 1;
}


}
