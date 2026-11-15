#include "queryrunner.h"
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef _WIN32
int wmain(int argc,wchar_t **wideArgv) {
    std::vector<std::string> storage; storage.reserve(std::size_t(argc));
    std::vector<char *> arguments; arguments.reserve(std::size_t(argc));
    for (int i=0;i<argc;++i) {
        const std::wstring wide(wideArgv[i]);
        for (std::size_t j=0;j<wide.size();++j) {
            const auto unit=static_cast<unsigned int>(wide[j]);
            if (unit>=0xd800 && unit<=0xdbff) {
                if (j+1>=wide.size() || static_cast<unsigned int>(wide[j+1])<0xdc00 || static_cast<unsigned int>(wide[j+1])>0xdfff) {
                    std::cerr<<"Invalid UTF-16 command-line argument\n"; return 2;
                }
                ++j;
            } else if (unit>=0xdc00 && unit<=0xdfff) {
                std::cerr<<"Invalid UTF-16 command-line argument\n"; return 2;
            }
        }
        const int count=::WideCharToMultiByte(CP_UTF8,0,wideArgv[i],-1,nullptr,0,nullptr,nullptr);
        if (!count) { std::cerr<<"Invalid UTF-16 command-line argument\n"; return 2; }
        std::vector<char> buffer(static_cast<std::size_t>(count),char());
        if (!::WideCharToMultiByte(CP_UTF8,0,wideArgv[i],-1,buffer.data(),count,nullptr,nullptr)) {
            std::cerr<<"Cannot encode UTF-8 command-line argument\n"; return 2;
        }
        storage.emplace_back(buffer.data());
    }
    for (auto &value:storage) arguments.push_back(&value[0]);
    return RunQuery(argc,arguments.data());
}
#else
int main(int argc,char **argv) { return RunQuery(argc,argv); }
#endif
