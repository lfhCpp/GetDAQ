#include "receiver_options.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void Stop(int) { interrupted = 1; }
}
int main(int argc, char **argv)
{
    try {
        std::vector<std::string> arguments;
        for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
        const GetDAQ::ReceiverCommandLine commandLine = GetDAQ::ParseReceiverOptions(arguments);
        if (commandLine.showHelp) { GetDAQ::PrintReceiverUsage(std::cout); return 0; }
        GetDAQ::ValidateReceiverIdentity(commandLine.config);
        std::signal(SIGINT, Stop); std::signal(SIGTERM, Stop);
        // 文件大小上限由write返回EFBIG，沿正常错误路径报告，避免信号直接终止丢失状态。
        std::signal(SIGXFSZ, SIG_IGN);
        GetDAQ::Receiver receiver(commandLine.config);
        return receiver.Run(interrupted);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
}
