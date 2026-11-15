#include "configurationrange.h"
#include <iostream>
#include <stdexcept>
#include <limits>

using GetDAQ::ParseConfigurationValue;
using GetDAQ::ParseConfigurationRange;
namespace {
void Check(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
template<typename F> void Reject(F function) {
    bool rejected=false; try { function(); } catch (const std::invalid_argument &) { rejected=true; }
    Check(rejected,"Malformed configuration accepted");
}
}
int main() {
    try {
        const auto full=ParseConfigurationRange("[-9223372036854775808,9223372036854775807]");
        Check(full.Contains(ParseConfigurationValue("-9223372036854775808","int")),"Signed minimum lost");
        Check(full.Contains(ParseConfigurationValue("9223372036854775807","int")),"Signed maximum lost");
        const auto high=ParseConfigurationRange("[0x20000000000001,0xffffffffffffffff]");
        Check(high.Contains(ParseConfigurationValue("ffffffffffffffff","hex")),"Unsigned maximum lost");
        Check(!high.Contains(ParseConfigurationValue("20000000000000","hex")),"Integer rounded through double");
        Check(ParseConfigurationRange("{9007199254740993}").Contains(ParseConfigurationValue("9007199254740993","int")),"Wide enum value lost");
        Check(!ParseConfigurationRange("{9007199254740993}").Contains(ParseConfigurationValue("9007199254740992","int")),"Wide enum value rounded");
        Check(ParseConfigurationRange("{3,1,2,1}").Equivalent(ParseConfigurationRange("{ 2,3,1 }")),"Enum order/dedup differs");
        Check(ParseConfigurationRange("[+0001,10]").Equivalent(ParseConfigurationRange("[1, 010]")),"Equivalent decimal bounds differ");
        Check(ParseConfigurationRange("[0x0,0xFF]").Equivalent(ParseConfigurationRange("[0x00, 0xff]")),"Equivalent hex bounds differ");
        Check(!ParseConfigurationRange("[0,10]").Equivalent(ParseConfigurationRange("[0.0,10.0]")),"Bound types conflated");
        Check(!ParseConfigurationRange("[0,10]").Contains(ParseConfigurationValue("5.0","real")),"Real silently coerced to integer range");
        Check(!ParseConfigurationRange("{0x10}").Contains(ParseConfigurationValue("16","int")),"Hex/integer types conflated");
        Check(ParseConfigurationRange("]0,10[").Contains(ParseConfigurationValue("5")),"Open interval rejected interior");
        Check(!ParseConfigurationRange("]0,10[").Contains(ParseConfigurationValue("0")),"Open lower bound accepted");
        Check(!ParseConfigurationRange("]0,10[").Contains(ParseConfigurationValue("10")),"Open upper bound accepted");
        Check(ParseConfigurationRange("]-inf,+inf[").Equivalent(ParseConfigurationRange("[-inf,+inf]")),"Unbounded delimiters changed meaning");
        Check(ParseConfigurationRange("[-inf,+inf]").Contains(ParseConfigurationValue("a","string")),"Fully unbounded range rejected string");
        Check(ParseConfigurationRange("{ } ").Equivalent(ParseConfigurationRange("{}")),"Empty enum whitespace differs");
        Check(!ParseConfigurationRange("{}").Contains(ParseConfigurationValue("" ,"string")),"Empty enum became empty string enum");
        Check(ParseConfigurationRange("{\"\"}").Contains(ParseConfigurationValue("","string")),"Empty quoted string rejected");
        Check(ParseConfigurationRange("{\"fast, slow\",\"a\\\"b\",\"a\\nline\",\"\\x41\\101\"}").Contains(ParseConfigurationValue("AA","string")),"C string escapes differ");
        Check(ParseConfigurationRange("{\"fast, slow\",\"a\\\"b\"}").Contains(ParseConfigurationValue("fast, slow","string")),"Quoted comma split into items");
        Check(ParseConfigurationRange("[\"a\",\"z\"]").Contains(ParseConfigurationValue("beta","string")),"String interval rejected");
        Check(ParseConfigurationRange("{TRUE,false}").Contains(ParseConfigurationValue("true","bool")),"Boolean case handling differs");
        Check(ParseConfigurationRange("[1.0,2e0]").Contains(ParseConfigurationValue("1.5","real")),"Real range rejected");
        for (const auto &s:{"9223372036854775808","-9223372036854775809"}) Reject([&] { ParseConfigurationValue(s,"int"); });
        Reject([] { ParseConfigurationValue("10000000000000000","hex"); });
        Reject([] { ParseConfigurationValue("nan","real"); });
        Reject([] { ParseConfigurationValue("1e309","real"); });
        Reject([] { ParseConfigurationValue(std::string(100000,'9'),"int"); });
        for (const auto &s:{"{1,1.0}","{1,}","{1, }","{,}","{\"x\" junk}","{\"bad\\z\"}","{\"open}","[1]","[1,2,3]","[1,2]tail","{fast slow}"})
            Reject([&] { ParseConfigurationRange(s); });
        Reject([] { ParseConfigurationRange(std::string(1024*1024+1,'a')); });
        std::string many="{"; for (int i=0;i<4097;++i) many+=(i ? ",1" : "1"); many+='}';
        Reject([&] { ParseConfigurationRange(many); });
        std::cout<<"PASS typed ranges: exact 64-bit, strict domains, enum/interval equality, quoted escapes and bounded full parsing\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
