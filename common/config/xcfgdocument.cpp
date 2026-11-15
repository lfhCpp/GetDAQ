#include "xcfgdocument.h"
#include "xcfgdocumentinternal_p.h"
#include <QRegularExpression>
#include <QSet>
#include <functional>

using namespace XcfgDocumentInternal;

bool XcfgDocument::ChildIndex(int parent,QMap<QString,int> *children,QString *error) const
{
    if (fChildIndexErrors.contains(parent)) return Fail(error,fChildIndexErrors.value(parent));
    if (fChildIndices.contains(parent)) { *children=fChildIndices.value(parent); error->clear(); return true; }
    QMap<QString,int> index;
    for (int content=parent;content>=0;content=fNodes[content].includedRoot) for (int child:fNodes[content].children) {
        const auto &node=fNodes[child]; const QString key=node.name+'['+node.attributes.value("id")+']';
        QString problem;
        if (index.contains(key)) problem="Duplicate declaration/include collision at "+key;
        else if (++fChildIndexEntries>2000000) problem="Configuration child index limit exceeded";
        if (!problem.isEmpty()) { fChildIndexErrors[parent]=problem; return Fail(error,problem); }
        index[key]=child;
    }
    fChildIndices[parent]=index; *children=index; error->clear(); return true;
}

int XcfgDocument::Lookup(const QString &path, QString *error) const
{
    return LookupImpl(path,nullptr,error);
}
QStringList XcfgDocument::SplitPath(const QString &path,QString *error)
{
    return PathParts(path,error);
}
int XcfgDocument::LookupImpl(const QString &path,bool *inherited,QString *error) const
{
    if(inherited)*inherited=false;
    auto fail = [error](const QString &message) { *error = message; return -1; };
    if (!fSources.contains(fRootFile)) return fail("No configuration loaded");
    const QStringList parts = PathParts(path, error);
    if (!error->isEmpty()) return -1;
    if (parts.isEmpty() || parts.size() > 128) return fail("Use a root/Element[id]/parameter path");
    const int root = fSources.value(fRootFile).root;
    const QString rootName = fNodes[root].name;
    if (parts[0] != rootName && parts[0] != fNodes[root].path) return fail("Path must begin with " + fNodes[root].path);
    const QRegularExpression segment("^([^/\\[\\]]+)(?:\\[([^\\[\\]]*)\\])?$");
    QVector<QPair<QString, QString>> query;
    for (int level = 1; level < parts.size(); ++level) {
        const auto match = segment.match(parts[level]);
        if (!match.hasMatch()) return fail("Invalid path segment: " + parts[level]);
        query.append(qMakePair(match.captured(1), match.captured(2)));
    }
    // 保留旧 Search 的“已访问的实例深度”语义，不能先把所有层的默认值做字典合并。
    // 一条支路尚未发现某层具体实例时，不允许直接拿该层 '*' 创造虚拟实例。
    int reached = -1, visits = 0; QString problem;
    std::function<int(int, int)> visit = [&](int parent, int level) -> int {
        if (level == query.size()) return parent;
        if (++visits > 200000) { problem = "Lookup branch limit exceeded"; return -1; }
        QMap<QString, int> lookup;
        if (!ChildIndex(parent,&lookup,&problem)) return -1;
        const QString name = query[level].first, id = query[level].second;
        for (int pass = 0; pass < (id == "*" ? 1 : 2); ++pass) {
            if (pass == 1 && reached < level) continue;
            const QString key = name + '[' + (pass == 0 ? id : "*") + ']';
            if (!lookup.contains(key)) continue;
            reached = qMax(reached, level);
            const int result = visit(lookup.value(key), level + 1);
            if (result >= 0) {if(pass==1 && inherited)*inherited=true;return result;}
            if (!problem.isEmpty()) return result;
        }
        return -1;
    };
    const int result = visit(root, 0);
    if (result < 0) return fail(problem.isEmpty() ? "No declared instance/value for " + path : problem);
    error->clear(); return result;
}

QStringList XcfgDocument::Children(const QString &path, const QString &name, QString *error) const
{
    if (Lookup(path, error) < 0) return {};
    const QStringList parts = PathParts(path, error);
    if (!error->isEmpty()) return {};
    const QRegularExpression segment("^([^/\\[\\]]+)(?:\\[([^\\[\\]]*)\\])?$");
    QVector<QPair<QString, QString>> query;
    for (int i = 1; i < parts.size(); ++i) {
        const auto match = segment.match(parts[i]);
        query.append(qMakePair(match.captured(1), match.captured(2)));
    }
    QSet<QString> candidates; int reached = -1, visits = 0;
    QString problem;
    std::function<void(int, int)> visit = [&](int parent, int level) {
        if (++visits > 200000) { problem = "Child enumeration branch limit exceeded"; return; }
        QMap<QString, int> children;
        if (!ChildIndex(parent,&children,&problem)) return;
        if (level == query.size()) {
            for (int child : children) {
                const auto &node = fNodes[child];
                if (node.name != name) continue;
                const QString id = node.attributes.value("id");
                if (id.isEmpty() || id.contains(QRegularExpression("[\\[\\]]"))) {
                    problem = "Invalid or missing object ID at " + node.path; return;
                }
                if (id != "*") candidates.insert(path + '/' + name + '[' + id + ']');
            }
            return;
        }
        const QString tag = query[level].first, id = query[level].second;
        for (int pass = 0; pass < (id == "*" ? 1 : 2); ++pass) {
            if (pass == 1 && reached < level) continue;
            const QString key = tag + '[' + (pass ? "*" : id) + ']';
            if (!children.contains(key)) continue;
            reached = qMax(reached, level);
            visit(children.value(key), level + 1);
            if (!problem.isEmpty()) return;
        }
    };
    visit(fSources.value(fRootFile).root, 0);
    if (!problem.isEmpty()) { *error = problem; return {}; }
    QStringList result;
    for (const QString &candidate : candidates) {
        if (Lookup(candidate, error) < 0) return {};
        result.append(candidate);
    }
    result.sort(); error->clear(); return result;
}

