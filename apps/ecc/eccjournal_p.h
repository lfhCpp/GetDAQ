#ifndef GETDAQ_ECCJOURNAL_P_H
#define GETDAQ_ECCJOURNAL_P_H

#include "eccstatemachine.h"
#include "strictjson.h"
#include <cstddef>
#include <initializer_list>
#include <string>

namespace GetDAQ {
namespace EccJournalInternal {
extern const std::size_t MaxBytes,MaxLine,MaxRecords,MaxOperations;
void TextLimit(const std::string &text);
void IdLimit(const std::string &id);
std::string Quote(const std::string &text);
void Keys(const JsonValue &root,std::initializer_list<const char *> keys);
std::string Text(const JsonValue &node);
unsigned long long Number(const JsonValue &node,bool sequence=false);
EccState State(const JsonValue &node);
}
}

#endif
