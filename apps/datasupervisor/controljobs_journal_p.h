#ifndef GETDAQ_SUPERVISORCONTROLJOURNAL_P_H
#define GETDAQ_SUPERVISORCONTROLJOURNAL_P_H

#include <memory>
#include <string>

namespace GetDAQ {
class SupervisorControlJournal {
public:
    SupervisorControlJournal(const std::string &directory,const std::string &policy);
    ~SupervisorControlJournal();
    SupervisorControlJournal(const SupervisorControlJournal &)=delete;
    SupervisorControlJournal &operator=(const SupervisorControlJournal &)=delete;

    bool Existing() const;
    const std::string &Content() const;
    void Append(const std::string &record);
    void Check() const;

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}

#endif
