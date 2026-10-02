// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <memory>

#include <QtTest>
#include <QtNetwork>
#include <QTemporaryDir>
#include <qplatformdefs.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#endif
#if defined(Q_OS_LINUX)
#  include <sys/xattr.h>
#endif

#include "global.h"
#include "packagecreator.h"
#include "packageextractor.h"
#include "installationreport.h"
#include "packageutilities.h"
#include "private/packageutilities_p.h"
#include "utilities.h"
#include "unix-utilities.h"

#include "../error-checking.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_PackageExtractor : public QObject
{
    Q_OBJECT

public:
    tst_PackageExtractor();

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void extractAndVerify_data();
    void extractAndVerify();

    void nestedDirectories();
    void invalidEntryPath_data();
    void invalidEntryPath();
    void duplicateEntry();
    void symlinkBypass();
    void extendedAttributes();

    void createAndExtractDigest_data();
    void createAndExtractDigest();
    void legacyDigestCollision();
    void digestV3Injective();
    void digestV3Header();
    void digestKnownAnswers_data();
    void digestKnownAnswers();

    void oversizedHeader();

    void cancelExtraction();

    void extractFromFifo();

private:
    QString m_taest;
    std::unique_ptr<QTemporaryDir> m_extractDir;
};

tst_PackageExtractor::tst_PackageExtractor()
    : m_taest(QString::fromUtf8("t\xc3\xa4st"))
{ }

void tst_PackageExtractor::initTestCase()
{
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");
}

void tst_PackageExtractor::init()
{
    m_extractDir.reset(new QTemporaryDir());
    QVERIFY(m_extractDir->isValid());
}

void tst_PackageExtractor::cleanup()
{
    m_extractDir.reset();
}

void tst_PackageExtractor::extractAndVerify_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("expectedSuccess");
    QTest::addColumn<QString>("errorString");
    QTest::addColumn<QStringList>("entries");
    QTest::addColumn<QMap<QString, QByteArray>>("content");
    QTest::addColumn<QMap<QString, qint64>>("sizes");

    QStringList noEntries;
    QMap<QString, QByteArray> noContent;
    QMap<QString, qint64> noSizes;

    QTest::newRow("normal") << "packages/test.ampkg"
                            << true << ""
                            << QStringList {
                                   u"info.yaml"_s,
                                   u"icon.png"_s,
                                   u"test"_s,
                                   m_taest }
                            << QMap<QString, QByteArray> {
                                   { u"test"_s, "test\n" },
                                   { m_taest, "test with umlaut\n" } }
                            << QMap<QString, qint64> {
                                   // { "info.yaml", 213 }, // this is different on Windows: \n vs. \r\n
                                   { u"icon.png"_s, 1157 },
                                   { u"test"_s, 5 },
                                   { m_taest, 17 } };

    QTest::newRow("invalid-url")    << "packages/no-such-file.ampkg"
                                    << false << "~Error opening .*: (No such file or directory|The system cannot find the file specified\\.)"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-format") << "packages/test-invalid-format.ampkg"
                                    << false << "~.* could not open archive: Unrecognized archive format"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-digest") << "packages/test-invalid-footer-digest.ampkg"
                                    << false << "~package digest mismatch.*"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-path")   << "packages/test-invalid-path.ampkg"
                                    << false << "~invalid archive entry .*: pointing outside of extraction directory"
                                    << noEntries << noContent << noSizes;
}

