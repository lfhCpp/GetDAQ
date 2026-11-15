#include "eccconfigurationselector.h"
#include "fixedeccconfiguration.h"
#include "configurationselection.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
std::string Json(const JsonValue &v) {
    if (v.type==JsonValue::Null) return "null";
    if (v.type==JsonValue::Boolean || v.type==JsonValue::Number) return v.text;
    if (v.type==JsonValue::String) return EncodeJsonString(v.text);
    std::string result=v.type==JsonValue::Array ? "[" : "{";
    if (v.type==JsonValue::Array) {
        for (const auto &item:v.items) {if (result.size()>1) result+=',';result+=Json(item);}
    } else {
        for (const auto &item:v.members) {if (result.size()>1) result+=',';result+=EncodeJsonString(item.first)+':'+Json(item.second);}
    }
    return result+(v.type==JsonValue::Array ? ']' : '}');
}
JsonValue Reference(const std::string &path,const std::string &sha) {
    return ParseStrictJson("{\"path\":"+EncodeJsonString(path)+",\"sha256\":"+EncodeJsonString(sha)+"}");
}
void Seal(const std::string &path) {
    Fd fd(::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    if (fd.value<0 || ::fchmod(fd.value,0400) || ::fsync(fd.value)) throw std::runtime_error("Cannot seal selected ECC input");
}
}
std::string SelectEccConfiguration(const std::string &templateFile,const std::string &selectionFile,const std::string &newDirectory) {
    Path(templateFile);Path(selectionFile);Path(newDirectory);
    const auto templateBytes=ReadFile(templateFile,1024*1024),selectionBytes=ReadFile(selectionFile,65536);
    FixedEccConfiguration original(templateFile);original.Check();
    const auto selected=DecodeConfigurationSelection(selectionBytes);
    const auto selectionSha=ConfigurationSha256(selectionBytes);
    auto candidate=ParseStrictJson(templateBytes);
    candidate.members["configuration_selection"]=Reference(selectionFile,selectionSha);
    candidate.members["configuration_publication"]=Reference(selected.registration.manifestFile,selected.registration.publicationSha256);
    const auto separator=selected.registration.manifestFile.find_last_of('/');
    candidate.members["experiment_artifact"]=Reference(selected.registration.manifestFile.substr(0,separator)+"/experiment.json",selected.registration.artifactSha256);
    // 保留模板审计根。已有历史与新绑定不符时，ECC启动必须拒绝，不能换根绕过UNKNOWN。
    candidate.members["deployment_id"]=ParseStrictJson(EncodeJsonString("selected-"+selectionSha));
    const auto output=Json(candidate)+'\n';
    if (output.size()>1024*1024) throw std::invalid_argument("Selected ECC service input exceeds 1 MiB");
    CreateDirectory(newDirectory);Directory directory(newDirectory);
    WriteNew(newDirectory+"/selection-intent.json","{\"schema\":1,\"template_sha256\":"+EncodeJsonString(ConfigurationSha256(templateBytes))+
        ",\"selection_sha256\":"+EncodeJsonString(selectionSha)+",\"hardware_loaded_verified\":false}\n");
    const auto pending=newDirectory+"/.pending-service.json",final=newDirectory+"/service.json";
    WriteNew(pending,output);Seal(pending);
    FixedEccConfiguration checked(pending);checked.Check();original.Check();
    if (ReadFile(templateFile,1024*1024)!=templateBytes || ReadFile(selectionFile,65536)!=selectionBytes)
        throw std::runtime_error("ECC template/selection changed during review");
    directory.Check();
    if (::link(pending.c_str(),final.c_str()) || ::fsync(directory.Descriptor()) ||
        ::unlink(pending.c_str()) || ::fsync(directory.Descriptor())) throw std::runtime_error("Cannot publish reviewed ECC deployment");
    FixedEccConfiguration published(final);published.Check();
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-selection-result\",\"configuration_file\":"+EncodeJsonString(final)+
        ",\"input_binding_sha256\":"+EncodeJsonString(published.BindingSha256())+",\"selection\":"+published.SelectionJson()+
        ",\"hardware_connected\":false,\"hardware_loaded_verified\":false,\"ecc_state_changed\":false}\n";
}
}
