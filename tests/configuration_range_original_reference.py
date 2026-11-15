#!/usr/bin/env python3
"""编译收到的旧比较/包含/等价函数体作独立参考；不运行旧Spirit字符串解析器。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def body(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    directory = args.output_dir or Path(tempfile.mkdtemp(prefix='getdaq-range-reference-'))
    directory.mkdir(parents=True, exist_ok=True)
    old = root / 'source/cconfig/CompoundConfig-release-20210208/src/CCfg'
    selections = {
        'Value.cpp': ['bool Value::operator==(const Value& rvalue) const', 'bool Value::operator<(const Value& rvalue) const'],
        'Interval.cpp': ['bool Interval::contains(const Value& value) const', 'bool Interval::equals(const Range& rvalue) const'],
        'Enum.cpp': ['bool Enum::contains(const Value& value) const', 'bool Enum::equals(const Range& rvalue) const', 'void Enum::checkTypeHomogeneity(const Value& v)'],
    }
    functions = []
    hashes = {}
    for name, signatures in selections.items():
        source = (old / name).read_text()
        hashes[str((old / name).relative_to(root))] = hashlib.sha256((old / name).read_bytes()).hexdigest()
        functions += [body(source, signature) for signature in signatures]
    scaffold = r'''
#include "configurationrange.h"
#include <set>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cassert>
#include <climits>
namespace CCfg {
enum ValueTypeId { INTEGER, HEXADECIMAL, REAL, BOOLEAN, STRING };
struct Value {
    ValueTypeId valueType; int intValue=0; unsigned hexValue=0; double realValue=0; bool boolValue=false; std::string textValue;
    explicit Value(int x):valueType(INTEGER),intValue(x) {}
    explicit Value(unsigned x):valueType(HEXADECIMAL),hexValue(x) {}
    explicit Value(double x):valueType(REAL),realValue(x) {}
    explicit Value(bool x):valueType(BOOLEAN),boolValue(x) {}
    explicit Value(const std::string &x):valueType(STRING),textValue(x) {}
    ValueTypeId getType() const { return valueType; }
    bool operator==(const Value &rvalue) const;
    bool operator<(const Value &rvalue) const;
    friend bool operator<=(const Value &a,const Value &b) { return !(b<a); }
    struct IncompatibleTypes:std::invalid_argument { IncompatibleTypes(const Value &,const Value &):std::invalid_argument("types differ") {} };
};
struct Range { virtual ~Range() {} virtual bool equals(const Range &) const=0; };
struct Interval:Range {
    enum BoundMode { IncludeBound, ExcludeBound, Unbounded };
    Value lowerBound,upperBound; BoundMode lowerBoundMode,upperBoundMode;
    Interval(Value a,Value b,BoundMode l,BoundMode u):lowerBound(a),upperBound(b),lowerBoundMode(l),upperBoundMode(u) {}
    bool contains(const Value &) const; bool equals(const Range &) const;
};
struct Enum:Range {
    std::set<Value> values;
    const std::set<Value> &getValues() const { return values; }
    bool contains(const Value &) const; bool equals(const Range &) const;
    void checkTypeHomogeneity(const Value &v);
    struct IncompatibleType:std::invalid_argument { IncompatibleType(const Enum &,const Value &):std::invalid_argument("enum types differ") {} };
};
'''
    program = scaffold + '\n'.join(functions) + r'''
}
struct Item { CCfg::Value old; std::string text,type; };
int main() {
    static_assert(sizeof(int)==4 && sizeof(unsigned)==4,"Reference requires original 32-bit domains");
    unsigned checks=0;
    const std::vector<std::vector<Item>> groups={
        {{CCfg::Value(-5),"-5","int"},{CCfg::Value(0),"0","int"},{CCfg::Value(3),"3","int"},{CCfg::Value(INT_MAX),"2147483647","int"}},
        {{CCfg::Value(0u),"0x0","hex"},{CCfg::Value(3u),"0x3","hex"},{CCfg::Value(255u),"0xff","hex"},{CCfg::Value(UINT_MAX),"0xffffffff","hex"}},
        {{CCfg::Value(-1.5),"-1.5","real"},{CCfg::Value(0.0),"0.0","real"},{CCfg::Value(1.5),"1.5","real"},{CCfg::Value(2.0),"2e0","real"}},
        {{CCfg::Value(false),"false","bool"},{CCfg::Value(true),"true","bool"}},
        {{CCfg::Value(std::string("a")),"a","string"},{CCfg::Value(std::string("b")),"b","string"},{CCfg::Value(std::string("z")),"z","string"}},
    };
    const auto literal=[](const Item &x) { return x.type=="string" ? '"'+x.text+'"' : x.text; };
    const auto check=[&](bool ok) { if (!ok) throw std::runtime_error("New/received old function outcome differs"); ++checks; };
    try {
        for (const auto &g:groups) for (const auto &a:g) for (const auto &b:g) for (int l=0;l<3;++l) for (int u=0;u<3;++u) {
            CCfg::Interval old(a.old,b.old,CCfg::Interval::BoundMode(l),CCfg::Interval::BoundMode(u));
            const std::string expression=std::string(l==1 ? "]" : "[")+(l==2 ? "-inf" : literal(a))+','+(u==2 ? "+inf" : literal(b))+(u==1 ? '[' : ']');
            const auto range=GetDAQ::ParseConfigurationRange(expression);
            for (const auto &c:groups) for (const auto &value:c)
                check(old.contains(value.old)==range.Contains(GetDAQ::ParseConfigurationValue(value.text,value.type)));
            check(old.equals(old)==range.Equivalent(GetDAQ::ParseConfigurationRange(" "+expression+" ")));
        }
        for (const auto &g:groups) {
            CCfg::Enum old; std::string expression="{";
            for (const auto &value:g) { old.checkTypeHomogeneity(value.old); old.values.insert(value.old); expression+=(expression.size()>1 ? "," : "")+literal(value); }
            expression+='}'; const auto range=GetDAQ::ParseConfigurationRange(expression);
            for (const auto &c:groups) for (const auto &value:c) {
                bool accepted=false; try { accepted=old.contains(value.old); } catch (const std::invalid_argument &) {}
                check(accepted==range.Contains(GetDAQ::ParseConfigurationValue(value.text,value.type)));
            }
            check(old.equals(old)==range.Equivalent(range));
        }
        std::cout<<"PASS "<<checks<<" comparisons against seven received old Value/Interval/Enum function bodies; original 32-bit domains; old Spirit parser not executed\n";
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
'''
    source = directory / 'reference.cpp'
    source.write_text(program)
    binary = directory / 'reference'
    command = ['g++', '-std=c++11', '-pedantic-errors', '-O2', '-Wall', '-Wextra', '-I' + str(root / 'common/config'), str(source), str(root / 'common/config/configurationrange.cpp'), str(root / 'common/config/configurationrange_parse.cpp'), '-o', str(binary)]
    subprocess.run(command, check=True)
    output = subprocess.check_output([str(binary)], text=True)
    (directory / 'results.txt').write_text(output)
    (directory / 'source-sha256.json').write_text(json.dumps(hashes, indent=2) + '\n')
    (directory / 'commands.json').write_text(json.dumps([command, [str(binary)]], indent=2) + '\n')
    print(output, end='')
    print('Evidence:', directory)


if __name__ == '__main__':
    main()