void tst_PackageExtractor::extractAndVerify()
{
    // macros are stupid...
    typedef QMap<QString, QByteArray> ByteArrayMap;
    typedef QMap<QString, qint64> IntMap;

    QFETCH(QString, path);
    QFETCH(bool, expectedSuccess);
    QFETCH(QString, errorString);
    QFETCH(QStringList, entries);
    QFETCH(ByteArrayMap, content);
    QFETCH(IntMap, sizes);

    PackageExtractor extractor(QUrl::fromLocalFile(QString::fromLatin1(AM_TESTDATA_DIR) + path), m_extractDir->path());
    bool result = extractor.extract();

    if (expectedSuccess) {
        QVERIFY2(result, qPrintable(extractor.errorString()));
    } else {
        QVERIFY(extractor.hasFailed());
        QVERIFY(!extractor.wasCanceled());

        QT_AM_CHECK_ERRORSTRING(extractor.errorString(), errorString);
        return;
    }

    QStringList checkEntries(entries);
    QDirIterator it(m_extractDir->path(), QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString entry = it.next();
#if defined(Q_OS_MACOS)
        // starting with Qt7 file names will be reported as-is in macOS decomposed form
        entry = entry.normalized(QString::NormalizationForm_C);
#endif
        entry = entry.mid(m_extractDir->path().size() + 1);

        QVERIFY2(checkEntries.contains(entry), qPrintable(entry));

        if (content.contains(entry)) {
            QVERIFY(QDir(m_extractDir->path()).exists(entry));
            QFile f(QDir(m_extractDir->path()).absoluteFilePath(entry));
            QVERIFY(f.open(QFile::ReadOnly));
            QCOMPARE(f.readAll(), content.value(entry));
        }

        if (sizes.contains(entry)) {
            QVERIFY(QDir(m_extractDir->path()).exists(entry));
            QFile f(QDir(m_extractDir->path()).absoluteFilePath(entry));
            QCOMPARE(f.size(), sizes.value(entry));
        }

        QVERIFY(checkEntries.removeOne(entry));
    }

    QVERIFY2(checkEntries.isEmpty(), qPrintable(checkEntries.join(u' ')));

    QStringList reportEntries = extractor.installationReport().files();
    reportEntries.sort();
    entries.sort();
    QCOMPARE(reportEntries, entries);
}

static void createFile(const QString &path, const QByteArray &content = "x")
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path + u": "_s + f.errorString()));
    QCOMPARE(f.write(content), qint64(content.size()));
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(path + u": "_s + f.errorString()));
    return f.readAll();
}

// Builds a package containing exactly the given entries. PackageCreator only needs them to exist
// relative to sourceDir, so this can produce entry names that no sane packager would.
static std::unique_ptr<QTemporaryFile> createPackage(const QDir &sourceDir, const QStringList &entries,
                                                     bool extendedAttributes = false)
{
    InstallationReport report(u"com.pelagicore.test"_s);
    report.addFiles(entries);
    report.setDiskSpaceUsed(1);
    report.setIncludeExtendedAttributes(extendedAttributes);

    auto package = std::make_unique<QTemporaryFile>();
    QVERIFY(package->open());
    PackageCreator creator(sourceDir, package.get(), report);
    QVERIFY2(creator.create(), qPrintable(creator.errorString()));
    package->close();
    return package;
}

