#ifndef GETDAQ_OWNED_TEST_DIRECTORY_H
#define GETDAQ_OWNED_TEST_DIRECTORY_H
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
// 测试专用：独占新建目录，析构不跟随符号链接，不触及真实Run。
class OwnedTestDirectory {
public:
    explicit OwnedTestDirectory(const std::string &prefix) {
        std::vector<char> pattern(prefix.begin(),prefix.end());const char ending[]="XXXXXX";
        pattern.insert(pattern.end(),ending,ending+sizeof(ending));
        const char *created=::mkdtemp(pattern.data());if(!created) throw std::runtime_error("Cannot create owned test directory");fPath=created;
    }
    ~OwnedTestDirectory() {
        const int fd=::open(fPath.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(fd>=0) {Clear(fd);::close(fd);::rmdir(fPath.c_str());}
    }
    const std::string &Path() const {return fPath;}
    void Preserve() {fPath.clear();}
    OwnedTestDirectory(const OwnedTestDirectory &)=delete;
    OwnedTestDirectory &operator=(const OwnedTestDirectory &)=delete;
private:
    std::string fPath;
    static void Clear(int fd) {
        const int copy=::dup(fd);if(copy<0) return;
        DIR *directory=::fdopendir(copy);if(!directory) {::close(copy);return;}
        while(dirent *entry=::readdir(directory)) {
            const std::string name=entry->d_name;if(name=="." || name=="..") continue;
            struct stat info;if(::fstatat(fd,name.c_str(),&info,AT_SYMLINK_NOFOLLOW)) continue;
            if(S_ISDIR(info.st_mode)) {
                const int child=::openat(fd,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
                if(child>=0) {Clear(child);::close(child);::unlinkat(fd,name.c_str(),AT_REMOVEDIR);}
            } else ::unlinkat(fd,name.c_str(),0);
        }
        ::closedir(directory);
    }
};
#endif
