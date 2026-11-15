#include "experimentreader.h"
#include "effectiveconfigurationsnapshot.h"
#include "xcfgstructureeditor.h"
#include "xcfgedithistory.h"
#include "xcfgexporter.h"
#include "xcfgbinary.h"
#include "xcfgworkspacecopy.h"
#include "xcfgworkspace.h"
#include "xcfgsavejournal.h"
#include "xcfgsourcemigration.h"
#include "strictjson.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

class TestXcfgShared : public QObject {
    Q_OBJECT
    static bool Write(const QString &path,const QByteArray &bytes) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size();
    }
private slots:
    void sourcePathReferrerDiscoveryFindsTransitiveRootsReadOnly() {
        QTemporaryDir directory;QString error;
        const auto root=directory.filePath("root.xcfg"),wrapper=directory.filePath("wrapper.xml");
        const auto shared=directory.filePath("shared.xcfg"),source=directory.filePath("source.xcfg");
        QVERIFY(Write(root,"<Setup><Shared include=\"shared.xcfg\"/></Setup>"));
        QVERIFY(Write(wrapper,"<Wrapper><Root include=\"root.xcfg\"/></Wrapper>"));
        QVERIFY(Write(shared,"<Shared><Source include=\"source.xcfg\"/></Shared>"));
        QVERIFY(Write(source,"<Source><n type=\"int\">2</n></Source>"));
        QJsonObject report;QVERIFY2(XcfgSourceMigration::DiscoverReferrers(directory.path(),source,&report,&error),qPrintable(error));
        QVERIFY(report.value("complete").toBool());QVERIFY(report.value("read_only").toBool());QVERIFY(!report.value("disk_written").toBool());
        QCOMPARE(report.value("referrers").toArray().size(),3);QCOMPARE(report.value("candidate_roots").toArray().size(),1);
        QCOMPARE(report.value("candidate_roots").toArray().first().toString(),QFileInfo(wrapper).canonicalFilePath());
        QVERIFY(QFileInfo::exists(source));QCOMPARE(QFileInfo(source).size(),qint64(QString("<Source><n type=\"int\">2</n></Source>").toUtf8().size()));
    }
    void sourcePathReferrerDiscoveryMarksExternalAndMalformedReferencesIncomplete() {
        QTemporaryDir directory;QString error;const auto source=directory.filePath("source.xcfg"),root=directory.filePath("root.xcfg");
        QVERIFY(Write(source,"<Source/>"));QVERIFY(Write(root,"<Setup><Other include=\"/outside/not-scanned.xcfg\"/></Setup>"));
        QJsonObject report;QVERIFY2(XcfgSourceMigration::DiscoverReferrers(directory.path(),source,&report,&error),qPrintable(error));
        QVERIFY(!report.value("complete").toBool());QVERIFY(!report.value("issues").toArray().isEmpty());
        QVERIFY(Write(root,"<Setup>"));QVERIFY2(XcfgSourceMigration::DiscoverReferrers(directory.path(),source,&report,&error),qPrintable(error));
        QVERIFY(!report.value("complete").toBool());
    }
    void hardwareCompilerRejectsUnknownLegacyScalarAttributes() {
        QTemporaryDir directory; QString error;
        const auto validPath=directory.filePath("valid.xcfg");
        QVERIFY(Write(validPath,"<Setup><gain id=\"0\" type=\"int\" unit=\"mV\" range=\"[0,10]\">5</gain></Setup>"));
        XcfgDocument valid; QVERIFY2(valid.Load(validPath,&error),qPrintable(error));
        ExperimentReader validReader(valid);
        QCOMPARE(validReader.Scalar("Setup/gain[0]",QString(),"mV"),QString("5"));

        const QList<QPair<QString,QByteArray>> unsupported={
            {"vendor", "<Setup><gain vendor=\"extension\">5</gain></Setup>"},
            {"class", "<Setup><gain class=\"extension\">5</gain></Setup>"},
            {"custom", "<Setup custom=\"extension\"><gain>5</gain></Setup>"}
        };
        for (const auto &entry:unsupported) {
            const auto &attribute=entry.first;
            const auto path=directory.filePath(attribute+".xcfg");
            QVERIFY(Write(path,entry.second));
            XcfgDocument document; QVERIFY2(document.Load(path,&error),qPrintable(error));
            bool rejected=false;
            try { ExperimentReader reader(document); reader.Scalar("Setup/gain"); }
            catch (const std::runtime_error &e) {
                const QString message=QString::fromUtf8(e.what());
                rejected=message.contains(attribute) && message.contains("Unsupported CompoundConfig");
            }
            QVERIFY2(rejected,qPrintable("Hardware parameter compiler accepted unsupported XML attribute: "+attribute));
        }
    }
    void sourcePathMigrationRewritesSharedIncomingAndOutgoingIncludes() {
        QTemporaryDir directory;QString error;QVERIFY(QDir(directory.path()).mkdir("old"));QVERIFY(QDir(directory.path()).mkdir("new"));
        const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg"),old=directory.filePath("old/shared.xcfg"),target=directory.filePath("new/moved.xcfg"),leaf=directory.filePath("old/leaf.xcfg");
        const QByteArray shared="<Shared custom=\"preserve\"><!-- keep --><part include=\"leaf.xcfg\"/><text type=\"string\"><![CDATA[a b]]></text></Shared>\r\n";
        QVERIFY(Write(a,"<A><one include=\"old/shared.xcfg\"/></A>"));QVERIFY(Write(b,"<B><two include=\"old/shared.xcfg\"/></B>"));
        QVERIFY(Write(old,shared));QVERIFY(Write(leaf,"<Leaf><n type=\"int\" unit=\"mV\" range=\"[0,9]\">3</n></Leaf>"));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));XcfgSourceMigration::Request request;request.destinations[old]=target;
        XcfgSourceMigration::Preview preview;QVERIFY2(XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error),qPrintable(error));
        QVERIFY(QFileInfo::exists(old));QVERIFY(!QFileInfo::exists(target));QCOMPARE(preview.report["include_rewrites"].toArray().size(),3);
        QVERIFY(preview.candidate[a].contains("new/moved.xcfg"));QVERIFY(preview.candidate[b].contains("new/moved.xcfg"));
        QVERIFY(preview.candidate[target].contains("../old/leaf.xcfg"));QVERIFY(preview.candidate[target].contains("<!-- keep -->"));
        QVERIFY(preview.candidate[target].contains("<![CDATA[a b]]>"));QVERIFY(preview.candidate[target].endsWith("\r\n"));
        XcfgSourceMigration::Result result;QVERIFY2(XcfgSourceMigration::Apply(&workspace,preview,&result,&error),qPrintable(error));
        QVERIFY(result.complete);QVERIFY(workspace.RequiresReopen());QVERIFY(!QFileInfo::exists(old));QVERIFY(QFileInfo::exists(target));
        XcfgDocument document;QVERIFY2(document.Load(a,&error),qPrintable(error));QCOMPARE(document.Nodes()[document.Lookup("A/one/part/n",&error)].value,QString("3"));
        XcfgSourceMigration::Recovery recovery;QVERIFY(!XcfgSourceMigration::PreviewRecovery(result.recoveryDirectory,true,&recovery,&error));
        QFile unchanged(leaf);QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),preview.original[leaf]);
    }
    void sourcePathMigrationMovesRootAndDependencyTogether() {
        QTemporaryDir directory;QString error;QVERIFY(QDir(directory.path()).mkdir("new"));const auto root=directory.filePath("root.xcfg"),part=directory.filePath("part.xcfg"),newRoot=directory.filePath("new/main.xcfg"),newPart=directory.filePath("new/part.xcfg");
        QVERIFY(Write(root,"<Setup><A include=\"part.xcfg\"/></Setup>"));QVERIFY(Write(part,"<A><n type=\"int\">4</n></A>"));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        XcfgSourceMigration::Request request;request.destinations[root]=newRoot;request.destinations[part]=newPart;XcfgSourceMigration::Preview preview;
        QVERIFY2(XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error),qPrintable(error));QCOMPARE(preview.migratedRoots,QStringList{newRoot});QCOMPARE(preview.candidate[newRoot],preview.original[root]);
        XcfgSourceMigration::Result result;QVERIFY2(XcfgSourceMigration::Apply(&workspace,preview,&result,&error),qPrintable(error));
        QVERIFY(!QFileInfo::exists(root));QVERIFY(!QFileInfo::exists(part));XcfgDocument document;QVERIFY2(document.Load(newRoot,&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/A/n",&error)].value,QString("4"));
    }
    void sourcePathMigrationRejectsDestinationCollisionStaleAndForgedReviews() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("new.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>";
        QVERIFY(Write(root,original));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));XcfgSourceMigration::Request request;request.destinations[root]=target;XcfgSourceMigration::Preview preview;
        QVERIFY(XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));auto forged=preview;forged.candidate[target]="<Other/>";XcfgSourceMigration::Result result;
        QVERIFY(!XcfgSourceMigration::Apply(&workspace,forged,&result,&error));QVERIFY(!workspace.RequiresReopen());QVERIFY(!QFileInfo::exists(target));
        QVERIFY(Write(target,"existing user content"));QVERIFY(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error));QVERIFY(!workspace.RequiresReopen());QVERIFY(QFile::remove(target));
        QVERIFY(Write(root,"<Setup><n type=\"int\">3</n></Setup>"));QVERIFY(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error));QVERIFY(!QFileInfo::exists(target));
    }
    void sourcePathMigrationRequiresSavedWorkspaceAndExistingDestinationParent() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("new.xcfg");QVERIFY(Write(root,"<Setup><n type=\"int\">2</n></Setup>"));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));QVERIFY(workspace.StageSource(root,"<Setup><n type=\"int\">3</n></Setup>",&error));
        XcfgSourceMigration::Request request;request.destinations[root]=target;XcfgSourceMigration::Preview preview;QVERIFY(!XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));
        QVERIFY(workspace.Undo(&error));request.destinations[root]=directory.filePath("absent/new.xcfg");QVERIFY(!XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));
        request.destinations[root]=root;QVERIFY(!XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));
    }
    void sourcePathMigrationRecoveryBothDirections_data() {
        QTest::addColumn<bool>("candidate");QTest::addColumn<QString>("phase");
        for(const auto &phase:QStringList{"created","replaced","removed"})for(bool candidate:{false,true})
            QTest::newRow(qPrintable(phase+(candidate ? "-finish" : "-restore")))<<candidate<<phase;
    }
    void sourcePathMigrationRecoveryBothDirections() {
        QFETCH(bool,candidate);QFETCH(QString,phase);QTemporaryDir directory;QString error;QVERIFY(QDir(directory.path()).mkdir("new"));
        const auto root=directory.filePath("root.xcfg"),part=directory.filePath("part.xcfg"),target=directory.filePath("new/part.xcfg");
        const QByteArray rootBytes="<Setup><A include=\"part.xcfg\"/></Setup>",partBytes="<A><n type=\"int\">2</n></A>";
        QVERIFY(Write(root,rootBytes));QVERIFY(Write(part,partBytes));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));XcfgSourceMigration::Request request;request.destinations[part]=target;
        XcfgSourceMigration::Preview preview;QVERIFY(XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));XcfgSourceMigration::Result result;
        bool reached=false;
        QVERIFY(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error,[&](const QString &step,const QString &){if(step==phase){reached=true;throw std::runtime_error("owned interruption");}}));
        QVERIFY2(reached,qPrintable(error));QCOMPARE(error,QString("owned interruption"));
        QVERIFY(workspace.RequiresReopen());QVERIFY(!result.recoveryDirectory.isEmpty());XcfgDocument blocked;QVERIFY(!blocked.Load(root,&error));QVERIFY(error.contains(result.recoveryDirectory));
        XcfgSourceMigration::Recovery recovery;QVERIFY2(XcfgSourceMigration::PreviewRecovery(result.recoveryDirectory,candidate,&recovery,&error),qPrintable(error));
        auto forged=recovery;forged.target[root]="<Other/>";QVERIFY(!XcfgSourceMigration::ApplyRecovery(forged,&error));
        QVERIFY2(XcfgSourceMigration::ApplyRecovery(recovery,&error),qPrintable(error));QVERIFY(blocked.Load(root,&error));
        QCOMPARE(QFileInfo::exists(part),!candidate);QCOMPARE(QFileInfo::exists(target),candidate);
        QFile file(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),candidate ? preview.candidate[root] : rootBytes);
        QVERIFY(!XcfgSourceMigration::ApplyRecovery(recovery,&error));
    }
    void sourcePathMigrationRecoveryRefusesThirdPartyChangeAndPayloadCorruption() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("new.xcfg");QVERIFY(Write(root,"<Setup/>"));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));XcfgSourceMigration::Request request;request.destinations[root]=target;XcfgSourceMigration::Preview preview;QVERIFY(XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error));
        XcfgSourceMigration::Result result;QVERIFY(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error,[](const QString &phase,const QString &){if(phase=="created")throw std::runtime_error("owned interruption");}));
        QVERIFY(Write(target,"<Unexpected/>"));XcfgSourceMigration::Recovery recovery;QVERIFY(!XcfgSourceMigration::PreviewRecovery(result.recoveryDirectory,true,&recovery,&error));
        QVERIFY(Write(target,"<Setup/>"));QVERIFY(XcfgSourceMigration::PreviewRecovery(result.recoveryDirectory,true,&recovery,&error));
        const auto files=QDir(result.recoveryDirectory+"/candidate").entryList({"*.xcfg"},QDir::Files);QVERIFY(!files.isEmpty());
        QVERIFY(Write(result.recoveryDirectory+"/candidate/"+files.first(),"<Corrupt/>"));QVERIFY(!XcfgSourceMigration::ApplyRecovery(recovery,&error));
    }
    void binaryAllTypesAndRangeRoundTrip() {
        const QByteArray xml="<Setup><i type=\"int\" range=\"]-5,10[\">-3</i><h type=\"hex\" range=\"{0x0,0xffffffff}\">0xffffffff</h><r type=\"real\" range=\"[-inf,+inf]\">1.25</r><b type=\"bool\" range=\"{false,true}\">true</b><s type=\"string\" unit=\"字节\" range=\"{&quot;  a\\nb  &quot;,&quot;x&quot;}\">&#32;&#32;a&#10;b&#32;&#32;</s><node id=\"*\"><n type=\"int\">1</n></node><node id=\"1\"><n type=\"int\">2</n></node></Setup>";
        QByteArray binary,decoded;QString error;QVERIFY2(XcfgBinary::Encode(xml,&binary,&error),qPrintable(error));QVERIFY(binary.startsWith(QByteArray::fromHex("0001")));QVERIFY2(XcfgBinary::Decode(binary,&decoded,&error),qPrintable(error));
        QTemporaryDir directory;const auto path=directory.filePath("decoded.xcfg");QVERIFY(Write(path,decoded));XcfgDocument doc;QVERIFY(doc.Load(path,&error));XcfgDocument::Node n;XcfgDocument::ScalarProperties properties;
        QVERIFY(doc.ResolveScalar("Setup/s",&n,&properties,&error));QCOMPARE(n.value,QString("  a\nb  "));QVERIFY(doc.ResolveScalar("Setup/i",&n,&properties,&error));QCOMPARE(n.value,QString("-3"));
    }
    void binaryRejectsEveryTruncatedPrefixAndTrailingBytes() {
        QByteArray binary,xml;QString error;QVERIFY(XcfgBinary::Encode("<Setup><n type=\"int\">-2147483648</n></Setup>",&binary,&error));
        for(int size=0;size<binary.size();++size)QVERIFY2(!XcfgBinary::Decode(binary.left(size),&xml,&error),qPrintable(QString::number(size)));
        QVERIFY(!XcfgBinary::Decode(binary+QByteArray(1,'x'),&xml,&error));binary[1]=2;QVERIFY(!XcfgBinary::Decode(binary,&xml,&error));
    }
    void binaryRejectsOverflowAndUnrepresentableMetadata_data() {
        QTest::addColumn<QByteArray>("xml");
        QTest::newRow("int64")<<QByteArray("<Setup><n type=\"int\">2147483648</n></Setup>");
        QTest::newRow("hex64")<<QByteArray("<Setup><n type=\"hex\">0x100000000</n></Setup>");
        QTest::newRow("extra-attribute")<<QByteArray("<Setup comment=\"important\"><n type=\"int\">2</n></Setup>");
        QTest::newRow("range-overflow")<<QByteArray("<Setup><n type=\"int\" range=\"[-2147483649,2]\">1</n></Setup>");
    }
    void binaryRejectsOverflowAndUnrepresentableMetadata() {QFETCH(QByteArray,xml);QString error;QByteArray binary;QVERIFY(!XcfgBinary::Encode(xml,&binary,&error));}
    void binaryMissingSourcesHaveExplicitErrors() {
        QTemporaryDir directory;QString error;XcfgBinary::Preview preview;
        QVERIFY(!XcfgBinary::PreviewImport(directory.filePath("absent.bcfg"),directory.filePath("copy.xcfg"),&preview,&error));QVERIFY(error.contains("does not exist"));
        XcfgDocument empty;XcfgExporter::Request request;request.destination=directory.filePath("copy.bcfg");request.path="Setup";
        QVERIFY(!XcfgBinary::PreviewExport(empty,request,&preview,&error));QVERIFY(error.contains("Open an XML"));
    }
    void binaryMalformedTablesAndValuesAreBounded() {
        QString error;QByteArray binary,decoded;
        for(const auto &type:QStringList{"hex","real","bool"}) {
            const auto xml=("<Setup><n type=\""+type+"\">"+(type=="bool" ? QString("true") : QString("1"))+"</n></Setup>").toUtf8();QVERIFY(XcfgBinary::Encode(xml,&binary,&error));
            auto bad=binary;if(type=="bool")bad[bad.size()-1]=2;
            else if(type=="hex")bad.replace(bad.size()-8,8,QByteArray::fromHex("0000000100000000"));
            else bad.replace(bad.size()-8,8,QByteArray::fromHex("7ff0000000000000"));
            QVERIFY(!XcfgBinary::Decode(bad,&decoded,&error));
        }
        QVERIFY(XcfgBinary::Encode("<Setup><n type=\"int\">2</n></Setup>",&binary,&error));auto bad=binary;bad.replace(2,4,QByteArray::fromHex("ffffffff"));QVERIFY(!XcfgBinary::Decode(bad,&decoded,&error));
        quint32 random=0x31415926;for(int iteration=0;iteration<512;++iteration) {bad=binary;random=random*1664525+1013904223;const int at=int(random%quint32(bad.size()));random=random*1664525+1013904223;bad[at]=char(random>>24);XcfgBinary::Decode(bad,&decoded,&error);}
    }
    void binarySaveRechecksSourceAndRefusesOutputOverwrite() {
        QTemporaryDir directory;QString error;const auto source=directory.filePath("source.xcfg"),dest=directory.filePath("copy.bcfg");QVERIFY(Write(source,"<Setup><n type=\"int\">2</n></Setup>"));XcfgDocument doc;QVERIFY(doc.Load(source,&error));XcfgExporter::Request request;request.path="Setup";request.destination=dest;XcfgBinary::Preview preview;
        QVERIFY2(XcfgBinary::PreviewExport(doc,request,&preview,&error),qPrintable(error));auto tampered=preview;tampered.bytes+='x';QVERIFY(!XcfgBinary::Save(tampered,&error));QVERIFY(!QFileInfo::exists(dest));
        QVERIFY(Write(source,"<Setup><n type=\"int\">3</n></Setup>"));QVERIFY(!XcfgBinary::Save(preview,&error));QVERIFY(!QFileInfo::exists(dest));QVERIFY(doc.Load(source,&error));QVERIFY(XcfgBinary::PreviewExport(doc,request,&preview,&error));QVERIFY(XcfgBinary::Save(preview,&error));QVERIFY(!XcfgBinary::Save(preview,&error));
        XcfgBinary::Preview imported;const auto xml=directory.filePath("imported.xcfg");QVERIFY(XcfgBinary::PreviewImport(dest,xml,&imported,&error));QVERIFY(XcfgBinary::Save(imported,&error));QVERIFY(!XcfgBinary::Save(imported,&error));QVERIFY(doc.Load(xml,&error));QCOMPARE(doc.Nodes()[1].value,QString("3"));
    }

    void standaloneEditorRefusesPendingWorkspaceSourceWrite() {
        QTemporaryDir directory;QString error;const auto path=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",candidate="<Setup><n type=\"int\">3</n></Setup>";QVERIFY(Write(path,original));XcfgDocument standalone;QVERIFY(standalone.Load(path,&error));QMap<QString,QByteArray> before,after;before[path]=original;after[path]=candidate;QString record;{XcfgSaveJournal journal;QVERIFY(journal.Begin({path},before,after,&error));record=journal.Directory();QVERIFY(!standalone.SetValue(1,"4",&error));QVERIFY(error.contains("unfinished Save All"));}
        XcfgSaveJournal::Recovery preview;QVERIFY(XcfgSaveJournal::PreviewRecovery(record,false,&preview,&error));QVERIFY(XcfgSaveJournal::ApplyRecovery(preview,&error));QVERIFY(standalone.SetValue(1,"4",&error));
    }
    void workspaceSaveObserverFailureRetainsRecoverableMixedCommit() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",candidate="<Setup><n type=\"int\">3</n></Setup>";QVERIFY(Write(a,original));QVERIFY(Write(b,original));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));QMap<QString,QByteArray> changes;changes[a]=candidate;changes[b]=candidate;QVERIFY(workspace.StageSources(changes,&error));XcfgWorkspace::Preview review;XcfgWorkspace::SaveResult result;QVERIFY(workspace.CreatePreview(&review,&error));
        QVERIFY(!workspace.SaveAll(review,&result,&error,[](const QString &,const QString &){throw std::runtime_error("fixture interruption after synced file");}));QVERIFY(workspace.RequiresReopen());QCOMPARE(result.savedFiles.size(),1);QCOMPARE(result.remainingFiles.size(),1);QVERIFY(!workspace.Undo(&error));XcfgSaveJournal::Recovery recovery;QVERIFY(XcfgSaveJournal::PreviewRecovery(result.recoveryDirectory,true,&recovery,&error));QVERIFY(XcfgSaveJournal::ApplyRecovery(recovery,&error));XcfgWorkspace reopened;QVERIFY(reopened.Open(a,&error));QVERIFY(reopened.Open(b,&error));QCOMPARE(reopened.Bytes(a),candidate);QCOMPARE(reopened.Bytes(b),candidate);
    }
    void workspaceRecoveryRetentionLimitRefusesBeforeSourceWrite() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),base=directory.filePath(".getdaq-workspace-recovery");const QByteArray original="<Setup/>",candidate="<Setup><n>2</n></Setup>";QVERIFY(Write(root,original));QVERIFY(QDir().mkdir(base));QVERIFY(QFile::setPermissions(base,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));for(int i=0;i<256;++i)QVERIFY(QDir(base).mkdir(QString("save-%1").arg(i)));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));QVERIFY(workspace.StageSource(root,candidate,&error));XcfgWorkspace::Preview review;XcfgWorkspace::SaveResult result;QVERIFY(workspace.CreatePreview(&review,&error));QVERIFY(!workspace.SaveAll(review,&result,&error));QVERIFY(error.contains("limit"));QVERIFY(!workspace.RequiresReopen());QVERIFY(result.savedFiles.isEmpty());QFile file(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void workspaceSaveAllKeepsDurableOriginalAndCandidateAndRetiresMarkers() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",changed="<Setup><n type=\"int\">3</n></Setup>";
        QVERIFY(Write(root,original));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));QVERIFY(workspace.StageSource(root,changed,&error));XcfgWorkspace::Preview preview;XcfgWorkspace::SaveResult result;QVERIFY(workspace.CreatePreview(&preview,&error));QVERIFY2(workspace.SaveAll(preview,&result,&error),qPrintable(error));QVERIFY(!result.recoveryDirectory.isEmpty());QVERIFY(QFileInfo::exists(result.recoveryDirectory+"/original/0.xcfg"));QVERIFY(QFileInfo::exists(result.recoveryDirectory+"/candidate/0.xcfg"));QVERIFY(QFileInfo::exists(result.recoveryDirectory+"/complete.json"));QString pending;QVERIFY(XcfgSaveJournal::Pending(root,&pending,&error));QVERIFY(pending.isEmpty());XcfgWorkspace reopened;QVERIFY(reopened.Open(root,&error));XcfgSaveJournal::Recovery recovery;QVERIFY(!XcfgSaveJournal::PreviewRecovery(result.recoveryDirectory,true,&recovery,&error));
    }
    void workspaceRecoveryFinishesMixedSourcesAfterWriterExit_data(){QTest::addColumn<bool>("candidate");QTest::newRow("finish-candidate")<<true;QTest::newRow("restore-original")<<false;}
    void workspaceRecoveryFinishesMixedSourcesAfterWriterExit(){
        QFETCH(bool,candidate);QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",changed="<Setup><n type=\"int\">3</n></Setup>";QVERIFY(Write(a,original));QVERIFY(Write(b,original));QMap<QString,QByteArray> before,after;before[a]=original;before[b]=original;after[a]=changed;after[b]=changed;QString record;
        {XcfgSaveJournal writer;QVERIFY(writer.Begin({a,b},before,after,&error));record=writer.Directory();XcfgSaveJournal::Recovery busy;QVERIFY(!XcfgSaveJournal::PreviewRecovery(record,candidate,&busy,&error));QVERIFY(Write(a,changed));}
        XcfgWorkspace cold;QVERIFY(!cold.Open(a,&error));QVERIFY(error.contains(record));XcfgSaveJournal::Recovery preview;QVERIFY2(XcfgSaveJournal::PreviewRecovery(record,candidate,&preview,&error),qPrintable(error));QCOMPARE(preview.observed[a],changed);QCOMPARE(preview.observed[b],original);QVERIFY2(XcfgSaveJournal::ApplyRecovery(preview,&error),qPrintable(error));QVERIFY(cold.Open(a,&error));QVERIFY(cold.Open(b,&error));QCOMPARE(cold.Bytes(a),candidate ? changed : original);QCOMPARE(cold.Bytes(b),candidate ? changed : original);QVERIFY(!XcfgSaveJournal::ApplyRecovery(preview,&error));
    }
    void workspaceRecoveryRejectsChangedSourcesAndTamperedReview(){
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",changed="<Setup><n type=\"int\">3</n></Setup>";QVERIFY(Write(root,original));QMap<QString,QByteArray> before,after;before[root]=original;after[root]=changed;QString record;{XcfgSaveJournal writer;QVERIFY(writer.Begin({root},before,after,&error));record=writer.Directory();}
        XcfgSaveJournal::Recovery preview;QVERIFY(XcfgSaveJournal::PreviewRecovery(record,true,&preview,&error));auto forged=preview;forged.target[root]="<Other/>";QVERIFY(!XcfgSaveJournal::ApplyRecovery(forged,&error));QVERIFY(Write(root,"<Unexpected/>"));QVERIFY(!XcfgSaveJournal::ApplyRecovery(preview,&error));QVERIFY(!XcfgSaveJournal::PreviewRecovery(record,true,&preview,&error));QFile file(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("<Unexpected/>"));
    }
    void workspaceRecoveryRejectsCorruptPayload(){
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");const QByteArray original="<Setup/>",changed="<Setup><n>2</n></Setup>";QVERIFY(Write(root,original));QMap<QString,QByteArray> before,after;before[root]=original;after[root]=changed;QString record;{XcfgSaveJournal writer;QVERIFY(writer.Begin({root},before,after,&error));record=writer.Directory();}QVERIFY(Write(record+"/candidate/0.xcfg","<Corrupt/>"));XcfgSaveJournal::Recovery preview;QVERIFY(!XcfgSaveJournal::PreviewRecovery(record,true,&preview,&error));QFile file(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void workspaceCrossFileScalarMovePreservesEncodedWhitespaceAndTargetChildren() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray source="<Left><text type=\"string\">&#x20;hi&#x20;</text></Left>",target="<Right><Target><!-- keep --><old type=\"int\">1</old></Target></Right>";
        QVERIFY(Write(a,source));QVERIFY(Write(b,target));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));XcfgDocument from,to;QVERIFY(workspace.Document(a,&from,&error));QVERIFY(workspace.Document(b,&to,&error));XcfgWorkspace::MoveRequest request;request.sourceRoot=a;request.targetRoot=b;request.sourceNode=from.Lookup("Left/text",&error);request.targetParent=to.Lookup("Right/Target",&error);XcfgWorkspace::MovePreview preview;
        QVERIFY(workspace.PreviewMove(request,&preview,&error));QVERIFY(workspace.StageMove(preview,&error));QVERIFY(workspace.Bytes(b).contains("<!-- keep -->"));QVERIFY(workspace.Bytes(b).contains("&#x20;hi&#x20;"));QVERIFY(workspace.Document(b,&to,&error));XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;QVERIFY(to.ResolveScalar("Right/Target/text",&value,&properties,&error));QCOMPARE(value.value,QString(" hi "));QVERIFY(to.ResolveScalar("Right/Target/old",&value,&properties,&error));QCOMPARE(value.value,QString("1"));QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(a),source);QCOMPARE(workspace.Bytes(b),target);
    }
    void workspaceCrossFileMoveRewritesIncludesAndGroupsBothSources() {
        QTemporaryDir directory;QString error;QVERIFY(QDir().mkpath(directory.filePath("source/sub")));QVERIFY(QDir().mkpath(directory.filePath("target")));
        const auto a=directory.filePath("source/main.xcfg"),b=directory.filePath("target/main.xcfg"),meta=directory.filePath("source/sub/元&值.xcfg"),other=directory.filePath("source/sub/other.xcfg"),mirror=directory.filePath("mirror.xcfg");
        const QByteArray original=QString("<Left><Thing id=\"3\" note=\"include=&quot;ignored&quot;\" include=\"sub/元&amp;值.xcfg\"><Nested include=\"sub/other.xcfg\"/><!-- preserve --></Thing></Left>\r\n").toUtf8(),target="<Right><Target id=\"0\"/></Right>\r\n",defaults="<Meta><n type=\"int\">2</n></Meta>";
        QVERIFY(Write(a,original));QVERIFY(Write(b,target));QVERIFY(Write(meta,defaults));QVERIFY(Write(other,"<Other><v type=\"string\">text</v></Other>"));QVERIFY(Write(mirror,"<Mirror><Use include=\"source/main.xcfg\"/></Mirror>"));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));QVERIFY(workspace.Open(mirror,&error));auto staged=defaults;staged.replace(">2<",">7<");QVERIFY(workspace.StageSource(meta,staged,&error));
        XcfgDocument from,to;QVERIFY(workspace.Document(a,&from,&error));QVERIFY(workspace.Document(b,&to,&error));XcfgWorkspace::MoveRequest request;request.sourceRoot=a;request.targetRoot=b;request.sourceNode=from.Lookup("Left/Thing[3]",&error);request.targetParent=to.Lookup("Right/Target[0]",&error);
        XcfgWorkspace::MovePreview preview;QVERIFY2(workspace.PreviewMove(request,&preview,&error),qPrintable(error));QCOMPARE(preview.before.size(),2);QCOMPARE(preview.report.value("include_rewrites").toArray().size(),2);QCOMPARE(preview.workspace.reports.size(),3);QCOMPARE(workspace.HistoryEntries(),1);
        QVERIFY(workspace.StageMove(preview,&error));QCOMPARE(workspace.HistoryEntries(),2);QVERIFY(!workspace.Bytes(a).contains("<Thing"));QVERIFY(workspace.Bytes(b).contains("../source/sub/"));QVERIFY(workspace.Bytes(b).contains("<!-- preserve -->"));
        QVERIFY(workspace.Document(b,&to,&error));XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;QVERIFY(to.ResolveScalar("Right/Target[0]/Thing[3]/n",&value,&properties,&error));QCOMPARE(value.value,QString("7"));QVERIFY(to.ResolveScalar("Right/Target[0]/Thing[3]/Nested/v",&value,&properties,&error));QCOMPARE(value.value,QString("text"));
        QFile disk(a);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);disk.close();QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(a),original);QCOMPARE(workspace.Bytes(b),target);QCOMPARE(workspace.Bytes(meta),staged);QVERIFY(workspace.Redo(&error));QCOMPARE(workspace.Bytes(b),preview.after.value(b));
        XcfgWorkspace::Preview reviewed;XcfgWorkspace::SaveResult saved;QVERIFY(workspace.CreatePreview(&reviewed,&error));QVERIFY(workspace.SaveAll(reviewed,&saved,&error));QCOMPARE(saved.savedFiles.size(),3);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(a),original);QCOMPARE(workspace.Bytes(b),target);QVERIFY(disk.open(QIODevice::ReadOnly));QVERIFY(!disk.readAll().contains("Thing"));QVERIFY(!workspace.StageMove(preview,&error));
    }
    void workspaceCrossFileMoveDenialsAreAtomic() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray from="<Left><Item id=\"0\"/><Other/></Left>",to="<Right><Target><Item id=\"0\"/></Target><s type=\"int\">2</s><Empty/></Right>";
        QVERIFY(Write(a,from));QVERIFY(Write(b,to));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));XcfgDocument source,target;QVERIFY(workspace.Document(a,&source,&error));QVERIFY(workspace.Document(b,&target,&error));XcfgWorkspace::MoveRequest request;request.sourceRoot=a;request.targetRoot=b;request.sourceNode=source.Lookup("Left/Item[0]",&error);request.targetParent=target.Lookup("Right/Target",&error);XcfgWorkspace::MovePreview preview;
        QVERIFY(!workspace.PreviewMove(request,&preview,&error));request.targetParent=target.Lookup("Right/s",&error);QVERIFY(!workspace.PreviewMove(request,&preview,&error));request.sourceNode=0;request.targetParent=0;QVERIFY(!workspace.PreviewMove(request,&preview,&error));
        request.sourceNode=source.Lookup("Left/Item[0]",&error);request.targetRoot=a;request.targetParent=0;QVERIFY(!workspace.PreviewMove(request,&preview,&error));request.targetRoot=b;request.targetParent=target.Lookup("Right/Empty",&error);QVERIFY(workspace.PreviewMove(request,&preview,&error));
        auto tampered=preview;tampered.after[b]="<Forged/>";QVERIFY(!workspace.StageMove(tampered,&error));QCOMPARE(workspace.HistoryEntries(),0);QCOMPARE(workspace.Bytes(a),from);QCOMPARE(workspace.Bytes(b),to);
        auto edited=from;edited.replace("<Other/>","<Changed/>");QVERIFY(workspace.StageSource(a,edited,&error));QVERIFY(workspace.Undo(&error));QVERIFY(!workspace.StageMove(preview,&error));QCOMPARE(workspace.Bytes(a),from);QCOMPARE(workspace.Bytes(b),to);
        QVERIFY(workspace.PreviewMove(request,&preview,&error));QVERIFY(Write(a,"<Left/>"));QVERIFY(!workspace.StageMove(preview,&error));QCOMPARE(workspace.Bytes(a),from);QCOMPARE(workspace.Bytes(b),to);
    }
    void workspaceCrossFileMoveRejectsNewIncludeCycle() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray source="<Left><Item include=\"b.xcfg\"/></Left>",target="<Right><Target/></Right>";
        QVERIFY(Write(a,source));QVERIFY(Write(b,target));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));XcfgDocument from,to;QVERIFY(workspace.Document(a,&from,&error));QVERIFY(workspace.Document(b,&to,&error));XcfgWorkspace::MoveRequest request;request.sourceRoot=a;request.targetRoot=b;request.sourceNode=from.Lookup("Left/Item",&error);request.targetParent=to.Lookup("Right/Target",&error);XcfgWorkspace::MovePreview preview;
        QVERIFY(!workspace.PreviewMove(request,&preview,&error));QCOMPARE(workspace.HistoryEntries(),0);QCOMPARE(workspace.Bytes(a),source);QCOMPARE(workspace.Bytes(b),target);
    }
    void workspaceStructureOperations_data() {
        QTest::addColumn<int>("operation");
        for(int i=0;i<=XcfgStructureEditor::OverrideInheritedObject;++i)QTest::newRow(qPrintable(QString("operation-%1").arg(i)))<<i;
    }
    void workspaceStructureOperations() {
        QFETCH(int,operation);QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),meta=directory.filePath("meta.xcfg"),input=directory.filePath("input.xcfg");
        const QByteArray original="<Setup><A id=\"*\" include=\"meta.xcfg\"/><A id=\"0\"><n type=\"int\">3</n><B id=\"0\"/><B id=\"1\"/></A><A id=\"1\"/></Setup>";
        const QByteArray defaults="<Template><n type=\"int\" range=\"[0,20]\">2</n><B id=\"*\"><Settings><v type=\"int\">4</v></Settings></B></Template>";
        QVERIFY(Write(root,original));QVERIFY(Write(meta,defaults));QVERIFY(Write(input,"<Input><z type=\"int\">7</z></Input>"));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));auto staged=defaults;staged.replace(">2<",">5<");QVERIFY(workspace.StageSource(meta,staged,&error));XcfgDocument document;QVERIFY(workspace.Document(root,&document,&error));
        auto node=[&](const QString &name,const QString &id,const QString &file){for(int i=0;i<document.Nodes().size();++i){const auto &n=document.Nodes()[i];if(n.name==name && n.attributes.value("id")==id && n.file==file)return i;}return -1;};
        XcfgStructureEditor::Request request;request.operation=static_cast<XcfgStructureEditor::Operation>(operation);request.node=node("A","0",root);request.name="Child";request.id="2";request.type="int";request.value="6";request.range="[0,30]";request.includePath="input.xcfg";
        if(operation==XcfgStructureEditor::RemoveDeclaration || operation==XcfgStructureEditor::ChangeId || operation==XcfgStructureEditor::ChangeName || operation==XcfgStructureEditor::CloneDeclaration || operation==XcfgStructureEditor::MoveBefore)request.node=node("B","1",root);
        if(operation==XcfgStructureEditor::MoveBefore)request.sibling=node("B","0",root);
        if(operation==XcfgStructureEditor::MoveAfter){request.node=node("B","0",root);request.sibling=node("B","1",root);}
        if(operation==XcfgStructureEditor::MaterializeObject || operation==XcfgStructureEditor::ReplaceWithInclude || operation==XcfgStructureEditor::ImportConfiguration)request.node=node("A","1",root);
        if(operation==XcfgStructureEditor::ChangeInclude){request.node=node("A","*",root);request.includePath=QString();}
        if(operation==XcfgStructureEditor::ChangeScalarProperties)request.node=node("n",QString(),meta);
        if(operation==XcfgStructureEditor::OverrideInheritedObject)request.path="Setup/A[0]/B[0]/Settings";
        QVERIFY(request.node>=0);XcfgWorkspace::StructurePreview preview;QVERIFY2(workspace.PreviewStructure(root,request,&preview,&error),qPrintable(error));QVERIFY2(workspace.StageStructure(preview,&error),qPrintable(error));QCOMPARE(workspace.HistoryEntries(),2);
        const auto changed=workspace.Bytes(preview.edit.destinationFile);QVERIFY(changed!=preview.edit.originalBytes);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(root),original);QCOMPARE(workspace.Bytes(meta),staged);
        QVERIFY(workspace.Redo(&error));QCOMPARE(workspace.Bytes(preview.edit.destinationFile),changed);
        QFile disk(root);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);
    }
    void workspaceStructureSharesTextHistoryWithoutWritingDisk() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg"),shared=directory.filePath("shared.xcfg");
        const QByteArray root="<Setup><A id=\"*\" include=\"shared.xcfg\"/><A id=\"0\"/></Setup>",original="<Template><n type=\"int\">2</n></Template>";
        QVERIFY(Write(a,root));QVERIFY(Write(b,root));QVERIFY(Write(shared,original));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));
        auto staged=original;staged.replace(">2<",">3<");QVERIFY(workspace.StageSource(shared,staged,&error));
        XcfgDocument document;QVERIFY(workspace.Document(a,&document,&error));int node=-1;for(int i=0;i<document.Nodes().size();++i)if(document.Nodes()[i].name=="n")node=i;QVERIFY(node>=0);
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::ChangeName;request.node=node;request.name="threshold";
        XcfgWorkspace::StructurePreview preview;QVERIFY2(workspace.PreviewStructure(a,request,&preview,&error),qPrintable(error));QCOMPARE(preview.workspace.reports.size(),2);QCOMPARE(preview.edit.originalBytes,staged);QCOMPARE(workspace.HistoryEntries(),1);
        QVERIFY(workspace.StageStructure(preview,&error));QCOMPARE(workspace.HistoryEntries(),2);QVERIFY(workspace.Bytes(shared).contains("<threshold"));
        QFile disk(shared);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(shared),staged);QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(shared),original);
        QVERIFY(workspace.Redo(&error));QCOMPARE(workspace.Bytes(shared),staged);QVERIFY(workspace.Redo(&error));QVERIFY(workspace.Bytes(shared).contains("threshold"));
        QVERIFY(!workspace.StageStructure(preview,&error));
    }
    void workspaceStructureRejectsInvalidOtherRootAndStaleReview() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg"),shared=directory.filePath("shared.xcfg");
        const QByteArray meta="<Template><n type=\"int\" range=\"[0,20]\">2</n></Template>";
        QVERIFY(Write(a,"<Setup><A id=\"*\" include=\"shared.xcfg\"/><A id=\"0\"><n type=\"int\">3</n></A></Setup>"));
        QVERIFY(Write(b,"<Setup><A id=\"*\" include=\"shared.xcfg\"/><A id=\"0\"><n type=\"int\">17</n></A></Setup>"));QVERIFY(Write(shared,meta));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));XcfgDocument document;QVERIFY(workspace.Document(a,&document,&error));int node=-1;for(int i=0;i<document.Nodes().size();++i)if(document.Nodes()[i].file==shared && document.Nodes()[i].name=="n")node=i;
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::ChangeScalarProperties;request.node=node;request.type="int";request.range="[0,9]";
        XcfgWorkspace::StructurePreview preview;QVERIFY(!workspace.PreviewStructure(a,request,&preview,&error));QCOMPARE(workspace.HistoryEntries(),0);QCOMPARE(workspace.Bytes(shared),meta);
        request.range="[0,30]";QVERIFY(workspace.PreviewStructure(a,request,&preview,&error));auto edited=meta;edited.replace(">2<",">4<");QVERIFY(workspace.StageSource(shared,edited,&error));QVERIFY(!workspace.StageStructure(preview,&error));QCOMPARE(workspace.HistoryEntries(),1);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(shared),meta);QVERIFY(!workspace.StageStructure(preview,&error));
    }
    void workspaceImportReadsReviewedStagedSourceAndRejectsChangedInput() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");
        const QByteArray root="<Setup><A id=\"0\"/></Setup>",imported="<Import><n type=\"int\">2</n></Import>";
        QVERIFY(Write(a,root));QVERIFY(Write(b,imported));XcfgWorkspace workspace;QVERIFY(workspace.Open(a,&error));QVERIFY(workspace.Open(b,&error));auto changed=imported;changed.replace(">2<",">5<");QVERIFY(workspace.StageSource(b,changed,&error));XcfgDocument document;QVERIFY(workspace.Document(a,&document,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::ImportConfiguration;request.node=1;request.includePath="b.xcfg";XcfgWorkspace::StructurePreview preview;
        QVERIFY2(workspace.PreviewStructure(a,request,&preview,&error),qPrintable(error));QVERIFY(preview.edit.updatedBytes.contains(">5<"));QVERIFY(workspace.StageStructure(preview,&error));QCOMPARE(workspace.HistoryEntries(),2);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(a),root);QCOMPARE(workspace.Bytes(b),changed);
        QVERIFY(workspace.PreviewStructure(a,request,&preview,&error));QVERIFY(Write(b,"<Import/>"));QVERIFY(!workspace.StageStructure(preview,&error));QCOMPARE(workspace.Bytes(a),root);
    }
    void workspaceHistoryUndoesCombinedChangesAfterSaveWithoutWritingDisk() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),meta=directory.filePath("meta.xcfg");
        const QByteArray original="<Setup><A id=\"*\" include=\"meta.xcfg\"/><A id=\"0\"><n type=\"int\">2</n></A></Setup>\r\n";
        const QByteArray range="<Template><n type=\"int\" range=\"[0,9]\">2</n></Template>\r\n";
        QVERIFY(Write(root,original));QVERIFY(Write(meta,range));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        auto changed=original,wider=range;changed.replace(">2<",">12<");wider.replace("[0,9]","[0,20]");
        QMap<QString,QByteArray> batch;batch[root]=changed;batch[meta]=wider;
        QVERIFY(workspace.StageSources(batch,&error));QCOMPARE(workspace.HistoryEntries(),1);
        XcfgWorkspace::Preview reviewed;QVERIFY(workspace.CreatePreview(&reviewed,&error));XcfgWorkspace::SaveResult result;
        QVERIFY(workspace.SaveAll(reviewed,&result,&error));QVERIFY(workspace.CanUndo());
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(root),original);QCOMPARE(workspace.Bytes(meta),range);
        QFile disk(root);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),changed);disk.close();
        QVERIFY(workspace.Redo(&error));QVERIFY(workspace.DirtyFiles().isEmpty());
        QVERIFY(!workspace.SaveAll(reviewed,&result,&error));QVERIFY(result.savedFiles.isEmpty());
        QVERIFY(workspace.Undo(&error));QVERIFY(workspace.CreatePreview(&reviewed,&error));
        QVERIFY(workspace.SaveAll(reviewed,&result,&error));QVERIFY(workspace.CanRedo());
        QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);
        QVERIFY(workspace.Redo(&error));QCOMPARE(workspace.DirtyFiles().size(),2);
    }
    void workspaceHistoryRejectsSourceDriftAndNewRootInvalidUndoAtomically() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),meta=directory.filePath("meta.xcfg"),other=directory.filePath("other.xcfg");
        const QByteArray original="<Setup><A id=\"*\" include=\"meta.xcfg\"/><A id=\"0\"><n type=\"int\">2</n></A></Setup>";
        const QByteArray range="<Template><n type=\"int\" range=\"[0,9]\">2</n></Template>";
        QVERIFY(Write(root,original));QVERIFY(Write(meta,range));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        auto changed=original,wider=range;changed.replace(">2<",">12<");wider.replace("[0,9]","[0,20]");
        QMap<QString,QByteArray> batch;batch[root]=changed;batch[meta]=wider;QVERIFY(workspace.StageSources(batch,&error));
        QVERIFY(Write(meta,"<Template/>"));QVERIFY(!workspace.Undo(&error));QCOMPARE(workspace.Bytes(root),changed);QVERIFY(workspace.CanUndo());
        QVERIFY(Write(meta,range));auto otherBytes=original;otherBytes.replace(">2<",">17<");QVERIFY(Write(other,otherBytes));QVERIFY(workspace.Open(other,&error));
        QVERIFY(!workspace.Undo(&error));QCOMPARE(workspace.Bytes(root),changed);QCOMPARE(workspace.Bytes(meta),wider);QVERIFY(workspace.CanUndo());QVERIFY(!workspace.CanRedo());
    }
    void workspaceHistoryKeepsRedoOnNoopAndReplacesItOnlyOnValidBranch() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">0</n></Setup>";
        QVERIFY(Write(root,original));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));auto one=original;one.replace(">0<",">1<");
        QVERIFY(workspace.StageSource(root,one,&error));QVERIFY(workspace.Undo(&error));QVERIFY(workspace.CanRedo());
        QVERIFY(workspace.StageSource(root,original,&error));QVERIFY(workspace.CanRedo());QCOMPARE(workspace.HistoryEntries(),1);
        QVERIFY(!workspace.StageSource(root,"<invalid",&error));QVERIFY(workspace.CanRedo());QCOMPARE(workspace.Bytes(root),original);
        auto two=original;two.replace(">0<",">2<");QVERIFY(workspace.StageSource(root,two,&error));QVERIFY(!workspace.CanRedo());QCOMPARE(workspace.HistoryEntries(),1);
        QVERIFY(workspace.Undo(&error));QCOMPARE(workspace.Bytes(root),original);
    }
    void workspaceHistoryEnforcesEntryAndSnapshotByteBounds() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");
        const QByteArray prefix=QByteArray("<Setup><!--")+QByteArray(2*1024*1024,'x')+"--><n type=\"int\">";
        QVERIFY(Write(root,prefix+"0</n></Setup>"));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        for(int i=1;i<=36;++i)QVERIFY2(workspace.StageSource(root,prefix+QByteArray::number(i)+"</n></Setup>",&error),qPrintable(error));
        QVERIFY(workspace.HistoryEntries()<=32);QVERIFY(workspace.HistoryBytes()<=128LL*1024*1024);QVERIFY(workspace.HistoryEntries()<32);
        int undone=0;while(workspace.CanUndo()){QVERIFY(workspace.Undo(&error));++undone;}
        QVERIFY(undone>0 && undone<36);QVERIFY(workspace.Bytes(root)!=prefix+"0</n></Setup>");
        while(workspace.CanRedo())QVERIFY(workspace.Redo(&error));QCOMPARE(workspace.Bytes(root),prefix+"36</n></Setup>");
    }
    void multiRootSaveAllKeepsDiskUnchangedUntilReview() {
        QTemporaryDir directory;QString error;
        const auto first=directory.filePath("first.xcfg"),second=directory.filePath("second.xcfg"),shared=directory.filePath("shared.xcfg");
        const QByteArray original="<Template><!-- preserve --><n type=\"int\" range=\"[0,9]\">2</n></Template>\r\n";
        QVERIFY(Write(first,"<Setup><Board id=\"0\" include=\"shared.xcfg\"/></Setup>"));
        QVERIFY(Write(second,"<Setup><Board id=\"1\" include=\"shared.xcfg\"/></Setup>"));QVERIFY(Write(shared,original));
        XcfgWorkspace workspace;QVERIFY2(workspace.Open(first,&error),qPrintable(error));QVERIFY(workspace.Open(second,&error));
        QCOMPARE(workspace.Files().size(),3);QCOMPARE(workspace.Roots().size(),2);
        const auto path=QFileInfo(shared).canonicalFilePath();QByteArray updated=original;updated.replace(">2<",">7<");
        QVERIFY(workspace.StageSource(path,updated,&error));QFile disk(shared);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);disk.close();
        XcfgWorkspace::Preview preview;QVERIFY(workspace.CreatePreview(&preview,&error));QCOMPARE(preview.writes.size(),1);QCOMPARE(preview.reports.size(),2);
        for(auto report:preview.reports) QVERIFY(report.value("effective_changed").toBool());
        XcfgWorkspace::SaveResult result;QVERIFY2(workspace.SaveAll(preview,&result,&error),qPrintable(error));
        QVERIFY(result.complete);QCOMPARE(result.savedFiles.size(),1);QVERIFY(workspace.DirtyFiles().isEmpty());
        QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),updated);
    }
    void workspaceStagesCombinedValueAndRangeChanges() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),metadata=directory.filePath("meta.xcfg");
        const QByteArray before="<Setup><A id=\"*\" include=\"meta.xcfg\"/><A id=\"0\"><n type=\"int\">2</n></A></Setup>";
        const QByteArray ranges="<Template><n type=\"int\" range=\"[0,9]\">2</n></Template>";
        QVERIFY(Write(root,before));QVERIFY(Write(metadata,ranges));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        auto changed=before;changed.replace(">2<",">12<");
        QVERIFY(!workspace.StageSource(QFileInfo(root).canonicalFilePath(),changed,&error));QVERIFY(workspace.DirtyFiles().isEmpty());
        auto wider=ranges;wider.replace("[0,9]","[0,20]");QMap<QString,QByteArray> both;
        both[QFileInfo(root).canonicalFilePath()]=changed;both[QFileInfo(metadata).canonicalFilePath()]=wider;
        QVERIFY2(workspace.StageSources(both,&error),qPrintable(error));QCOMPARE(workspace.DirtyFiles().size(),2);
        XcfgWorkspace::Preview preview;QVERIFY(workspace.CreatePreview(&preview,&error));XcfgWorkspace::SaveResult result;
        QVERIFY(workspace.SaveAll(preview,&result,&error));QCOMPARE(result.savedFiles.size(),2);
    }
    void workspaceRejectsChangedDiskAndStaleReview() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");
        const QByteArray before="<Setup><n type=\"int\">2</n></Setup>";QVERIFY(Write(root,before));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));const auto path=QFileInfo(root).canonicalFilePath();
        auto changed=before;changed.replace(">2<",">3<");QVERIFY(workspace.StageSource(path,changed,&error));
        XcfgWorkspace::Preview preview;QVERIFY(workspace.CreatePreview(&preview,&error));
        auto newer=changed;newer.replace(">3<",">4<");QVERIFY(workspace.StageSource(path,newer,&error));
        XcfgWorkspace::SaveResult result;QVERIFY(!workspace.SaveAll(preview,&result,&error));QVERIFY(result.savedFiles.isEmpty());
        QVERIFY(workspace.CreatePreview(&preview,&error));auto external=before;external.replace(">2<",">8<");QVERIFY(Write(root,external));
        QVERIFY(!workspace.SaveAll(preview,&result,&error));QVERIFY(result.savedFiles.isEmpty());
        QFile disk(root);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),external);
    }
    void workspaceInvalidBatchDoesNotRetainPartialStaging() {
        QTemporaryDir directory;QString error;const auto first=directory.filePath("one.xcfg"),second=directory.filePath("two.xcfg");
        const QByteArray before="<Setup><n type=\"int\">2</n></Setup>";QVERIFY(Write(first,before));QVERIFY(Write(second,before));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(first,&error));QVERIFY(workspace.Open(second,&error));
        auto valid=before;valid.replace(">2<",">3<");QMap<QString,QByteArray> changes;
        changes[QFileInfo(first).canonicalFilePath()]=valid;changes[QFileInfo(second).canonicalFilePath()]="<Setup>";
        QVERIFY(!workspace.StageSources(changes,&error));QVERIFY(workspace.DirtyFiles().isEmpty());
        QCOMPARE(workspace.Bytes(QFileInfo(first).canonicalFilePath()),before);
    }
    void workspaceTamperedReviewAndDetachedInvalidSourceDoNotWrite() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),child=directory.filePath("child.xcfg");
        const QByteArray original="<Setup><A include=\"child.xcfg\"/></Setup>",leaf="<Template><n type=\"int\">2</n></Template>";
        QVERIFY(Write(root,original));QVERIFY(Write(child,leaf));XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        QMap<QString,QByteArray> batch;batch[QFileInfo(root).canonicalFilePath()]="<Setup><A/></Setup>";batch[QFileInfo(child).canonicalFilePath()]="<Template>";
        QVERIFY(!workspace.StageSources(batch,&error));QVERIFY(workspace.DirtyFiles().isEmpty());
        batch[QFileInfo(child).canonicalFilePath()]="<Template><n type=\"int\">3</n></Template>";
        QVERIFY(workspace.StageSources(batch,&error));XcfgWorkspace::Preview preview;QVERIFY(workspace.CreatePreview(&preview,&error));
        auto tampered=preview;tampered.writes[QFileInfo(child).canonicalFilePath()]=leaf;
        XcfgWorkspace::SaveResult result;QVERIFY(!workspace.SaveAll(tampered,&result,&error));QVERIFY(result.savedFiles.isEmpty());
        QFile file(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void workspaceSaveAllPreparesEveryFileBeforeReplacingAny() {
#ifdef Q_OS_UNIX
        QTemporaryDir directory;QString error;QVERIFY(QDir(directory.path()).mkdir("locked"));
        const auto first=directory.filePath("a.xcfg"),folder=directory.filePath("locked"),second=folder+"/z.xcfg";
        const QByteArray original="<Setup><n type=\"int\">2</n></Setup>";QVERIFY(Write(first,original));QVERIFY(Write(second,original));
        XcfgWorkspace workspace;QVERIFY(workspace.Open(first,&error));QVERIFY(workspace.Open(second,&error));
        auto changed=original;changed.replace(">2<",">3<");QMap<QString,QByteArray> changes;
        changes[QFileInfo(first).canonicalFilePath()]=changed;changes[QFileInfo(second).canonicalFilePath()]=changed;
        QVERIFY(workspace.StageSources(changes,&error));XcfgWorkspace::Preview preview;QVERIFY(workspace.CreatePreview(&preview,&error));
        const auto permissions=QFile::permissions(folder);QVERIFY(QFile::setPermissions(folder,QFile::ReadOwner|QFile::ExeOwner));
        XcfgWorkspace::SaveResult result;const bool saved=workspace.SaveAll(preview,&result,&error);
        const bool restored=QFile::setPermissions(folder,permissions);QVERIFY(restored);QVERIFY(!saved);
        QVERIFY(result.savedFiles.isEmpty());QCOMPARE(result.remainingFiles.size(),2);QVERIFY(!workspace.RequiresReopen());
        QFile file(first);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
#else
        QTest::qSkip("Unix directory permission fixture; Windows requires its own ACL fixture",__FILE__,__LINE__);return;
#endif
    }
    void workspaceCopyPreservesValuesAndSplitsReferences_data() {
        QTest::addColumn<int>("sharing");QTest::addColumn<int>("files");
        QTest::newRow("shared")<<int(XcfgWorkspaceCopy::PreserveSharedFiles)<<3;
        QTest::newRow("isolated")<<int(XcfgWorkspaceCopy::IsolateEachReference)<<5;
    }
    void workspaceCopyPreservesValuesAndSplitsReferences() {
        QFETCH(int,sharing);QFETCH(int,files);QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),shared=directory.filePath("shared.xcfg"),leaf=directory.filePath("leaf.xcfg");
        const QByteArray original="<Setup><!-- keep --><Board id=\"0\" include=\"shared.xcfg\"/><Board id=\"1\" include=\"shared.xcfg\"/></Setup>";
        const QByteArray external="<Template><Module id=\"*\" include=\"leaf.xcfg\"/><Module id=\"0\"/></Template>",tail="<Other><Group><n type=\"int\" range=\"[0,9]\">2</n></Group></Other>";
        QVERIFY(Write(main,original));QVERIFY(Write(shared,external));QVERIFY(Write(leaf,tail));XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));
        XcfgWorkspaceCopy::Request request;request.sharing=static_cast<XcfgWorkspaceCopy::Sharing>(sharing);request.directory=directory.filePath("copy");
        XcfgWorkspaceCopy::Preview preview;QVERIFY2(XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.files.size(),files);QVERIFY(!QFileInfo::exists(request.directory));QVERIFY(preview.files.value("main.xcfg").contains("keep"));
        QJsonObject receipt;QVERIFY2(XcfgWorkspaceCopy::Save(document,preview,&receipt,&error),qPrintable(error));
        QVERIFY(QFileInfo::exists(request.directory+"/workspace.json"));QVERIFY(!QFileInfo::exists(request.directory+"/workspace.incomplete"));
        XcfgDocument copy;QVERIFY(copy.Load(receipt.value("root_file").toString(),&error));QCOMPARE(copy.Sources().size(),files);
        const int first=copy.Lookup("Setup/Board[0]/Module[0]",&error),second=copy.Lookup("Setup/Board[1]/Module[0]",&error);
        if(sharing==XcfgWorkspaceCopy::PreserveSharedFiles)QCOMPARE(copy.Nodes()[first].file,copy.Nodes()[second].file);
        else QVERIFY(copy.Nodes()[first].file!=copy.Nodes()[second].file);
        XcfgStructureEditor::Request override;override.operation=XcfgStructureEditor::OverrideInheritedObject;override.path="Setup/Board[0]/Module[0]/Group";
        XcfgStructureEditor::Preview proposal;QVERIFY(XcfgStructureEditor::CreatePreview(copy,override,&proposal,&error));
        QCOMPARE(proposal.localizedPaths.size(),sharing==XcfgWorkspaceCopy::PreserveSharedFiles ? 2 : 1);
        QVERIFY(XcfgStructureEditor::Apply(&copy,proposal,&error));
        QFile rootFile(main);QVERIFY(rootFile.open(QIODevice::ReadOnly));QCOMPARE(rootFile.readAll(),original);
        QFile other(shared);QVERIFY(other.open(QIODevice::ReadOnly));QCOMPARE(other.readAll(),external);
        QFile last(leaf);QVERIFY(last.open(QIODevice::ReadOnly));QCOMPARE(last.readAll(),tail);
        QVERIFY(!XcfgWorkspaceCopy::Save(document,preview,&receipt,&error));QVERIFY(error.contains("exists"));
    }
    void workspaceCopyRejectsDriftTamperedFilesAndExistingDirectories() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg");const QByteArray original="<Setup><n>2</n></Setup>";QVERIFY(Write(main,original));
        XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));XcfgWorkspaceCopy::Request request;request.directory=directory.filePath("copy");
        XcfgWorkspaceCopy::Preview preview;QVERIFY(XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error));QJsonObject receipt;
        auto tampered=preview;tampered.files["main.xcfg"].replace(">2<",">3<");QVERIFY(!XcfgWorkspaceCopy::Save(document,tampered,&receipt,&error));QVERIFY(!QFileInfo::exists(request.directory));
        tampered=preview;tampered.request.directory=directory.filePath("redirected");QVERIFY(!XcfgWorkspaceCopy::Save(document,tampered,&receipt,&error));QVERIFY(!QFileInfo::exists(tampered.request.directory));
        QVERIFY(Write(main,original+"<!-- drift -->"));QVERIFY(!XcfgWorkspaceCopy::Save(document,preview,&receipt,&error));QVERIFY(!QFileInfo::exists(request.directory));
        QVERIFY(Write(main,original));QVERIFY(document.Load(main,&error));request.directory=directory.path();QVERIFY(!XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error));
        request.directory=directory.filePath("missing/copy");QVERIFY(!XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error));
    }
    void workspaceIsolationExpansionIsBoundedBeforeCreatingDestination() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),leaf=directory.filePath("leaf.xcfg");QByteArray xml="<Setup>";
        for(int i=0;i<257;++i)xml+="<Board id=\""+QByteArray::number(i)+"\" include=\"leaf.xcfg\"/>";xml+="</Setup>";
        QVERIFY(Write(main,xml));QVERIFY(Write(leaf,"<Template><n>2</n></Template>"));XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));
        XcfgWorkspaceCopy::Request request;request.directory=directory.filePath("isolated");XcfgWorkspaceCopy::Preview preview;
        QVERIFY(!XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("256"));QVERIFY(!QFileInfo::exists(request.directory));
        request.sharing=XcfgWorkspaceCopy::PreserveSharedFiles;request.directory=directory.filePath("shared");QVERIFY2(XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error),qPrintable(error));QCOMPARE(preview.files.size(),2);
    }
    void shallowInheritedObjectOverride_data() {
        QTest::addColumn<QByteArray>("body");QTest::addColumn<QString>("query");QTest::addColumn<int>("created");
        QTest::newRow("simple")<<QByteArray("<Module><gain>2</gain></Module>")<<QString("Module")<<1;
        QTest::newRow("nested")<<QByteArray("<Module><Group><gain>2</gain></Group><Other/></Module>")<<QString("Module/Group")<<2;
        QTest::newRow("default-siblings")<<QByteArray("<Module id=\"*\"><gain>2</gain></Module><Module id=\"1\"/><Module id=\"2\"/>")<<QString("Module[*]")<<3;
        QTest::newRow("concrete-sibling")<<QByteArray("<Module id=\"*\"><gain>2</gain></Module><Module id=\"1\"/><Module id=\"2\"/>")<<QString("Module[1]")<<1;
    }
    void shallowInheritedObjectOverride() {
        QFETCH(QByteArray,body);QFETCH(QString,query);QFETCH(int,created);QTemporaryDir directory;const auto path=directory.filePath("override.xcfg");
        const QByteArray original="<Setup><!-- main --><Board id=\"*\">"+body+"</Board><Board id=\"0\"/></Setup>";
        QVERIFY(Write(path,original));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::OverrideInheritedObject;request.path="Setup/Board[0]/"+query;
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.createdPaths.size(),created);QVERIFY(!preview.report.value("effective_changed").toBool());QCOMPARE(preview.destinationFile,path);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QVERIFY2(history.ApplyStructure(&document,preview,&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup(request.path,&error)].file,path);
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("direct declaration"));
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));
        QCOMPARE(document.Sources().value(path).bytes,original);
        XcfgEditHistory::Preview redo;QVERIFY(history.CreatePreview(document,true,&redo,&error));QVERIFY(history.Apply(&document,redo,&error));
    }
    void objectOverrideHandlesInheritedIncludeAndForeignParent() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),foreign=directory.filePath("foreign.xcfg");
        const QByteArray external="<Template><!-- pre --><Module id=\"*\"><Group><n>2</n></Group><!-- post --></Module><Module id=\"0\"/></Template>";
        const QByteArray original="<Setup><Board id=\"0\" include=\"foreign.xcfg\"/></Setup>";QVERIFY(Write(main,original));QVERIFY(Write(foreign,external));
        XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(main,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::OverrideInheritedObject;request.path="Setup/Board[0]/Module[0]/Group";
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.destinationFile,foreign);QVERIFY(preview.dependencyBytes.contains(main));QVERIFY(!preview.report.value("effective_changed").toBool());
        QVERIFY(history.ApplyStructure(&document,preview,&error));QCOMPARE(document.Sources().value(main).bytes,original);
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));QCOMPARE(document.Sources().value(foreign).bytes,external);
        request.path="Setup/Board[0]/Module[0]";QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("direct declaration"));
        QVERIFY(Write(main,"<Setup><Board id=\"*\"><Module include=\"foreign.xcfg\"/></Board><Board id=\"0\"/></Setup>"));QVERIFY(document.Load(main,&error));
        request.path="Setup/Board[0]/Module";QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QVERIFY(history.ApplyStructure(&document,preview,&error));const int own=document.Lookup(request.path,&error);QVERIFY(!document.Nodes()[own].attributes.contains("include"));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/Module/Module[0]/Group/n",&error)].value,QString("2"));
    }
    void sharedIncludeObjectOverrideListsEveryConcreteProjection() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),foreign=directory.filePath("shared.xcfg");
        const QByteArray original="<Setup><Board id=\"0\" include=\"shared.xcfg\"/><Board id=\"1\" include=\"shared.xcfg\"/></Setup>";
        const QByteArray shared="<Template><Module id=\"*\"><Group><n>2</n></Group></Module><Module id=\"0\"/></Template>";
        QVERIFY(Write(main,original));QVERIFY(Write(foreign,shared));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(main,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::OverrideInheritedObject;request.path="Setup/Board[0]/Module[0]/Group";
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.destinationFile,foreign);QCOMPARE(preview.createdPaths.size(),1);QCOMPARE(preview.localizedPaths.size(),2);
        QVERIFY(preview.localizedPaths.contains("Setup/Board[0]/Module[0]/Group"));QVERIFY(preview.localizedPaths.contains("Setup/Board[1]/Module[0]/Group"));
        auto forged=preview;forged.localizedPaths.removeLast();QVERIFY(!XcfgStructureEditor::Apply(&document,forged,&error));
        QVERIFY(history.ApplyStructure(&document,preview,&error));QCOMPARE(document.Sources().value(main).bytes,original);
        QCOMPARE(document.Lookup("Setup/Board[0]/Module[0]/Group",&error),document.Lookup("Setup/Board[1]/Module[0]/Group",&error));
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));QCOMPARE(document.Sources().value(foreign).bytes,shared);
    }
    void objectOverrideRejectsTamperingDriftAndUnknownInstances() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),foreign=directory.filePath("foreign.xcfg");
        const QByteArray original="<Setup><Board id=\"*\" include=\"foreign.xcfg\"/><Board id=\"0\"/></Setup>",external="<Template><!-- before --><Module><n>2</n><!-- after --></Module></Template>";
        QVERIFY(Write(main,original));QVERIFY(Write(foreign,external));XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::OverrideInheritedObject;request.path="Setup/Board[0]/Module";
        XcfgStructureEditor::Preview preview;QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QVERIFY(preview.updatedBytes.contains("before"));QVERIFY(preview.updatedBytes.contains("after"));
        auto bad=preview;bad.createdPaths.clear();QVERIFY(!XcfgStructureEditor::Apply(&document,bad,&error));QCOMPARE(document.Sources().value(main).bytes,original);
        QVERIFY(Write(foreign,external+"<!-- drift -->"));QVERIFY(!XcfgStructureEditor::Apply(&document,preview,&error));QVERIFY(Write(foreign,external));QVERIFY(document.Load(main,&error));
        for(const auto &path:{QString("Setup/Board[8]/Module"),QString("Setup"),QString("Setup/Board[0]/Module/n")}) {
            request.path=path;QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        }
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void exportXmlCopy_data() {
        QTest::addColumn<int>("mode");QTest::newRow("declarations")<<int(XcfgExporter::FlattenDeclarations);
        QTest::newRow("effective")<<int(XcfgExporter::EffectiveConcreteObject);
    }
    void exportXmlCopy() {
        QFETCH(int,mode);QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),included=directory.filePath("template.xcfg"),output=directory.filePath("copy.xcfg");
        const QByteArray original="<Setup><!-- main --><Board id=\"*\" include=\"template.xcfg\"/><Board id=\"0\"/><Board id=\"1\"><gain unit=\"mV\">3</gain></Board></Setup>";
        const QByteArray external="<Template><!-- external --><gain type=\"int\" unit=\"V\" range=\"[0,9]\">2</gain><group id=\"*\"><label type=\"string\"><![CDATA[  a   b  ]]></label></group><group id=\"0\"/><Empty/></Template>";
        QVERIFY(Write(main,original));QVERIFY(Write(included,external));XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));
        QJsonObject before;QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&before,&error));
        XcfgExporter::Request request;request.mode=static_cast<XcfgExporter::Mode>(mode);request.path="Setup";request.destination=output;
        XcfgExporter::Preview preview;QVERIFY2(XcfgExporter::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(!QFileInfo::exists(output));QCOMPARE(preview.sourceBytes.value(main),original);QCOMPARE(preview.sourceBytes.value(included),external);
        QVERIFY(!preview.xml.contains("include="));QCOMPARE(preview.effectiveSha256,before.value("effective_sha256").toString());
        if(mode==XcfgExporter::FlattenDeclarations){QVERIFY(preview.xml.contains("id=\"*\""));QVERIFY(preview.xml.contains("external"));QVERIFY(preview.xml.contains("CDATA"));}
        else{QVERIFY(!preview.xml.contains("id=\"*\""));QVERIFY(preview.xml.contains("&#32;&#32;a&#32;&#32;&#32;b&#32;&#32;"));}
        QVERIFY2(XcfgExporter::Save(document,preview,&error),qPrintable(error));
        XcfgDocument copy;QVERIFY(copy.Load(output,&error));QCOMPARE(copy.Sources().size(),1);
        XcfgDocument::Node gain;XcfgDocument::ScalarProperties properties;
        QVERIFY(copy.ResolveScalar("Setup/Board[0]/gain",&gain,&properties,&error));QCOMPARE(gain.value,QString("2"));QCOMPARE(properties.unit,QString("mV"));
        QCOMPARE(copy.Nodes()[copy.Lookup("Setup/Board[0]/group[0]/label",&error)].value,QString("  a   b  "));
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QFile foreign(included);QVERIFY(foreign.open(QIODevice::ReadOnly));QCOMPARE(foreign.readAll(),external);
        QVERIFY(!XcfgExporter::Save(document,preview,&error));QVERIFY(error.contains("exists"));
    }
    void exportXmlSubtreeModesAndTemplates() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        QVERIFY(Write(path,"<Setup id=\"run\"><Board id=\"*\"><gain unit=\"mV\">2</gain><group id=\"*\"><n>3</n></group><group id=\"0\"/></Board><Board id=\"0\"/></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgExporter::Request request;request.path="Setup/Board[0]";request.destination=directory.filePath("effective.xcfg");request.mode=XcfgExporter::EffectiveConcreteObject;
        XcfgExporter::Preview preview;QVERIFY2(XcfgExporter::CreatePreview(document,request,&preview,&error),qPrintable(error));QCOMPARE(preview.scalarCount,2);
        QVERIFY(XcfgExporter::Save(document,preview,&error));XcfgDocument copy;QVERIFY(copy.Load(request.destination,&error));QVERIFY(copy.Lookup("Board[0]/group[0]/n",&error)>=0);
        request.mode=XcfgExporter::FlattenDeclarations;request.destination=directory.filePath("declaration.xcfg");
        QVERIFY(XcfgExporter::CreatePreview(document,request,&preview,&error));QCOMPARE(preview.scalarCount,0);QVERIFY(XcfgExporter::Save(document,preview,&error));
        request.path="Setup/Board[*]";request.destination=directory.filePath("template.xcfg");
        QVERIFY2(XcfgExporter::CreatePreview(document,request,&preview,&error),qPrintable(error));QVERIFY(preview.xml.startsWith("<Board>"));QVERIFY(preview.xml.contains("group id=\"*\""));
        QVERIFY(XcfgExporter::Save(document,preview,&error));request.mode=XcfgExporter::EffectiveConcreteObject;request.destination=directory.filePath("refused.xcfg");
        QVERIFY(!XcfgExporter::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("concrete"));QVERIFY(!QFileInfo::exists(request.destination));
    }
    void exportXmlRejectsDriftTamperingAndInvalidTargets() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),foreign=directory.filePath("include.xcfg"),output=directory.filePath("new.xcfg");
        const QByteArray original="<Setup include=\"include.xcfg\"/>",external="<Template><n>2</n></Template>";QVERIFY(Write(main,original));QVERIFY(Write(foreign,external));
        XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));XcfgExporter::Request request;request.path="Setup";request.destination=output;
        XcfgExporter::Preview preview;QVERIFY(XcfgExporter::CreatePreview(document,request,&preview,&error));
        auto tampered=preview;tampered.xml.replace(">2<",">3<");QVERIFY(!XcfgExporter::Save(document,tampered,&error));QVERIFY(!QFileInfo::exists(output));
        QVERIFY(Write(foreign,external+"<!-- changed -->"));QVERIFY(!XcfgExporter::Save(document,preview,&error));QVERIFY(!QFileInfo::exists(output));
        QVERIFY(Write(foreign,external));QVERIFY(document.Load(main,&error));request.destination=main;QVERIFY(!XcfgExporter::CreatePreview(document,request,&preview,&error));
        request.destination=directory.filePath("missing/output.xcfg");QVERIFY(!XcfgExporter::CreatePreview(document,request,&preview,&error));
        request.destination=output;request.path="Setup/n";QVERIFY(!XcfgExporter::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("object"));
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void legacyXmlTextWhitespace_data() {
        QTest::addColumn<QByteArray>("content");QTest::addColumn<QString>("value");
        QTest::newRow("literal")<<QByteArray("  a   b\t c\n d  ")<<QString("a b c d");
        QTest::newRow("references")<<QByteArray("&#32;&#32;a&#32;&#32;b&#10;c&#32;")<<QString("  a  b\nc ");
        QTest::newRow("cdata")<<QByteArray("<![CDATA[  a   b\t c\n d  ]]>")<<QString("  a   b\t c\n d  ");
        QTest::newRow("unicode")<<QString::fromUtf8("\xc2\xa0" "a\xc2\xa0\xc2\xa0" "b\xc2\xa0").toUtf8()<<QString::fromUtf8("\xc2\xa0" "a\xc2\xa0\xc2\xa0" "b\xc2\xa0");
        QTest::newRow("comment-tail")<<QByteArray(" a   b <!-- keep -->")<<QString("a b");
        QTest::newRow("comment-head")<<QByteArray("<!-- keep --> a   b ")<<QString("a b");
        QTest::newRow("empty-typed")<<QByteArray(" \n ")<<QString("");
        QTest::newRow("literal-and-reference")<<QByteArray(" a &#32; b ")<<QString("a   b");
    }
    void legacyXmlTextWhitespace() {
        QFETCH(QByteArray,content);QFETCH(QString,value);QTemporaryDir directory;
        const auto path=directory.filePath("text.xcfg");const auto bytes="<Setup><label type=\"string\">"+content+"</label></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY2(document.Load(path,&error),qPrintable(error));
        const int node=document.Lookup("Setup/label",&error);QVERIFY(node>=0);QCOMPARE(document.Nodes()[node].value,value);
        QCOMPARE(document.Sources().value(path).bytes,bytes);
        QJsonObject snapshot;QVERIFY2(EffectiveConfigurationSnapshot::Capture(document,&snapshot,&error),qPrintable(error));
        QCOMPARE(snapshot.value("scalars").toArray().first().toObject().value("normalized_value").toString(),value);
    }
    void scalarTokenEditingPreservesMarkup_data() {
        QTest::addColumn<QByteArray>("content");QTest::addColumn<QString>("value");QTest::addColumn<QByteArray>("expected");
        QTest::newRow("cdata-comments")<<QByteArray(" <!-- head -->\n<![CDATA[old]]><!-- tail --> ")<<QString::fromUtf8("  中文<&B\n ")<<QString::fromUtf8(" <!-- head -->\n<![CDATA[  中文<&B\n ]]><!-- tail --> ").toUtf8();
        QTest::newRow("comment-text")<<QByteArray("<!-- head -->  old <!-- tail -->")<<QString(" A<&B ")<<QByteArray("<!-- head -->  &#32;A&lt;&amp;B&#32; <!-- tail -->");
        QTest::newRow("instruction")<<QByteArray("<?keep before?>old<?keep after?>")<<QString("new")<<QByteArray("<?keep before?>new<?keep after?>");
        QTest::newRow("empty-comment")<<QByteArray("<!-- keep -->")<<QString("new")<<QByteArray("new<!-- keep -->");
        QTest::newRow("empty-cdata")<<QByteArray("<![CDATA[]]>")<<QString("new")<<QByteArray("<![CDATA[new]]>");
        QTest::newRow("cdata-terminator")<<QByteArray("<!-- keep --><![CDATA[old]]>")<<QString(" A]]>B\r\n ")<<QByteArray("<!-- keep -->&#32;A]]&gt;B&#13;&#10;&#32;");
        QTest::newRow("nbsp-is-value")<<QString::fromUtf8("\xc2\xa0old\xc2\xa0").toUtf8()<<QString("new")<<QByteArray("new");
    }
    void scalarTokenEditingPreservesMarkup() {
        QFETCH(QByteArray,content);QFETCH(QString,value);QFETCH(QByteArray,expected);
        QTemporaryDir directory;const auto path=directory.filePath("tokens.xcfg");const QByteArray prefix="<Setup><label type=\"string\">",suffix="</label><!-- outside --></Setup>";const auto original=prefix+content+suffix;
        QVERIFY(Write(path,original));XcfgDocument document;QString error;QVERIFY2(document.Load(path,&error),qPrintable(error));const int node=document.Lookup("Setup/label",&error);QVERIFY(node>=0);
        XcfgEditHistory history;QVERIFY2(history.SetValue(&document,node,value,&error),qPrintable(error));QCOMPARE(document.Sources().value(path).bytes,prefix+expected+suffix);QCOMPARE(document.Nodes()[document.Lookup("Setup/label",&error)].value,value);
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QCOMPARE(undo.updatedBytes,original);QVERIFY(history.Apply(&document,undo,&error));QCOMPARE(document.Sources().value(path).bytes,original);
        XcfgEditHistory::Preview redo;QVERIFY(history.CreatePreview(document,true,&redo,&error));QVERIFY(history.Apply(&document,redo,&error));QCOMPARE(document.Sources().value(path).bytes,prefix+expected+suffix);
    }
    void scalarTokenEditingRejectsRangeAndSourceChange() {
        QTemporaryDir directory;const auto root=directory.filePath("root.xcfg"),included=directory.filePath("values.xcfg");const QByteArray original="<Values><n type=\"int\" range=\"[0,9]\"><!-- keep --><![CDATA[2]]></n></Values>";
        QVERIFY(Write(root,"<Setup include=\"values.xcfg\"/>"));QVERIFY(Write(included,original));XcfgDocument document;QString error;QVERIFY(document.Load(root,&error));const auto node=document.Lookup("Setup/n",&error);QVERIFY(node>=0);
        QVERIFY(!document.SetValue(node,"10",&error));QVERIFY(error.contains("outside"));QCOMPARE(document.Sources().value(included).bytes,original);
        QVERIFY(Write(root,"<Setup include=\"values.xcfg\"/><!-- external change -->"));QVERIFY(!document.SetValue(node,"3",&error));QVERIFY(error.contains("changed"));QFile file(included);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void untypedReferencedWhitespaceRemainsScalar() {
        QTemporaryDir directory;const auto path=directory.filePath("spaces.xcfg");QVERIFY(Write(path,"<Setup><label><!-- keep -->old</label></Setup>"));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));const auto node=document.Lookup("Setup/label",&error);QVERIFY(node>=0);
        QVERIFY2(document.SetValue(node," ",&error),qPrintable(error));const auto edited=document.Lookup("Setup/label",&error);QVERIFY(document.Nodes()[edited].scalar);QCOMPARE(document.Nodes()[edited].value,QString(" "));QVERIFY(!document.SetValue(edited,"",&error));
    }
    void splitScalarTextPreservesLegacyTypedValue_data() {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("comment")<<QByteArray("a<!-- comment -->b");
        QTest::newRow("cdata")<<QByteArray("a<![CDATA[b]]>");
        QTest::newRow("instruction")<<QByteArray("a<?old instruction?>b");
    }
    void splitScalarTextPreservesLegacyTypedValue() {
        QFETCH(QByteArray,content);QTemporaryDir directory;const auto path=directory.filePath("split.xcfg");
        const auto bytes="<Setup><label type=\"string\">"+content+"</label></Setup>";QVERIFY(Write(path,bytes));
        XcfgDocument document;QString error;QVERIFY2(document.Load(path,&error),qPrintable(error));
        const int node=document.Lookup("Setup/label",&error);QVERIFY(node>=0);QCOMPARE(document.Nodes()[node].value,QString("b"));
        QVERIFY(!document.Nodes()[node].valueEditable);QVERIFY(!document.SetValue(node,"replacement",&error));QVERIFY(error.contains("read-only"));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);
    }
    void untypedSplitScalarTextRemainsRejected() {
        QTemporaryDir directory;const auto path=directory.filePath("untyped-split.xcfg");
        const QByteArray bytes="<Setup><label>a<!-- comment -->b</label></Setup>";QVERIFY(Write(path,bytes));
        XcfgDocument document;QString error;QVERIFY(!document.Load(path,&error));QVERIFY(error.contains("Untyped scalar text is split"));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);
    }
    void exactStringWhitespaceWritesAndHistory() {
        QTemporaryDir directory;const auto path=directory.filePath("strings.xcfg");
        const QByteArray original="<Setup><Board id=\"*\"><label type=\"string\">old</label></Board><Board id=\"0\"><local type=\"string\">old</local></Board><Board id=\"1\"/></Setup>";
        QVERIFY(Write(path,original));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(path,&error));
        const QString intended="  a   b\t c\n d\r ";const int local=document.Lookup("Setup/Board[0]/local",&error);
        QVERIFY2(history.SetValue(&document,local,intended,&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/local",&error)].value,intended);
        QVERIFY(document.Sources().value(path).bytes.contains("&#32;&#32;a&#32;&#32;&#32;b&#9;"));
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));
        QCOMPARE(document.Sources().value(path).bytes,original);
        XcfgDocument::OverridePreview override;QVERIFY2(document.PreviewOverride("Setup/Board[0]/label",intended,&override,&error),qPrintable(error));
        QVERIFY2(history.ApplyOverride(&document,override,&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/label",&error)].value,intended);
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::AddScalar;request.node=document.Lookup("Setup/Board[0]",&error);
        request.name="added";request.type="string";request.value=intended;
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(history.ApplyStructure(&document,preview,&error));QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/added",&error)].value,intended);
        QVERIFY(history.SetValue(&document,document.Lookup("Setup/Board[*]/label",&error),intended,&error));
        request.operation=XcfgStructureEditor::MaterializeObject;request.node=document.Lookup("Setup/Board[1]",&error);
        QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));QVERIFY(history.ApplyStructure(&document,preview,&error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[1]/label",&error)].value,intended);
        XcfgDocument::Node ranged;ranged.value=intended;ranged.attributes["type"]="string";ranged.attributes["range"]="{\"  a   b\\t c\\n d\\r \"}";
        QVERIFY2(XcfgDocument::CheckValue(ranged,intended,&error),qPrintable(error));QVERIFY(!XcfgDocument::CheckValue(ranged,intended.trimmed(),&error));
    }
    void localIncludeAndImportHistory_data() {
        QTest::addColumn<int>("operation");
        QTest::newRow("replace-include")<<int(XcfgStructureEditor::ReplaceWithInclude);
        QTest::newRow("change-include")<<int(XcfgStructureEditor::ChangeInclude);
        QTest::newRow("import")<<int(XcfgStructureEditor::ImportConfiguration);
    }
    void localIncludeAndImportHistory() {
        QFETCH(int,operation);QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),input=directory.filePath("new config.xcfg"),nested=directory.filePath("nested.xcfg");
        const QByteArray external=R"(<External><!-- import comment --><gain type="int" range="[0,9]">4</gain><group id="*"><n type="int">2</n></group><group id="0"/><sub include="nested.xcfg"/></External>)";
        const QByteArray tail=R"(<Sub><tail type="string" note="keep">a&amp;b</tail></Sub>)";
        const QByteArray original=operation==XcfgStructureEditor::ChangeInclude ? R"(<Setup><!-- root comment --><Board id="0" unknown="keep" include="old.xcfg"><local type="bool">true</local></Board></Setup>)" : R"(<Setup><!-- root comment --><Board id="0" unknown="keep"><old type="int">1</old></Board></Setup>)";
        QVERIFY(Write(main,original));QVERIFY(Write(input,external));QVERIFY(Write(nested,tail));QVERIFY(Write(directory.filePath("old.xcfg"),"<Old><before type=\"int\">1</before></Old>"));
        XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(main,&error));
        XcfgStructureEditor::Request request;request.operation=static_cast<XcfgStructureEditor::Operation>(operation);request.node=document.Lookup("Setup/Board[0]",&error);request.includePath="new config.xcfg";
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.dependencyBytes.value(input),external);QCOMPARE(preview.dependencyBytes.value(nested),tail);
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QVERIFY2(history.ApplyStructure(&document,preview,&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/gain",&error)].value,QString("4"));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/group[0]/n",&error)].value,QString("2"));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/sub/tail",&error)].value,QString("a&b"));
        const int board=document.Lookup("Setup/Board[0]",&error);QCOMPARE(document.Nodes()[board].attributes.value("unknown"),QString("keep"));
        if(operation==XcfgStructureEditor::ChangeInclude)QVERIFY(document.Lookup("Setup/Board[0]/local",&error)>=0);
        else QVERIFY(document.Lookup("Setup/Board[0]/old",&error)<0);
        if(operation==XcfgStructureEditor::ImportConfiguration) {
            QCOMPARE(document.Sources().size(),1);QVERIFY(!document.Nodes()[board].attributes.contains("include"));
            QVERIFY(document.Sources().value(main).bytes.contains("import comment"));
        }else QVERIFY(document.Sources().contains(input));
        const auto modified=document.Sources().value(main).bytes;
        XcfgEditHistory::Preview undo;QVERIFY2(history.CreatePreview(document,false,&undo,&error),qPrintable(error));QVERIFY(history.Apply(&document,undo,&error));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        XcfgEditHistory::Preview redo;QVERIFY(history.CreatePreview(document,true,&redo,&error));QVERIFY(history.Apply(&document,redo,&error));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),modified);file.close();
        QFile foreign(input);QVERIFY(foreign.open(QIODevice::ReadOnly));QCOMPARE(foreign.readAll(),external);
    }
    void importedSourceChangesWithdrawPreviewAndHistory() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),input=directory.filePath("external.xcfg");
        const QByteArray original="<Setup><Board id=\"0\"/></Setup>",external="<External><n type=\"int\">4</n></External>";
        QVERIFY(Write(main,original));QVERIFY(Write(input,external));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(main,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::ImportConfiguration;request.node=document.Lookup("Setup/Board[0]",&error);request.includePath="external.xcfg";
        XcfgStructureEditor::Preview preview;QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QVERIFY(Write(input,external+"<!-- changed comment -->"));QVERIFY(!history.ApplyStructure(&document,preview,&error));
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();QCOMPARE(history.UndoSteps(),0);
        QVERIFY(Write(input,external));QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(history.ApplyStructure(&document,preview,&error));
        const auto imported=document.Sources().value(main).bytes;QVERIFY(Write(input,external+"<!-- after import -->"));
        XcfgEditHistory::Preview undo;QVERIFY(!history.CreatePreview(document,false,&undo,&error));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),imported);
    }
    void includeChangeRejectsCyclesCollisionsAndRemotePaths() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),input=directory.filePath("external.xcfg");
        QVERIFY(Write(main,"<Setup><Board id=\"0\"><n type=\"int\">1</n></Board></Setup>"));QVERIFY(Write(input,"<External><n type=\"int\">2</n></External>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));XcfgStructureEditor::Request r;r.operation=XcfgStructureEditor::ChangeInclude;r.node=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview p;r.includePath="external.xcfg";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));
        r.operation=XcfgStructureEditor::ReplaceWithInclude;r.includePath="main.xcfg";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));
        r.includePath="https://example.invalid/config.xcfg";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));
        r.includePath="missing.xcfg";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));
        r.includePath="file:";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));QVERIFY(!error.isEmpty());
        QVERIFY(Write(input,"<n type=\"int\">2</n>"));r.includePath="external.xcfg";QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));
    }
    void removeIncludeRetainsLocalDeclarations() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg"),input=directory.filePath("external.xcfg");
        QVERIFY(Write(input,"<External><inherited type=\"int\">2</inherited></External>"));QVERIFY(Write(main,"<Setup><Board id=\"0\" include=\"external.xcfg\"><local type=\"int\">1</local></Board></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));XcfgStructureEditor::Request r;r.operation=XcfgStructureEditor::ChangeInclude;r.node=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview p;QVERIFY(XcfgStructureEditor::CreatePreview(document,r,&p,&error));QVERIFY(XcfgStructureEditor::Apply(&document,p,&error));
        QVERIFY(document.Lookup("Setup/Board[0]/local",&error)>=0);QVERIFY(document.Lookup("Setup/Board[0]/inherited",&error)<0);QCOMPARE(document.Sources().size(),1);
    }
    void importedCombinedDepthIsBoundedBeforeWriting() {
        QTemporaryDir directory;const auto main=directory.filePath("main.xcfg");const QByteArray original="<Setup><Board id=\"0\"/></Setup>";QVERIFY(Write(main,original));
        for(int file=0;file<3;++file) {
            QByteArray xml="<External>";for(int depth=0;depth<80;++depth)xml+="<level>";
            xml+=file<2 ? QByteArray("<next include=\"file")+QByteArray::number(file+1)+".xcfg\"/>" : QByteArray("<n type=\"int\">1</n>");
            for(int depth=0;depth<80;++depth)xml+="</level>";xml+="</External>";
            QVERIFY(Write(directory.filePath(QString("file%1.xcfg").arg(file)),xml));
        }
        XcfgDocument document;QString error;QVERIFY(document.Load(main,&error));XcfgStructureEditor::Request r;r.operation=XcfgStructureEditor::ImportConfiguration;
        r.node=document.Lookup("Setup/Board[0]",&error);r.includePath="file0.xcfg";XcfgStructureEditor::Preview p;
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,r,&p,&error));QVERIFY(error.contains("depth"));
        QFile file(main);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void historyAllStructureOperations_data() {
        QTest::addColumn<int>("operation");
        for(int i=0;i<10;++i) QTest::newRow(qPrintable(QString::number(i)))<<i;
    }
    void historyAllStructureOperations() {
        QFETCH(int,operation);QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray original=R"(<Setup><!-- keep --><Board id="*"><inherited type="int">9</inherited></Board><Board id="0"><n type="int">3</n><z type="int">4</z><group><n type="int">5</n></group></Board></Setup>)";
        QVERIFY(Write(path,original));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=static_cast<XcfgStructureEditor::Operation>(operation);
        const int board=document.Lookup("Setup/Board[0]",&error),n=document.Lookup("Setup/Board[0]/n",&error),z=document.Lookup("Setup/Board[0]/z",&error);
        request.node=board;
        if(operation==0) {request.name="Extra";request.id="a";}
        if(operation==1) {request.name="extra";request.type="int";request.value="6";}
        if(operation==2) request.node=z;
        if(operation==3) {request.node=board;request.id="1";}
        if(operation==5) {request.node=z;request.name="renamed";}
        if(operation==6) {request.node=document.Lookup("Setup/Board[0]/group",&error);request.id="copy";}
        if(operation==7) {request.node=z;request.sibling=n;}
        if(operation==8) {request.node=n;request.sibling=z;}
        if(operation==9) {request.node=n;request.type="int";request.unit="mV";request.range="[0,9]";}
        XcfgStructureEditor::Preview edit;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&edit,&error),qPrintable(error));
        QVERIFY2(history.ApplyStructure(&document,edit,&error),qPrintable(error));QCOMPARE(history.UndoSteps(),1);
        QFile saved(path);QVERIFY(saved.open(QIODevice::ReadOnly));const auto modified=saved.readAll();saved.close();QVERIFY(modified!=original);
        XcfgEditHistory::Preview undo;QVERIFY2(history.CreatePreview(document,false,&undo,&error),qPrintable(error));
        QCOMPARE(undo.updatedBytes,original);QVERIFY2(history.Apply(&document,undo,&error),qPrintable(error));
        QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),original);saved.close();QCOMPARE(history.RedoSteps(),1);
        XcfgEditHistory::Preview redo;QVERIFY2(history.CreatePreview(document,true,&redo,&error),qPrintable(error));
        QCOMPARE(redo.updatedBytes,modified);QVERIFY(history.Apply(&document,redo,&error));
        QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),modified);
    }
    void historyIncludeScalarAndConcreteOverrideAreReversible() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg"),include=directory.filePath("default.xcfg");
        const QByteArray original=R"(<Setup><Board id="*" include="default.xcfg"/><Board id="0"/><Board id="1"/></Setup>)";
        const QByteArray shared=R"(<Board><!-- preserve --><n type="int">2</n></Board>)";
        QVERIFY(Write(path,original));QVERIFY(Write(include,shared));XcfgDocument document;XcfgEditHistory history;QString error;
        QVERIFY(document.Load(path,&error));int n=document.Lookup("Setup/Board[0]/n",&error);QVERIFY(n>=0);
        QVERIFY2(history.SetValue(&document,n,"3",&error),qPrintable(error));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[1]/n",&error)].value,QString("3"));
        XcfgDocument::OverridePreview edit;QVERIFY2(document.PreviewOverride("Setup/Board[0]/n","7",&edit,&error),qPrintable(error));
        QVERIFY2(history.ApplyOverride(&document,edit,&error),qPrintable(error));QCOMPARE(history.UndoSteps(),2);
        for(int i=0;i<2;++i) {XcfgEditHistory::Preview undo;QVERIFY2(history.CreatePreview(document,false,&undo,&error),qPrintable(error));QVERIFY(history.Apply(&document,undo,&error));}
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        file.setFileName(include);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),shared);file.close();
        for(int i=0;i<2;++i) {XcfgEditHistory::Preview redo;QVERIFY(history.CreatePreview(document,true,&redo,&error));QVERIFY(history.Apply(&document,redo,&error));}
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/n",&error)].value,QString("7"));
        QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[1]/n",&error)].value,QString("3"));
    }
    void historyMaterializationGuardsRemovedIncludes() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg"),include=directory.filePath("default.xcfg");
        const QByteArray original=R"(<Setup><Board id="0" include="default.xcfg"/></Setup>)",shared=R"(<Board><n type="int">2</n></Board>)";
        QVERIFY(Write(path,original));QVERIFY(Write(include,shared));XcfgDocument document;XcfgEditHistory history;QString error;
        QVERIFY(document.Load(path,&error));XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::MaterializeObject;request.node=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview edit;QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&edit,&error));QVERIFY(history.ApplyStructure(&document,edit,&error));
        QCOMPARE(document.Sources().size(),1);XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));
        QVERIFY(Write(include,R"(<Board><n type="int">8</n></Board>)"));
        QVERIFY(!history.CreatePreview(document,false,&undo,&error));QVERIFY(error.contains("dependency changed"));QCOMPARE(history.UndoSteps(),1);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto unchanged=file.readAll();file.close();
        QVERIFY(!history.Apply(&document,undo,&error));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),unchanged);file.close();
        QVERIFY(Write(include,shared));QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));
        QCOMPARE(document.Sources().size(),2);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void historyTamperingStalePreviewAndExternalChangesDoNotWrite() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray original=R"(<Setup><n type="int" range="[0,10]">2</n></Setup>)";
        QVERIFY(Write(path,original));XcfgDocument document;XcfgEditHistory history;QString error;QVERIFY(document.Load(path,&error));
        QVERIFY(history.SetValue(&document,document.Lookup("Setup/n",&error),"3",&error));
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));auto changed=undo;changed.updatedBytes.append("broken");
        QVERIFY(!history.Apply(&document,changed,&error));QCOMPARE(history.UndoSteps(),1);
        // 参数校验失败不清空已存在的撤销记录。
        QVERIFY(!history.SetValue(&document,document.Lookup("Setup/n",&error),"99",&error));QCOMPARE(history.UndoSteps(),1);
        QVERIFY(history.SetValue(&document,document.Lookup("Setup/n",&error),"4",&error));
        QVERIFY(!history.Apply(&document,undo,&error));QVERIFY(error.contains("History changed"));
        QByteArray external=original;external.replace(">2<",">5<");QVERIFY(Write(path,external));
        QVERIFY(!history.CreatePreview(document,false,&undo,&error));QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),external);
    }
    void historyNewBranchAndEntryBudgetDiscardOnlyOldestSteps() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");QVERIFY(Write(path,R"(<Setup><n type="int">2</n></Setup>)"));
        XcfgDocument document;XcfgEditHistory history(2);QString error;QVERIFY(document.Load(path,&error));
        for(int i=3;i<=6;++i) QVERIFY(history.SetValue(&document,document.Lookup("Setup/n",&error),QString::number(i),&error));
        QCOMPARE(history.UndoSteps(),2);for(int i=0;i<2;++i) {XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(history.Apply(&document,undo,&error));}
        QCOMPARE(document.Nodes()[document.Lookup("Setup/n",&error)].value,QString("4"));QCOMPARE(history.RedoSteps(),2);
        QVERIFY(history.SetValue(&document,document.Lookup("Setup/n",&error),"8",&error));QCOMPARE(history.RedoSteps(),0);QCOMPARE(history.UndoSteps(),1);
        history.Clear();QCOMPARE(history.UndoSteps(),0);QCOMPARE(history.RedoSteps(),0);QCOMPARE(history.RetainedBytes(),qint64(0));
    }
    void historyByteBudgetIsBoundedForLargePreservedXml() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray original="<!--"+QByteArray(4*1024*1024,'x')+"--><Setup><n type=\"int\">2</n></Setup>";
        QVERIFY(Write(path,original));XcfgDocument document;XcfgEditHistory history(32,32*1024*1024);QString error;QVERIFY(document.Load(path,&error));
        for(int i=3;i<=7;++i) QVERIFY2(history.SetValue(&document,document.Lookup("Setup/n",&error),QString::number(i),&error),qPrintable(error));
        QVERIFY(history.RetainedBytes()<=32*1024*1024);QCOMPARE(history.UndoSteps(),3);
        XcfgEditHistory::Preview undo;QVERIFY(history.CreatePreview(document,false,&undo,&error));QVERIFY(undo.updatedBytes.startsWith("<!--"));
    }
    void structureScalarPropertiesPreserveTokensAndReviewEverySharedMember() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray bytes="<Setup><Board id=\"0\"><gain extra=\"keep\" unit=\"V\" range=\"[0,10]\"><![CDATA[2]]><!-- token --></gain></Board><Board id=\"1\"><gain>3</gain></Board></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::ChangeScalarProperties;request.node=document.Lookup("Setup/Board[0]/gain",&error);
        request.type="int";request.unit="mV";request.range="[0,1]";XcfgStructureEditor::Preview preview;
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.range="[0,10]";QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.report.value("scalar_changes").toArray().size(),2);
        QVERIFY(preview.updatedBytes.contains("<![CDATA[2]]><!-- token -->"));QVERIFY(preview.updatedBytes.contains("extra=\"keep\""));
        QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[1]/gain",&value,&properties,&error));QCOMPARE(properties.unit,QString("mV"));
        request.node=document.Lookup("Setup/Board[0]/gain",&error);request.type.clear();request.unit.clear();request.range.clear();
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        QVERIFY(document.ResolveScalar("Setup/Board[1]/gain",&value,&properties,&error));QVERIFY(properties.unit.isEmpty());QVERIFY(properties.range.isEmpty());
    }
    void structureCloneRenameAndMovePreserveSubtreesAndReviewPrecedence() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        QVERIFY(Write(path,"<Setup><!-- untouched --><Board id=\"0\" extra=\"keep\"><gain unit=\"V\">2</gain><!-- child --></Board><Board id=\"1\"><gain unit=\"mV\">3</gain></Board></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::MoveBefore;
        request.node=document.Lookup("Setup/Board[1]",&error);request.sibling=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.report.value("scalar_changes").toArray().size(),2);
        QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        XcfgDocument::Node scalar;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[1]/gain",&scalar,&properties,&error));QCOMPARE(properties.unit,QString("V"));
        request.operation=XcfgStructureEditor::CloneDeclaration;request.node=document.Lookup("Setup/Board[0]",&error);request.id="2";
        QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(preview.updatedBytes.contains("<!-- child -->"));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        request.operation=XcfgStructureEditor::ChangeName;request.node=document.Lookup("Setup/Board[2]",&error);request.name="Reference";
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        QVERIFY(document.Lookup("Setup/Reference[2]/gain",&error)>=0);
        request.operation=XcfgStructureEditor::MoveAfter;request.node=document.Lookup("Setup/Board[1]",&error);request.sibling=document.Lookup("Setup/Reference[2]",&error);
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
        QVERIFY(bytes.contains("<!-- untouched -->"));QVERIFY(bytes.contains("extra=\"keep\""));
        QVERIFY(bytes.indexOf("<Reference")<bytes.indexOf("<Board id=\"1\""));
    }
    void structureCloneCollisionAndCrossParentMovesAreRejected() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray bytes="<Setup><Board id=\"0\"><gain>2</gain></Board><Board id=\"1\"><gain>3</gain></Board></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::CloneDeclaration;request.node=document.Lookup("Setup/Board[0]",&error);request.id="1";
        XcfgStructureEditor::Preview preview;QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.operation=XcfgStructureEditor::MoveAfter;request.node=document.Lookup("Setup/Board[0]/gain",&error);request.sibling=document.Lookup("Setup/Board[1]/gain",&error);
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.sibling=request.node;QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.operation=XcfgStructureEditor::ChangeName;request.node=document.Lookup("Setup/Board[0]",&error);request.name="invalid/name";
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);file.close();
        QVERIFY(Write(path,"<Setup><Template id=\"*\"><Empty/><Empty/></Template></Setup>"));
        QVERIFY(document.Load(path,&error));request.operation=XcfgStructureEditor::AddObject;request.node=0;request.name="Board";request.id="2";
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("collision"));
    }
    void structureInsertRenameRemovePreservesUntouchedXml() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray original="<Setup><!-- keep --><Board id=\"0\" extra=\"preserve\"><gain range=\"[0,10]\">2</gain></Board></Setup>";
        QVERIFY(Write(path,original));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;XcfgStructureEditor::Preview preview;
        request.node=0;request.name="Board";request.id="1";
        QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(preview.report.value("effective_changed").toBool());
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QVERIFY2(XcfgStructureEditor::Apply(&document,preview,&error),qPrintable(error));
        request.operation=XcfgStructureEditor::AddScalar;request.node=document.Lookup("Setup/Board[1]",&error);
        request.name="label";request.id.clear();request.type="string";request.value="<&\"hello";
        QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY2(XcfgStructureEditor::Apply(&document,preview,&error),qPrintable(error));
        // This is an editor-preservation fixture, not a legacy-loadable
        // hardware configuration: its untouched Board[0] deliberately has an
        // unknown attribute. Inspect the edited value through the document
        // model; hardware compilation of this source is tested as a rejection.
        const int label=document.Lookup("Setup/Board[1]/label",&error);QVERIFY(label>=0);
        QCOMPARE(document.Nodes()[label].value,QString("<&\"hello"));
        request.operation=XcfgStructureEditor::ChangeId;request.node=document.Lookup("Setup/Board[1]",&error);request.id="2";
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        QVERIFY(document.Lookup("Setup/Board[2]/label",&error)>=0);
        request.operation=XcfgStructureEditor::RemoveDeclaration;request.node=document.Lookup("Setup/Board[2]",&error);
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        QVERIFY(file.open(QIODevice::ReadOnly));const auto saved=file.readAll();
        QVERIFY(saved.contains("<!-- keep -->"));QVERIFY(saved.contains("<Board id=\"0\" extra=\"preserve\"><gain range=\"[0,10]\">2</gain></Board>"));
        QVERIFY(document.Lookup("Setup/Board[2]",&error)<0);
    }
    void structurePreviewBindsEverySourceAndReviewedDifference() {
        QTemporaryDir directory;const auto path=directory.filePath("root.xcfg"),included=directory.filePath("inc.xcfg");
        QVERIFY(Write(included,"<Template><gain>2</gain></Template>"));
        QVERIFY(Write(path,"<Setup><Board id=\"0\" include=\"inc.xcfg\"/></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::AddObject;request.node=0;request.name="Board";request.id="1";
        XcfgStructureEditor::Preview preview;QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        auto modified=preview;modified.report["effective_changed"]=false;
        QVERIFY(!XcfgStructureEditor::Apply(&document,modified,&error));QVERIFY(error.contains("preview differs"));
        modified=preview;modified.updatedBytes.replace("id=\"1\"","id=\"2\"");
        QVERIFY(!XcfgStructureEditor::Apply(&document,modified,&error));
        QVERIFY(Write(included,"<Template><gain>3</gain></Template>"));
        QVERIFY(!XcfgStructureEditor::Apply(&document,preview,&error));QVERIFY(error.contains("Source changed"));
    }
    void structureRejectsDuplicateInvalidRangeAndRootDeletion() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray bytes="<Setup><Board id=\"0\"/><Board id=\"1\"/></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.node=0;request.name="Board";request.id="0";XcfgStructureEditor::Preview preview;
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.operation=XcfgStructureEditor::ChangeId;request.node=document.Lookup("Setup/Board[0]",&error);request.id="1";
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.operation=XcfgStructureEditor::RemoveDeclaration;request.node=0;QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.operation=XcfgStructureEditor::AddScalar;request.name="gain";request.id.clear();request.type="int";request.range="[0,10]";request.value="11";
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        request.range.clear();request.value="3";request.name="bad><element";QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);
    }
    void structureMaterializesInheritedSubtreeWithoutChangingEffectiveValues() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        const QByteArray bytes="<Setup><Board id=\"*\"><Module><gain type=\"int\" unit=\"mV\" range=\"[0,10]\">2</gain><label type=\"string\"></label><Empty/></Module></Board>"
            "<Board id=\"0\" extra=\"preserve\"/><Board id=\"1\"/></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::MaterializeObject;request.node=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(!preview.report.value("effective_changed").toBool());QVERIFY(preview.report.value("snapshot_changed").toBool());
        QVERIFY2(XcfgStructureEditor::Apply(&document,preview,&error),qPrintable(error));
        XcfgDocument::Node value;XcfgDocument::ScalarProperties info;
        QVERIFY(document.ResolveScalar("Setup/Board[0]/Module/gain",&value,&info,&error));QCOMPARE(value.path,QString("Setup/Board[0]/Module/gain"));
        QCOMPARE(value.value,QString("2"));QVERIFY(document.Lookup("Setup/Board[0]/Module/Empty",&error)>=0);
        XcfgDocument::Node inherited;XcfgDocument::ScalarProperties inheritedProperties;
        QVERIFY(document.ResolveScalar("Setup/Board[1]/Module/gain",&inherited,&inheritedProperties,&error));
        QCOMPARE(inherited.value,QString("2"));QCOMPARE(inheritedProperties.unit,QString("mV"));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("extra=\"preserve\""));
    }
    void structureMaterializesIncludeAndPreservesLocalCommentsAndQuotedAttributes() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg"),included=directory.filePath("inc.xcfg");
        const QByteArray includeBytes="<Template><Module><gain unit=\"mV\" range=\"[0,10]\">2</gain><Empty/></Module></Template>";
        QVERIFY(Write(included,includeBytes));
        QVERIFY(Write(path,"<Setup><Board extra=\"id='fake' > include='keep'\" id='0' include='inc.xcfg'><!-- local --><own>3</own></Board></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::MaterializeObject;request.node=document.Lookup("Setup/Board[0]",&error);
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(!preview.report.value("effective_changed").toBool());QVERIFY(preview.updatedBytes.contains("<!-- local --><own>3</own>"));
        QVERIFY(preview.updatedBytes.contains("extra=\"id='fake' > include='keep'\""));
        QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));QCOMPARE(document.Sources().size(),1);
        QFile file(included);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),includeBytes);
        request.operation=XcfgStructureEditor::ChangeId;request.node=document.Lookup("Setup/Board[0]",&error);request.id="new&<id";
        QVERIFY(XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
        QVERIFY(preview.updatedBytes.contains("extra=\"id='fake' > include='keep'\""));
        QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));QVERIFY(document.Lookup("Setup/Board[new&<id]/own",&error)>=0);
    }
    void structureSharedMetadataChangesAreAllIncludedInReview() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg");
        QVERIFY(Write(path,"<Setup><Board id=\"0\"><gain unit=\"V\" range=\"[0,10]\">2</gain></Board><Board id=\"1\"/></Setup>"));
        XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::AddScalar;request.node=document.Lookup("Setup/Board[1]",&error);
        request.name="gain";request.type="int";request.value="3";request.unit="mV";
        XcfgStructureEditor::Preview preview;QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QCOMPARE(preview.report.value("scalar_changes").toArray().size(),2);
        QVERIFY(preview.report.value("effective_changed").toBool());
        QVERIFY(XcfgStructureEditor::Apply(&document,preview,&error));
        XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[0]/gain",&value,&properties,&error));QCOMPARE(value.value,QString("2"));QCOMPARE(properties.unit,QString("mV"));
        request.node=document.Lookup("Setup/Board[1]",&error);request.name="other";request.type="real";request.range="[0,1]";request.value="1.5";
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));
    }
    void structureSelfClosingIncludeAndDuplicateImportRemainExplicit() {
        QTemporaryDir directory;const auto path=directory.filePath("tree.xcfg"),included=directory.filePath("inc.xcfg");
        QVERIFY(Write(included,"<Template><Empty/><label type=\"string\"></label></Template>"));
        const QByteArray bytes="<Setup><Board include=\"inc.xcfg\" extra=\"x > y\" id=\"0\"/></Setup>";
        QVERIFY(Write(path,bytes));XcfgDocument document;QString error;QVERIFY(document.Load(path,&error));
        XcfgStructureEditor::Request request;request.node=document.Lookup("Setup/Board[0]",&error);request.name="Empty";XcfgStructureEditor::Preview preview;
        QVERIFY(!XcfgStructureEditor::CreatePreview(document,request,&preview,&error));QVERIFY(error.contains("collision"));
        request.operation=XcfgStructureEditor::MaterializeObject;
        QVERIFY2(XcfgStructureEditor::CreatePreview(document,request,&preview,&error),qPrintable(error));
        QVERIFY(!preview.report.value("effective_changed").toBool());QVERIFY(preview.updatedBytes.contains("extra=\"x > y\""));
        QVERIFY2(XcfgStructureEditor::Apply(&document,preview,&error),qPrintable(error));QCOMPARE(document.Sources().size(),1);
        QVERIFY(document.Lookup("Setup/Board[0]/Empty",&error)>=0);
        const int label=document.Lookup("Setup/Board[0]/label",&error);QVERIFY(label>=0);QCOMPARE(document.Nodes()[label].value,QString());
    }
    void effectiveSnapshotPreservesUtf16PathOrdering() {
        QTemporaryDir directory; const auto root=directory.filePath("unicode.xcfg"),saved=directory.filePath("unicode.json");
        const auto supplementary=QString::fromUtf8("\xf0\x90\x80\x80"),privateUse=QString::fromUtf8("\xee\x80\x80");
        const auto xml=QString("<Setup><Item id=\"%1\">1</Item><Item id=\"%2\">2</Item></Setup>").arg(supplementary,privateUse).toUtf8();
        QVERIFY(Write(root,xml)); XcfgDocument document; QString error; QJsonObject a,b;
        QVERIFY(document.Load(root,&error)); QVERIFY2(EffectiveConfigurationSnapshot::Capture(document,&a,&error),qPrintable(error));
        QCOMPARE(a["scalars"].toArray()[0].toObject()["path"].toString(),"Setup/Item["+supplementary+"]");
        QVERIFY(EffectiveConfigurationSnapshot::Save(a,saved,&error)); QVERIFY(EffectiveConfigurationSnapshot::Load(saved,&b,&error)); QCOMPARE(a,b);
    }
    void effectiveSnapshotRoundTripAndTypedProvenance() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg"),saved=directory.filePath("snapshot.json");
        QVERIFY(Write(child,"<Template><mask type=\"hex\" range=\"[0x0,0xffffffffffffffff]\">ffffffffffffffff</mask>"
            "<enabled type=\"bool\">TRUE</enabled><label type=\"string\"></label></Template>"));
        QVERIFY(Write(root,"<Setup><Board id=\"*\" include=\"child.xcfg\"/><Board id=\"0\"/><Board id=\"1\"/></Setup>"));
        XcfgDocument document; QString error; QJsonObject snapshot,loaded;
        QVERIFY(document.Load(root,&error)); QVERIFY2(EffectiveConfigurationSnapshot::Capture(document,&snapshot,&error),qPrintable(error));
        QCOMPARE(snapshot["scalars"].toArray().size(),6); QCOMPARE(snapshot["objects"].toArray().size(),3);
        const auto scalar=snapshot["scalars"].toArray()[2].toObject(); QCOMPARE(scalar["path"].toString(),QString("Setup/Board[0]/mask"));
        QCOMPARE(scalar["normalized_value"].toString(),QString("0xffffffffffffffff")); QCOMPARE(scalar["value_source"].toObject()["file"].toString(),child);
        QCOMPARE(snapshot["scalars"].toArray()[0].toObject()["normalized_value"].toString(),QString("true"));
        QVERIFY(EffectiveConfigurationSnapshot::Save(snapshot,saved,&error)); QVERIFY(EffectiveConfigurationSnapshot::Load(saved,&loaded,&error)); QCOMPARE(snapshot,loaded);
        QVERIFY(!EffectiveConfigurationSnapshot::Save(snapshot,saved,&error));
        QJsonObject report; QVERIFY(EffectiveConfigurationSnapshot::Compare(snapshot,loaded,&report,&error));
        QVERIFY(!report["effective_changed"].toBool()); QVERIFY(!report["snapshot_changed"].toBool()); QVERIFY(report["scalar_changes"].toArray().isEmpty());
    }
    void effectiveSnapshotSeparatesSemanticsFromSourceBytesAndLocation() {
        QTemporaryDir directory; const auto first=directory.filePath("first.xcfg"),second=directory.filePath("second.xcfg");
        QVERIFY(Write(first,"<Setup><Board id=\"*\"><gain range=\"{1,2,1}\">01</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>"));
        QVERIFY(Write(second,"<Setup><!-- changed comment --><Board id=\"*\"><gain range=\"{2,1}\">1</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>"));
        XcfgDocument document; QString error; QJsonObject a,b,report;
        QVERIFY(document.Load(first,&error)); QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&a,&error));
        QVERIFY(document.Load(second,&error)); QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&b,&error));
        QCOMPARE(a["effective_sha256"],b["effective_sha256"]); QVERIFY(a["snapshot_sha256"]!=b["snapshot_sha256"]);
        QVERIFY(EffectiveConfigurationSnapshot::Compare(a,b,&report,&error)); QVERIFY(!report["effective_changed"].toBool()); QVERIFY(report["snapshot_changed"].toBool());
        QCOMPARE(report["scalar_changes"].toArray().size(),2); QCOMPARE(report["source_changes"].toArray().size(),2);
        const auto fields=report["scalar_changes"].toArray()[0].toObject()["fields"].toArray();
        QVERIFY(fields.contains("range_text")); QVERIFY(fields.contains("text_or_declared_type")); QVERIFY(!fields.contains("value"));
    }
    void effectiveSnapshotReportsEveryAffectedInstanceAndObject() {
        QTemporaryDir directory; const auto root=directory.filePath("root.xcfg"); QString error; XcfgDocument document; QJsonObject a,b,report;
        QVERIFY(Write(root,"<Setup><Board id=\"*\"><gain range=\"[0,10]\">2</gain></Board><Board id=\"0\"/><Board id=\"1\"/><Empty/></Setup>"));
        QVERIFY(document.Load(root,&error)); QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&a,&error));
        QVERIFY(document.SetValue(document.Lookup("Setup/Board[*]/gain",&error),"3",&error)); QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&b,&error));
        QVERIFY(EffectiveConfigurationSnapshot::Compare(a,b,&report,&error)); QVERIFY(report["effective_changed"].toBool()); QCOMPARE(report["scalar_changes"].toArray().size(),2);
        for (const auto &item:report["scalar_changes"].toArray()) QVERIFY(item.toObject()["fields"].toArray().contains("value"));
        QVERIFY(Write(root,"<Setup><Board id=\"*\"><gain range=\"[0,10]\">3</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>"));
        QVERIFY(document.Load(root,&error)); QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&a,&error));
        QVERIFY(EffectiveConfigurationSnapshot::Compare(b,a,&report,&error)); QVERIFY(report["effective_changed"].toBool());
        QCOMPARE(report["object_changes"].toArray().size(),1); QVERIFY(report["scalar_changes"].toArray().isEmpty());
    }
    void effectiveSnapshotRejectsTamperingStaleSourcesAndInvalidValues() {
        QTemporaryDir directory; const auto root=directory.filePath("root.xcfg"),saved=directory.filePath("bad.json"); XcfgDocument document; QString error;
        QVERIFY(Write(root,"<Setup><gain range=\"[0,10]\">2</gain></Setup>")); QVERIFY(document.Load(root,&error)); QJsonObject valid,keep; keep["keep"]=true;
        QVERIFY(EffectiveConfigurationSnapshot::Capture(document,&valid,&error));
        auto altered=valid; altered["effective_sha256"]=QString(64,'0'); QVERIFY(!EffectiveConfigurationSnapshot::Validate(altered,&error));
        altered=valid; auto entries=altered["scalars"].toArray(); auto scalar=entries[0].toObject(); scalar["normalized_value"]="3"; entries[0]=scalar; altered["scalars"]=entries;
        QVERIFY(!EffectiveConfigurationSnapshot::Validate(altered,&error));
        auto bytes=QJsonDocument(valid).toJson(QJsonDocument::Compact); bytes.insert(1,"\"schema_version\":1,"); QVERIFY(Write(saved,bytes));
        QVERIFY(!EffectiveConfigurationSnapshot::Load(saved,&keep,&error)); QVERIFY(error.contains("Duplicate")); QVERIFY(keep["keep"].toBool());
        altered=valid; altered["unexpected"]=true; QVERIFY(Write(saved,QJsonDocument(altered).toJson())); QVERIFY(!EffectiveConfigurationSnapshot::Load(saved,&keep,&error));
        QVERIFY(Write(saved,bytes+QByteArray(1,char(0xff)))); QVERIFY(!EffectiveConfigurationSnapshot::Load(saved,&keep,&error));
        QVERIFY(Write(root,"<Setup><gain range=\"[0,10]\">20</gain></Setup>")); QVERIFY(!EffectiveConfigurationSnapshot::Capture(document,&keep,&error)); QVERIFY(error.contains("Source changed"));
        QVERIFY(document.Load(root,&error)); QVERIFY(!EffectiveConfigurationSnapshot::Capture(document,&keep,&error)); QVERIFY(keep["keep"].toBool());
        QVERIFY(!EffectiveConfigurationSnapshot::Save(keep,directory.filePath("not-published.json"),&error)); QVERIFY(!QFile::exists(directory.filePath("not-published.json")));
    }
    void effectiveSnapshotBudgetsDoNotRelaxControlJsonDefaults() {
        QTemporaryDir directory; const auto root=directory.filePath("large.xcfg"),saved=directory.filePath("large.json");
        QByteArray xml="<Setup>"; for (int i=0;i<4100;++i) xml+="<Item id=\""+QByteArray::number(i)+"\" type=\"int\">"+QByteArray::number(i)+"</Item>";
        xml+="<Label type=\"string\">"+QByteArray(5000,'a')+"</Label></Setup>"; QVERIFY(Write(root,xml));
        XcfgDocument document; QString error; QJsonObject a,b; QVERIFY(document.Load(root,&error)); QVERIFY2(EffectiveConfigurationSnapshot::Capture(document,&a,&error),qPrintable(error));
        QVERIFY(EffectiveConfigurationSnapshot::Save(a,saved,&error)); QVERIFY2(EffectiveConfigurationSnapshot::Load(saved,&b,&error),qPrintable(error)); QCOMPARE(a,b);
        bool rejected=false; try { GetDAQ::ParseStrictJson("\""+std::string(5000,'a')+"\""); } catch (const std::exception &) { rejected=true; } QVERIFY(rejected);
        std::string array="[0"; for (int i=0;i<4096;++i) array+=",0"; array+=']';
        rejected=false; try { GetDAQ::ParseStrictJson(array); } catch (const std::exception &) { rejected=true; } QVERIFY(rejected);
    }
    void sharedMetadataDoesNotShareValuesOrTypes() {
        QTemporaryDir directory; const auto file=directory.filePath("setup.xcfg");
        QVERIFY(Write(file,"<Setup><Board id=\"*\"><gain unit=\"V\" range=\"[-inf,+inf]\">2</gain></Board>"
            "<Board id=\"0\"><gain type=\"real\">4.0</gain><gain id=\"1\" unit=\"mV\">5</gain></Board></Setup>"));
        XcfgDocument document; QString error; QVERIFY(document.Load(file,&error));
        XcfgDocument::Node value; XcfgDocument::ScalarProperties info;
        QVERIFY2(document.ResolveScalar("Setup/Board[0]/gain",&value,&info,&error),qPrintable(error));
        QCOMPARE(value.value,QString("4.0")); QCOMPARE(value.attributes.value("type"),QString("real"));
        QCOMPARE(info.unit,QString("mV")); QCOMPARE(info.range,QString("[-inf,+inf]"));
        ExperimentReader reader(document); QCOMPARE(reader.Scalar("Setup/Board[0]/gain",QString(),"mV",false,true),QString("4.0"));
        QVERIFY(reader.parameters.last().toObject().contains("shared_metadata"));
    }
    void sharedIncludeDeclarationChecksEveryContext() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg");
        const QByteArray original="<Template><gain>5</gain></Template>";
        QVERIFY(Write(child,original));
        QVERIFY(Write(root,"<Setup><A include=\"child.xcfg\"/><A id=\"1\"><gain range=\"[0,20]\">2</gain></A>"
            "<B include=\"child.xcfg\"/><B id=\"1\"><gain range=\"[0,10]\">2</gain></B></Setup>"));
        XcfgDocument document; QString error; QVERIFY(document.Load(root,&error));
        const int source=document.Lookup("Setup/A/gain",&error); QVERIFY(source>=0);
        QVector<XcfgDocument::ScalarProperties> contexts;
        QVERIFY(document.DeclarationProperties(source,&contexts,&error)); QCOMPARE(contexts.size(),2);
        QVERIFY(!document.SetValue(source,"15",&error)); QVERIFY(error.startsWith("Setup/B/gain"));
        QFile file(child); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original); file.close();
        QVERIFY2(document.SetValue(source,"8",&error),qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("<Template><gain>8</gain></Template>"));
    }
    void includeClassInfoMustMatchExistingClass() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg");
        QVERIFY(Write(child,"<Template><gain unit=\"V\" range=\"[0,10]\">5</gain></Template>"));
        QVERIFY(Write(root,"<Setup><A id=\"0\"><gain unit=\"V\" range=\"[0,10]\">2</gain></A><A id=\"1\" include=\"child.xcfg\"/></Setup>"));
        XcfgDocument document; QString error; XcfgDocument::Node value; XcfgDocument::ScalarProperties info;
        QVERIFY(document.Load(root,&error)); QVERIFY2(document.ResolveScalar("Setup/A[1]/gain",&value,&info,&error),qPrintable(error));
        QVERIFY(Write(root,"<Setup><A id=\"0\"><gain unit=\"mV\" range=\"[0,10]\">2</gain></A><A id=\"1\" include=\"child.xcfg\"/></Setup>"));
        QVERIFY(document.Load(root,&error)); QVERIFY(!document.ResolveScalar("Setup/A[1]/gain",&value,&info,&error));
        QVERIFY(error.contains("metadata conflict"));
    }
    void includedClassInfoJoinsMemberWithoutLocalMetadata() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg");
        QVERIFY(Write(child,"<Template><gain unit=\"V\" range=\"[0,10]\">5</gain></Template>"));
        QVERIFY(Write(root,"<Setup><A id=\"0\"><gain>2</gain></A><A id=\"1\" include=\"child.xcfg\"/></Setup>"));
        XcfgDocument document; QString error; XcfgDocument::Node value; XcfgDocument::ScalarProperties info;
        QVERIFY2(document.Load(root,&error),qPrintable(error));
        QVERIFY2(document.ResolveScalar("Setup/A[0]/gain",&value,&info,&error),qPrintable(error));
        QCOMPARE(info.unit,QString("V")); QCOMPARE(info.range,QString("[0,10]"));
        const int local=document.Lookup("Setup/A[0]/gain",&error); QVERIFY(local>=0);
        QVERIFY(!document.SetValue(local,"11",&error)); QVERIFY(error.contains("[0,10]"));
        QVERIFY2(document.SetValue(local,"8",&error),qPrintable(error));
        QVERIFY2(document.ResolveScalar("Setup/A[1]/gain",&value,&info,&error),qPrintable(error));
        QCOMPARE(info.unit,QString("V")); QCOMPARE(info.range,QString("[0,10]"));
    }
    void externalMetadataChangePreventsEditingAnotherFile() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg");
        QVERIFY(Write(child,"<Template><gain range=\"[0,10]\">2</gain></Template>"));
        const QByteArray original="<Setup><A id=\"*\" include=\"child.xcfg\"/><A id=\"0\"><gain>5</gain></A></Setup>";
        QVERIFY(Write(root,original)); XcfgDocument document; QString error; QVERIFY(document.Load(root,&error));
        const int selected=document.Lookup("Setup/A[0]/gain",&error); QVERIFY(selected>=0);
        QVERIFY(Write(child,"<Template><gain range=\"[0,3]\">2</gain></Template>"));
        QVERIFY(!document.SetValue(selected,"8",&error)); QVERIFY(error.contains("Source changed"));
        QFile file(root); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original);
    }
    void oversizedProjectionKeepsSourceUntouched() {
        QTemporaryDir directory; const auto path=directory.filePath("large.xcfg");
        // Exercise our aggregate path limit using individually bounded XML
        // names. Patched Qt versions reject a single 64KiB name during parsing.
        const QByteArray name(2048,'n');QByteArray bytes="<Setup>";
        for(int i=0;i<40;++i)bytes+='<'+name+'>';
        bytes+='1';for(int i=0;i<40;++i)bytes+="</"+name+'>';bytes+="</Setup>";
        QVERIFY(Write(path,bytes)); XcfgDocument document; QString error; QVERIFY2(document.Load(path,&error),qPrintable(error));
        QCOMPARE(document.Nodes().size(),41);
        QVERIFY(!document.SetValue(40,"2",&error)); QVERIFY2(error.contains("Shared class path depth/byte limit"),qPrintable(error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes);
    }
    void semanticallyEqualRangeImportsAndExactWideEdit() {
        QTemporaryDir directory; const auto child=directory.filePath("child.xcfg"),root=directory.filePath("root.xcfg");
        QVERIFY(Write(child,"<Template><mask type=\"hex\" range=\"[0x20000000000001, 0xFFFFFFFFFFFFFFFF]\">20000000000001</mask></Template>"));
        QVERIFY(Write(root,"<Setup><Board id=\"0\"><mask type=\"hex\" range=\"[0x20000000000001,0xffffffffffffffff]\">20000000000002</mask></Board>"
            "<Board id=\"1\" include=\"child.xcfg\"/></Setup>"));
        XcfgDocument document; QString error; QVERIFY(document.Load(root,&error));
        XcfgDocument::Node value; XcfgDocument::ScalarProperties properties;
        QVERIFY2(document.ResolveScalar("Setup/Board[1]/mask",&value,&properties,&error),qPrintable(error));
        const int selected=document.Lookup("Setup/Board[1]/mask",&error); QVERIFY(selected>=0);
        QVERIFY(!document.SetValue(selected,"20000000000000",&error));
        QVERIFY2(document.SetValue(selected,"ffffffffffffffff",&error),qPrintable(error));
        ExperimentReader reader(document);
        QCOMPARE(reader.Scalar("Setup/Board[1]/mask",QString(),QString(),true),QString("0xffffffffffffffff"));
    }
    void quotedStringsAndBooleanNormalization() {
        QTemporaryDir directory; const auto path=directory.filePath("ranges.xcfg");
        QVERIFY(Write(path,"<Setup><choice type=\"string\" range=\"{&quot;a,b&quot;,&quot;a\\&quot;b&quot;}\">a,b</choice>"
            "<enabled type=\"bool\" range=\"{TRUE, false}\">TRUE</enabled><number type=\"real\" range=\"[1,10]\">5.0</number></Setup>"));
        XcfgDocument document; QString error; QVERIFY2(document.Load(path,&error),qPrintable(error));
        const int selected=document.Lookup("Setup/choice",&error); QVERIFY(selected>=0);
        QVERIFY2(document.SetValue(selected,"a\"b",&error),qPrintable(error));
        ExperimentReader reader(document); QCOMPARE(reader.Scalar("Setup/choice"),QString("a\"b"));
        QVERIFY(reader.Boolean("Setup/enabled"));
        bool refused=false; try { reader.Scalar("Setup/number",QString(),QString(),false,true); } catch (const std::runtime_error &) { refused=true; }
        QVERIFY(refused);
    }
    void concreteOverrideExpandsLocalParentAndPreservesPeers() {
        QTemporaryDir directory; const auto file=directory.filePath("overrides.xcfg");
        const QByteArray original="<Setup><!-- keep -->\n<Board id=\"*\"><Module><gain range=\"[0,10]\">2</gain><offset>7</offset></Module></Board>\n"
            "<Board id=\"0\" extra=\"preserve\"/><Board id=\"1\"/></Setup>";
        QVERIFY(Write(file,original)); XcfgDocument document; QString error; QVERIFY(document.Load(file,&error));
        XcfgDocument::OverridePreview preview;
        QVERIFY2(document.PreviewOverride("Setup/Board[0]/Module/gain","4",&preview,&error),qPrintable(error));
        QCOMPARE(preview.destinationFile,file); QVERIFY(preview.updatedBytes.contains("extra=\"preserve\""));
        QVERIFY(preview.updatedBytes.contains("<gain range=\"[0,10]\">2</gain>"));
        QFile check(file); QVERIFY(check.open(QIODevice::ReadOnly)); QCOMPARE(check.readAll(),original); check.close();
        auto tampered=preview; tampered.updatedBytes.replace("type=\"int\">4","type=\"int\">9");
        QVERIFY(!document.ApplyOverride(tampered,&error)); QVERIFY(error.contains("preview differs"));
        QVERIFY2(document.ApplyOverride(preview,&error),qPrintable(error));
        XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[0]/Module/gain",&value,&properties,&error));QCOMPARE(value.value,QString("4"));
        QVERIFY(document.ResolveScalar("Setup/Board[1]/Module/gain",&value,&properties,&error));QCOMPARE(value.value,QString("2"));
        QVERIFY(document.ResolveScalar("Setup/Board[0]/Module/offset",&value,&properties,&error));QCOMPARE(value.value,QString("7"));
        QVERIFY(!document.PreviewOverride("Setup/Board[0]/Module/gain","5",&preview,&error)); QVERIFY(error.contains("direct value"));
    }
    void overrideRejectsUnverifiedPathsRangesAndIncludeCollisions() {
        QTemporaryDir directory; const auto file=directory.filePath("root.xcfg"),child=directory.filePath("child.xcfg");
        QVERIFY(Write(child,"<Template><Module><gain range=\"[0,10]\">2</gain></Module></Template>"));
        const QByteArray original="<Setup><Board id=\"0\" include=\"child.xcfg\"/></Setup>";
        QVERIFY(Write(file,original)); XcfgDocument document; QString error; QVERIFY(document.Load(file,&error));
        XcfgDocument::OverridePreview preview;
        QVERIFY(!document.PreviewOverride("Setup/Board[99]/Module/gain","4",&preview,&error));
        QVERIFY(!document.PreviewOverride("Setup/Board[0]/Module/gain","11",&preview,&error));
        QVERIFY(!document.PreviewOverride("Setup/Board[0]/Module/gain","4",&preview,&error)); QVERIFY(error.contains("collision"));
        QFile check(file); QVERIFY(check.open(QIODevice::ReadOnly)); QCOMPARE(check.readAll(),original);
    }
    void overridePreviewCannotIgnoreExternalSourceChanges() {
        QTemporaryDir directory; const auto file=directory.filePath("root.xcfg");
        const QByteArray original="<Setup><Board id=\"*\"><gain>2</gain></Board><Board id=\"0\"/></Setup>";
        QVERIFY(Write(file,original)); XcfgDocument document; QString error; QVERIFY(document.Load(file,&error));
        XcfgDocument::OverridePreview preview;
        QVERIFY(document.PreviewOverride("Setup/Board[0]/gain","3",&preview,&error));
        const auto changed=QByteArray(original).replace("<gain>2</gain>","<gain>5</gain>");
        QVERIFY(Write(file,changed)); QVERIFY(!document.ApplyOverride(preview,&error)); QVERIFY(error.contains("Source changed"));
        QFile check(file); QVERIFY(check.open(QIODevice::ReadOnly)); QCOMPARE(check.readAll(),changed);
    }
    void overridePreservesEmptyStringScalarRole() {
        QTemporaryDir directory; const auto file=directory.filePath("empty.xcfg");
        QVERIFY(Write(file,"<Setup><Board id=\"*\"><label type=\"string\">old</label></Board><Board id=\"0\"/></Setup>"));
        XcfgDocument document; QString error; QVERIFY(document.Load(file,&error)); XcfgDocument::OverridePreview preview;
        QVERIFY2(document.PreviewOverride("Setup/Board[0]/label",QString(),&preview,&error),qPrintable(error));
        QVERIFY(document.ApplyOverride(preview,&error)); ExperimentReader reader(document);
        QCOMPARE(reader.Scalar("Setup/Board[0]/label"),QString());
    }
};
QTEST_GUILESS_MAIN(TestXcfgShared)
#include "test_xcfg_shared.moc"