void tst_PackageExtractor::nestedDirectories()
{
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    QDir src(sourceDir.path());
    QVERIFY(src.mkpath(u"sub/deeper"_s));
    createFile(src.filePath(u"sub/file"_s), "one");
    createFile(src.filePath(u"sub/deeper/file"_s), "two");
    QVERIFY(QFile::setPermissions(src.filePath(u"sub/deeper/file"_s),
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

    const QStringList entries { u"sub"_s, u"sub/file"_s, u"sub/deeper"_s, u"sub/deeper/file"_s };
    auto package = createPackage(src, entries);
    PackageExtractor extractor(QUrl::fromLocalFile(package->fileName()), m_extractDir->path());
    QVERIFY2(extractor.extract(), qPrintable(extractor.errorString()));

    QDir dest(m_extractDir->path());
    QVERIFY(QFileInfo(dest.filePath(u"sub/deeper"_s)).isDir());
    QCOMPARE(readFile(dest.filePath(u"sub/file"_s)), "one"_ba);
    QCOMPARE(readFile(dest.filePath(u"sub/deeper/file"_s)), "two"_ba);
#if defined(Q_OS_UNIX)
    QVERIFY(!QFileInfo(dest.filePath(u"sub/file"_s)).isExecutable());
    QVERIFY(QFileInfo(dest.filePath(u"sub/deeper/file"_s)).isExecutable());
#endif

    QStringList reportEntries = extractor.installationReport().files();
    QStringList expectedEntries = entries;
    reportEntries.sort();
    expectedEntries.sort();
    QCOMPARE(reportEntries, expectedEntries);
}

void tst_PackageExtractor::invalidEntryPath_data()
{
    QTest::addColumn<QStringList>("entries");
    QTest::addColumn<QString>("errorString");

    const QString outside = u"~invalid archive entry .*: pointing outside of extraction directory"_s;
    const QString malformed = u"~invalid archive entry .*: empty or '\\.' path component"_s;

    QTest::newRow("dotdot")          << QStringList { u"../outside"_s } << outside;
    QTest::newRow("dotdot-nested")   << QStringList { u"sub"_s, u"sub/../../outside"_s } << outside;
    QTest::newRow("absolute")        << QStringList { u"/file"_s } << outside;
#if !defined(Q_OS_WIN)
    QTest::newRow("backslash")       << QStringList { u"sub\\file"_s } << outside;
#endif
    QTest::newRow("empty-component") << QStringList { u"sub"_s, u"sub//file"_s } << malformed;
    QTest::newRow("dot-prefix")      << QStringList { u"./file"_s } << malformed;
    QTest::newRow("dot-component")   << QStringList { u"sub"_s, u"sub/./file"_s } << malformed;
}

void tst_PackageExtractor::invalidEntryPath()
{
    QFETCH(QStringList, entries);
    QFETCH(QString, errorString);

    // all the entry names above resolve to existing files relative to src/
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir src(tmp.filePath(u"src"_s));
    QVERIFY(src.mkpath(u"sub"_s));
    createFile(tmp.filePath(u"outside"_s));
    createFile(src.filePath(u"file"_s));
    createFile(src.filePath(u"sub/file"_s));
#if !defined(Q_OS_WIN)
    createFile(src.filePath(u"sub\\file"_s));
#endif

    auto package = createPackage(src, entries);
    PackageExtractor extractor(QUrl::fromLocalFile(package->fileName()), m_extractDir->path());
    QVERIFY(!extractor.extract());
    QVERIFY(!extractor.wasCanceled());
    QT_AM_CHECK_ERRORSTRING(extractor.errorString(), errorString);

    // a leading "sub" directory entry is legitimately created, but no file may have been written
    QStringList files;
    for (QDirIterator it(m_extractDir->path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories); it.hasNext(); )
        files << it.next();
    QVERIFY2(files.isEmpty(), qPrintable(files.join(u' ')));
}

void tst_PackageExtractor::duplicateEntry()
{
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    createFile(sourceDir.filePath(u"file"_s));

    auto package = createPackage(QDir(sourceDir.path()), { u"file"_s, u"file"_s });
    PackageExtractor extractor(QUrl::fromLocalFile(package->fileName()), m_extractDir->path());
    QVERIFY(!extractor.extract());
    QT_AM_CHECK_ERRORSTRING(extractor.errorString(), u"~could not create file .*[Ff]ile exists"_s);
}

// The former canonicalPath() check accepted this entry: its parent directory resolves back into
// the extraction directory through the symlink. Lexically it points outside and must be rejected.
void tst_PackageExtractor::symlinkBypass()
{
#if !defined(Q_OS_UNIX)
    QSKIP("No symlink support on this platform");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir src(tmp.filePath(u"src"_s));
    QVERIFY(src.mkpath(u"."_s));
    QVERIFY(QDir(tmp.path()).mkdir(u"link"_s));
    createFile(tmp.filePath(u"link/evil"_s));

    QDir extractDir(m_extractDir->path());
    QVERIFY(extractDir.mkdir(u"dest"_s));
    QVERIFY(QFile::link(extractDir.filePath(u"dest"_s), extractDir.filePath(u"link"_s)));

    auto package = createPackage(src, { u"../link/evil"_s });
    PackageExtractor extractor(QUrl::fromLocalFile(package->fileName()), extractDir.filePath(u"dest"_s));
    QVERIFY(!extractor.extract());
    QT_AM_CHECK_ERRORSTRING(extractor.errorString(),
                            u"~invalid archive entry .*: pointing outside of extraction directory"_s);
    QVERIFY(QDir(extractDir.filePath(u"dest"_s)).isEmpty());
#endif
}

void tst_PackageExtractor::extendedAttributes()
{
#if !defined(Q_OS_LINUX)
    QSKIP("Extended attributes are only supported on Linux");
#else
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    QDir src(sourceDir.path());
    QVERIFY(src.mkdir(u"sub"_s));
    createFile(src.filePath(u"sub/file"_s), "content");

    // user.* attributes need no privileges, but not every filesystem supports them. Anything else
    // already present (e.g. security.selinux) could not be recreated by the unprivileged extractor.
    for (const QString &path : { src.filePath(u"sub"_s), src.filePath(u"sub/file"_s) }) {
        const QByteArray localPath = QFile::encodeName(path);
        if (::setxattr(localPath.constData(), "user.am-test", "value", 5, 0) != 0)
            QSKIP("The filesystem does not support user extended attributes");
        char names[256];
        const ssize_t size = ::listxattr(localPath.constData(), names, sizeof(names));
        if ((size < 0) || (QByteArray(names, size) != QByteArray("user.am-test\0", 13)))
            QSKIP("The source files have extended attributes beyond the test's control");
    }

    auto package = createPackage(src, { u"sub"_s, u"sub/file"_s }, true /*extendedAttributes*/);
    PackageExtractor extractor(QUrl::fromLocalFile(package->fileName()), m_extractDir->path());
    QVERIFY2(extractor.extract(), qPrintable(extractor.errorString()));

    QDir dest(m_extractDir->path());
    QCOMPARE(readFile(dest.filePath(u"sub/file"_s)), "content"_ba);
    for (const QString &path : { dest.filePath(u"sub"_s), dest.filePath(u"sub/file"_s) }) {
        char value[16];
        const ssize_t size = ::getxattr(QFile::encodeName(path).constData(), "user.am-test", value, sizeof(value));
        QCOMPARE(size, ssize_t(5));
        QCOMPARE(QByteArray(value, 5), "value"_ba);
    }
#endif
}

void tst_PackageExtractor::createAndExtractDigest_data()
{
    QTest::addColumn<int>("formatVersion");

    QTest::newRow("legacy") << 2;
    QTest::newRow("injective") << 3;
}

void tst_PackageExtractor::createAndExtractDigest()
{
    QFETCH(int, formatVersion);

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    QDir src(sourceDir.path());
    QVERIFY(src.mkdir(u"sub"_s));
    createFile(src.filePath(u"sub/file"_s), "content");
    createFile(src.filePath(u"empty"_s), "");

    InstallationReport report(u"com.pelagicore.test"_s);
    report.addFiles({ u"empty"_s, u"sub"_s, u"sub/file"_s });
    report.setDiskSpaceUsed(1);
    report.setPackageFormatVersion(formatVersion);
    report.setExtraSignedMetaData({
        { u"a"_s, 1 },
        { u"b"_s, QVariantList { 1.5, u"x"_s, true, QVariant::fromValue(nullptr) } },
        { u"c"_s, QVariantMap { { u"d"_s, 2.0 }, { u"e"_s, u""_s } } }
    });

    QTemporaryFile package;
    QVERIFY(package.open());
    PackageCreator creator(src, &package, report);
    QVERIFY2(creator.create(), qPrintable(creator.errorString()));
    package.close();
    QVERIFY(!creator.createdDigest().isEmpty());

    PackageExtractor extractor(QUrl::fromLocalFile(package.fileName()), m_extractDir->path());
    QVERIFY2(extractor.extract(), qPrintable(extractor.errorString()));
    QCOMPARE(extractor.installationReport().packageFormatVersion(), formatVersion);
    QCOMPARE(extractor.installationReport().digest(), creator.createdDigest());
    QCOMPARE(extractor.installationReport().extraSignedMetaData().size(), 3);

    // re-creating from the extracted report needs to reproduce the digest and the format version
    QTemporaryFile package2;
    QVERIFY(package2.open());
    PackageCreator creator2(QDir(m_extractDir->path()), &package2, extractor.installationReport());
    QVERIFY2(creator2.create(), qPrintable(creator2.errorString()));
    QCOMPARE(creator2.createdDigest(), creator.createdDigest());
}

// An absent extraSigned adds nothing to a legacy digest and the file data is not framed, so a
// file that looks like a serialized extraSigned can be swapped for a real extraSigned header.
void tst_PackageExtractor::legacyDigestCollision()
{
    // foo's content: QVariant(QVariantMap { "k": <string> }) as written by QDataStream, where the
    // string's last 8 bytes are the "F/<size>/foo" descriptor that follows foo in the digest
    constexpr int fillerUnits = 8;
    QByteArray foo;
    {
        QDataStream ds(&foo, QDataStream::WriteOnly);
        ds.setVersion(QDataStream::Qt_6_7);
        ds << quint32(8) << qint8(0) << quint32(1) << u"k"_s << quint32(10) << qint8(0);
    }
    const qint64 fooSize = foo.size() + 4 + fillerUnits * 2;
    const QByteArray descriptor = "F/" + QByteArray::number(fooSize) + "/foo";
    QCOMPARE(descriptor.size() % 2, 0);
    {
        QDataStream ds(&foo, QDataStream::Append);
        ds << quint32(fillerUnits * 2 + descriptor.size());
    }
    for (int i = 0; i < fillerUnits; ++i) {
        foo.append(char(0x4e));
        foo.append(char(i));
    }
    QCOMPARE(foo.size(), fooSize);
    const QByteArray bar = "the payload";

    QString tail;
    const QByteArray tailBytes = foo.last(fillerUnits * 2) + descriptor;
    for (int i = 0; i < tailBytes.size(); i += 2)
        tail += QChar((uchar(tailBytes.at(i)) << 8) | uchar(tailBytes.at(i + 1)));
    const QVariantMap forgedHeader { { u"extraSigned"_s, QVariantMap { { u"k"_s, tail } } } };

    auto digestOf = [&](int version, const QVariantMap &header, bool withFoo) {
        PackageDigest d(version);
        QVERIFY_THROWS_NO_EXCEPTION({
            d.addHeader(header);
            if (withFoo) {
                d.beginEntry(u"foo"_s, false, foo.size());
                d.addContent(foo);
                d.endEntry(foo.size());
            }
            d.beginEntry(u"bar"_s, false, bar.size());
            d.addContent(bar);
            d.endEntry(bar.size());
        });
        return d.result();
    };

    QCOMPARE(digestOf(2, forgedHeader, false), digestOf(2, { }, true));
    QCOMPARE_NE(digestOf(3, forgedHeader, false), digestOf(3, { }, true));
}

void tst_PackageExtractor::digestV3Injective()
{
    auto digestOf = [](std::function<void(PackageDigest &)> f, int version = 3) {
        PackageDigest d(version);
        QVERIFY_THROWS_NO_EXCEPTION({
            d.addHeader({ { u"packageId"_s, u"com.pelagicore.test"_s } });
            f(d);
        });
        return d.result();
    };
    auto file = [](PackageDigest &d, const QString &path, const QByteArray &content,
                   const QList<std::pair<QByteArray, QByteArray>> &xattrs = { }) {
        d.beginEntry(path, false, content.size());
        for (const auto &[name, value] : xattrs)
            d.addXattr(name, value);
        d.addContent(content);
        d.endEntry(content.size());
    };

    // xattr name/value boundary
    auto xattrSplit = [&](const QByteArray &name, const QByteArray &value) {
        return [=](PackageDigest &d) { file(d, u"f"_s, "x", { { name, value } }); };
    };
    QCOMPARE(digestOf(xattrSplit("user.a/b", "c"), 2), digestOf(xattrSplit("user.a", "b/c"), 2));
    QCOMPARE_NE(digestOf(xattrSplit("user.a/b", "c")), digestOf(xattrSplit("user.a", "b/c")));

    // one xattr whose value looks like another one
    auto twoXattrs = [&](PackageDigest &d) { file(d, u"f"_s, "x", { { "user.a", "x" }, { "user.b", "y" } }); };
    auto oneXattr = [&](PackageDigest &d) { file(d, u"f"_s, "x", { { "user.a", "xXATTR/user.b/y" } }); };
    QCOMPARE(digestOf(twoXattrs, 2), digestOf(oneXattr, 2));
    QCOMPARE_NE(digestOf(twoXattrs), digestOf(oneXattr));

    // file boundary: two files vs. one file that contains the first and the second's data
    auto twoFiles = [&](PackageDigest &d) { file(d, u"a"_s, "1"); file(d, u"b"_s, "2"); };
    auto mergedFile = [&](PackageDigest &d) { file(d, u"b"_s, "1F/1/a2"); };
    QCOMPARE_NE(digestOf(twoFiles), digestOf(mergedFile));

    // entry types, paths and sizes are part of the digest
    auto asFile = [&](PackageDigest &d) { file(d, u"a"_s, ""); };
    auto asDir = [&](PackageDigest &d) { d.beginEntry(u"a"_s, true, 0); d.endEntry(0); };
    QCOMPARE_NE(digestOf(asFile), digestOf(asDir));
    QCOMPARE_NE(digestOf(asFile), digestOf([&](PackageDigest &d) { file(d, u"b"_s, ""); }));

    // an entry whose data doesn't match its declared size is an error
    PackageDigest d(3);
    QVERIFY_THROWS_NO_EXCEPTION(d.beginEntry(u"a"_s, false, 3));
    d.addContent("12");
    QVERIFY_THROWS_EXCEPTION(Exception, d.endEntry(3));
}

void tst_PackageExtractor::digestV3Header()
{
    auto digestOf = [](const QVariantMap &header, int version = 3) {
        PackageDigest d(version);
        QVERIFY_THROWS_NO_EXCEPTION(d.addHeader(header));
        return d.result();
    };

    const QVariantMap base {
        { u"packageId"_s, u"com.pelagicore.test"_s },
        { u"diskSpaceUsed"_s, 1000 },
        { u"extra"_s, QVariantMap { { u"x"_s, 1 } } },
        { u"extraSigned"_s, QVariantMap { { u"y"_s, 2 } } }
    };
    auto with = [&](const QString &key, const QVariant &value) {
        QVariantMap m = base;
        m.insert(key, value);
        return m;
    };

    // signed
    QCOMPARE_NE(digestOf(base), digestOf(with(u"packageId"_s, u"com.pelagicore.other"_s)));
    QCOMPARE_NE(digestOf(base), digestOf(with(u"extendedAttributes"_s, true)));
    QCOMPARE_NE(digestOf(base), digestOf(with(u"extraSigned"_s, QVariantMap { { u"y"_s, 3 } })));
    QCOMPARE_NE(digestOf(base), digestOf(with(u"extraSigned"_s, QVariantMap { { u"y"_s, u"2"_s } })));
    QCOMPARE_NE(digestOf(base), digestOf(with(u"extraSigned"_s, QVariantMap { { u"z"_s, 2 } })));
    QCOMPARE_NE(digestOf(base), digestOf(with(u"extraSigned"_s, QVariantMap { })));

    // not signed: these are hints that a store server may rewrite
    QCOMPARE(digestOf(base), digestOf(with(u"diskSpaceUsed"_s, 5)));
    QCOMPARE(digestOf(base), digestOf(with(u"extra"_s, QVariantMap { { u"x"_s, 2 } })));

    // absent, null and empty extraSigned are the same thing
    QVariantMap noExtraSigned = base;
    noExtraSigned.remove(u"extraSigned"_s);
    QCOMPARE(digestOf(noExtraSigned), digestOf(with(u"extraSigned"_s, QVariantMap { })));
    QCOMPARE(digestOf(noExtraSigned), digestOf(with(u"extraSigned"_s, QVariant::fromValue(nullptr))));
    PackageDigest invalid(3);
    QVERIFY_THROWS_EXCEPTION(Exception, invalid.addHeader(with(u"extraSigned"_s, u"foo"_s)));

    // the algorithm doesn't depend on the order in which the map was filled
    QCOMPARE(digestOf(with(u"extraSigned"_s, QVariantMap { { u"a"_s, 1 }, { u"b"_s, 2 } })),
             digestOf(with(u"extraSigned"_s, QVariantMap { { u"b"_s, 2 }, { u"a"_s, 1 } })));

    // known answer test: this must never change
    QCOMPARE(digestOf(base).toHex(), "92ab91cda23e8ca958e8bcde03d698cb99d286fd28d6e48ee11a090c32a395ed"_ba);
}

// The expected values were calculated by an independent implementation. Neither of them must ever
// change, as that would invalidate the signatures of all existing packages.
void tst_PackageExtractor::digestKnownAnswers_data()
{
    QTest::addColumn<int>("formatVersion");
    QTest::addColumn<QByteArray>("digest");

    QTest::newRow("legacy") << 2 << "f2ef7476a96adf4ff8415aba8ca49167bbe12bf170bde559d13c0ecd1983312d"_ba;
    QTest::newRow("injective") << 3 << "5f8366925f2482475c55aa2800d766703510ec9395aff5cfa9caaae3c665d76c"_ba;
}

void tst_PackageExtractor::digestKnownAnswers()
{
    QFETCH(int, formatVersion);
    QFETCH(QByteArray, digest);

    PackageDigest d(formatVersion);
    QVERIFY_THROWS_NO_EXCEPTION({
        d.addHeader({ { u"packageId"_s, u"com.pelagicore.test"_s },
                      { u"extendedAttributes"_s, true },
                      { u"extraSigned"_s, QVariantMap { { u"y"_s, 2 } } } });

        d.beginEntry(u"sub"_s, true, 0);
        d.endEntry(0);

        d.beginEntry(u"sub/f"_s, false, 5);
        d.addXattr("user.a", "v");
        d.addContent("hello");
        d.endEntry(5);
    });
    QCOMPARE(d.result().toHex(), digest);
}

void tst_PackageExtractor::oversizedHeader()
{
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    QFile content(sourceDir.filePath(u"test"_s));
    QVERIFY(content.open(QIODevice::WriteOnly));
    QCOMPARE(content.write("test\n"), 5);
    content.close();

    InstallationReport report(u"com.pelagicore.test"_s);
    report.addFile(u"test"_s);
    report.setDiskSpaceUsed(5);
    // the header compresses down to almost nothing, but it is buffered uncompressed
    report.setExtraMetaData({ { u"filler"_s, QString(2 * 1024 * 1024, u'x') } });

    QTemporaryFile package;
    QVERIFY(package.open());
    PackageCreator creator(QDir(sourceDir.path()), &package, report);
    QVERIFY2(creator.create(), qPrintable(creator.errorString()));
    package.close();

    PackageExtractor extractor(QUrl::fromLocalFile(package.fileName()), m_extractDir->path());
    QVERIFY(!extractor.extract());
    QVERIFY(extractor.hasFailed());
    QVERIFY(!extractor.wasCanceled());
    QT_AM_CHECK_ERRORSTRING(extractor.errorString(), u"~.*the package's header is too big \\(> 1MB\\)"_s);
}

void tst_PackageExtractor::cancelExtraction()
{
    {
        PackageExtractor extractor(QUrl::fromLocalFile(QString::fromLatin1(AM_TESTDATA_DIR "packages/test.ampkg")), m_extractDir->path());
        extractor.cancel();
        QVERIFY(!extractor.extract());
        QVERIFY(extractor.wasCanceled());
        QCOMPARE(extractor.errorString(), u"canceled"_s);
        QVERIFY(extractor.hasFailed());
    }
    {
        PackageExtractor extractor(QUrl::fromLocalFile(QString::fromLatin1(AM_TESTDATA_DIR "packages/test.ampkg")), m_extractDir->path());
        connect(&extractor, &PackageExtractor::progress, this, [&extractor](qreal p) {
            if (p >= 0.1)
                extractor.cancel();
        });
        QVERIFY(!extractor.extract());
        QVERIFY(extractor.wasCanceled());
        QCOMPARE(extractor.errorString(), u"canceled"_s);
        QVERIFY(extractor.hasFailed());
    }
}

class FifoSource : public QThread // clazy:exclude=missing-qobject-macro
{
public:
    FifoSource(const QString &file)
        : m_file(file)
    {
        m_fifoPath = QDir::temp().absoluteFilePath(u"autotext-package-extractor-%1.fifo"_s)
                .arg(QCoreApplication::applicationPid())
                .toLocal8Bit();
#ifdef Q_OS_UNIX
        QVERIFY2(m_file.open(QFile::ReadOnly), qPrintable(m_file.errorString()));
        QVERIFY2(::mkfifo(m_fifoPath.constData(), 0600) == 0, ::strerror(errno));
#endif
    }

    ~FifoSource() override
    {
#ifdef Q_OS_UNIX
        ::unlink(m_fifoPath.constData());
#endif
    }

    QString path() const
    {
        return QString::fromLocal8Bit(m_fifoPath);
    }

    void run() override
    {
#ifdef Q_OS_UNIX
        Unix::Fd fifoFd { QT_OPEN(m_fifoPath.constData(), O_WRONLY) };
        QVERIFY2(fifoFd, ::strerror(errno));

        QByteArray buffer;
        buffer.resize(1024 * 1024);

        while (!m_file.atEnd()) {
            qint64 bytesRead = m_file.read(buffer.data(), buffer.size());
            QVERIFY(bytesRead >= 0);
            qint64 bytesWritten = QT_WRITE(fifoFd.get(), buffer.constData(), size_t(bytesRead));
            QCOMPARE(bytesRead, bytesWritten);
        }
#endif
    }

private:
    QFile m_file;
    QByteArray m_fifoPath;
};

void tst_PackageExtractor::extractFromFifo()
{
#if !defined(Q_OS_UNIX)
    QSKIP("No FIFO support on this platform");
#endif

    FifoSource fifo(QString::fromLatin1(AM_TESTDATA_DIR "packages/test.ampkg"));
    fifo.start();

    PackageExtractor extractor(QUrl::fromLocalFile(fifo.path()), m_extractDir->path());
    QVERIFY2(extractor.extract(), qPrintable(extractor.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(fifo.isFinished(), 5000 * timeoutFactor());
}

QTEST_GUILESS_MAIN(tst_PackageExtractor)

#include "tst_packageextractor.moc"
