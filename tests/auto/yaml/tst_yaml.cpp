// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore>
#include <QtTest>
#include <QThreadPool>
#include <QtLogging>
#include <QTemporaryDir>

#include "qtyaml.h"
#include "configcache.h"
#include "exception.h"
#include "global.h"
#include "sudo.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Yaml : public QObject
{
    Q_OBJECT

public:
    tst_Yaml();

private Q_SLOTS:
    void initTestCase();
    void tests_data();
    void tests();
    void parser_data();
    void parser();
    void documentParser();
    void cache();
    void mergedCache();
    void parallel();
    void generate();
    void emitter();

private:
    // version shortcuts for tests()
    static constexpr int DEF = -1;
    static constexpr int ALL = 0;
    static constexpr int V11 = 1;
    static constexpr int V12 = 2;

    QTemporaryDir m_testRoot;
};

static QVariant vnull = QVariant::fromValue(nullptr);

tst_Yaml::tst_Yaml()
{
    auto verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";
    QLoggingCategory::setFilterRules(u"*.debug=%1"_s.arg(verbose ? "true" : "false"));

#if QT_AM_VERSION < QT_VERSION_CHECK(6, 13, 0)
    // remove this line to see all the YAML 1.1 deprecation warnings
    YamlParser::disableDeprecationWarnings();
#endif
}

void tst_Yaml::initTestCase()
{
    // ConfigCache now routes through SudoClient unconditionally. Use the fallback (in-process)
    // implementation and redirect its file storage into a per-run temp dir.
    Sudo::fallbackServer();
    QVERIFY(m_testRoot.isValid());
#if defined(QT_BUILD_INTERNAL)
    SudoClient::instance()->setTestRootPathPrefix(m_testRoot.path() + u'/');
#endif
    SudoClient::instance()->setInstanceId(QString());
}

void tst_Yaml::parser_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("yaml12");
    QTest::newRow("Yaml 1.1") << u":/data/test.yaml"_s << false;
    QTest::newRow("Yaml 1.2") << u":/data/test.yaml"_s << true;
}

void tst_Yaml::parser()
{
    QFETCH(QString, file);
    QFETCH(bool, yaml12);

    struct TestData {
        const char *m_name;
        QVariant m_yaml11;
        QVariant m_yaml12;

        TestData(const char *name, const QVariant &yaml11)
            : m_name(name), m_yaml11(yaml11), m_yaml12(yaml11) { }
        TestData(const char *name, const QVariant &yaml11, const QVariant &yaml12)
            : m_name(name), m_yaml11(yaml11), m_yaml12(yaml12) { }
    };

    std::array<TestData, 22> tests {{
        { "dec", 10 },
        { "hex", 16 },
        { "bin", 2, u"0b10"_s },
        { "oct", 8, 10 },
        { "octNew", u"0o10"_s, 8 },
        { "float1", 10.1 },
        { "float2", .1 },
        { "float3", .1 },
        { "number-separators", 1234567, u"1_234_567"_s },
        { "bool-true", true },
        { "bool-yes", true, u"yes"_s },
        { "bool-false", false },
        { "bool-no", false, u"no"_s },
        { "null-literal", vnull },
        { "null-tilde", vnull },
        { "null-empty", vnull },
        { "string-unquoted", u"unquoted"_s },
        { "string-singlequoted", u"singlequoted"_s },
        { "string-doublequoted", u"doublequoted"_s },
        { "list-int", QVariantList { 1, 2, 3 } },
        { "list-mixed", QVariantList { 1, u"two"_s, QVariantList { true, vnull } },
                        QVariantList { 1, u"two"_s, QVariantList { u"Yes"_s, vnull } } },
        { "map1", QVariantMap { { u"a"_s, 1 }, { u"b"_s, u"two"_s },
                                { u"c"_s, QVariantList { 1, 2, 3 } } } }
    }};

    try {
        QFile f(file);
        QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
        QByteArray ba = f.readAll();
        QVERIFY(!ba.isEmpty());
        ba.prepend(yaml12 ? "%YAML 1.2\n---\n" : "%YAML 1.1\n---\n");
        YamlParser yp(ba, f.fileName());
        auto header = yp.parseHeader();

        QCOMPARE(header.first, u"testfile"_s);
        QCOMPARE(header.second, 42);

        QVERIFY(yp.nextDocument());

        YamlParser::Fields fields;
        for (const auto &[name, value11, value12] : tests) {
            QVariant value = yaml12 ? value12 : value11;
            YamlParser::FieldType type = YamlParser::Scalar;
            if (value.metaType() == QMetaType::fromType<QVariantList>())
                type = YamlParser::List;
            else if (value.metaType() == QMetaType::fromType<QVariantMap>())
                type = YamlParser::Map;

            fields.emplace_back(name, true, type, [&yp, type, value]() {
                switch (type) {
                case YamlParser::Scalar: {
                    QVERIFY(yp.isScalar());
                    QVariant v = yp.parseScalar();
                    QCOMPARE(int(v.metaType().id()), value.metaType().id());
                    QVERIFY(v == value);
                    break;
                }
                case YamlParser::List: {
                    QVERIFY(yp.isList());
                    QVariantList vl = yp.parseList();
                    QVERIFY(vl == value.toList());
                    break;
                }
                case YamlParser::Map: {
                    QVERIFY(yp.isMap());
                    QVariantMap vm = yp.parseMap();
                    QVERIFY(vm == value.toMap());
                    break;
                }
                }
            });
        }
        fields.emplace_back("extended", true, YamlParser::Map, [&yp]() {
            const YamlParser::Fields extFields = {
                { "ext-string", true, YamlParser::Scalar, [&]() {
                      QVERIFY(yp.isScalar());
                      QVariant v = yp.parseScalar();
                      QCOMPARE(v.metaType(), QMetaType::fromType<QString>());
                      QCOMPARE(v.toString(), u"ext string"_s);
                  } }
            };
            yp.parseFields(extFields);
        });

        fields.emplace_back("stringlist-string", true, YamlParser::Scalar | YamlParser::List, [&]() {
            QCOMPARE(yp.parseStringOrStringList(), QStringList { u"string"_s });
        });
        fields.emplace_back("stringlist-list1", true, YamlParser::Scalar | YamlParser::List, [&]() {
            QCOMPARE(yp.parseStringOrStringList(), QStringList { u"string"_s });
        });
        fields.emplace_back("stringlist-list2", true, YamlParser::Scalar | YamlParser::List, [&]() {
            QCOMPARE(yp.parseStringOrStringList(), QStringList({ u"string1"_s, u"string2"_s }));
        });

        fields.emplace_back("list-of-maps", true, YamlParser::List, [&yp]() {
            int index = 0;
            yp.parseList([&index, &yp]() {
                ++index;
                const YamlParser::Fields lomFields = {
                    { "index", true, YamlParser::Scalar, [&index, &yp]() {
                         QCOMPARE(yp.parseScalar().toInt(), index);
                     } },
                    { "name", true, YamlParser::Scalar, [&index, &yp]() {
                         QCOMPARE(yp.parseScalar().toString(), QString::number(index));
                     } }
                };
                yp.parseFields(lomFields);
            });
            QCOMPARE(index, 2);
        });

        fields.emplace_back("durations", true, YamlParser::Map, [&]() {
            const YamlParser::Fields durationsFields = {
                { "h", true, YamlParser::Scalar, [&]() {
                     std::chrono::seconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsSec());
                     QCOMPARE(d, std::chrono::minutes(-90));
                 } },
                { "min", true, YamlParser::Scalar, [&]() {
                     std::chrono::seconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsSec());
                     QCOMPARE(d, std::chrono::seconds(90));
                 } },
                { "s", true, YamlParser::Scalar, [&]() {
                     std::chrono::milliseconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsMSec());
                     QCOMPARE(d, std::chrono::milliseconds(1500));
                 } },
                { "ms", true, YamlParser::Scalar, [&]() {
                     std::chrono::microseconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsUSec());
                     QCOMPARE(d, std::chrono::microseconds(1500));
                 } },
                { "us", true, YamlParser::Scalar, [&]() {
                     std::chrono::microseconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsUSec());
                     QCOMPARE(d, std::chrono::microseconds(1));
                 } },
                { "default", true, YamlParser::Scalar, [&]() {
                     std::chrono::milliseconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsMSec(u"s"));
                     QCOMPARE(d, std::chrono::seconds(1500));
                 } },
                { "offas0", true, YamlParser::Scalar, [&]() {
                     std::chrono::milliseconds d;
                     QVERIFY_THROWS_NO_EXCEPTION(d = yp.parseDurationAsMSec());
                     QCOMPARE(d, std::chrono::milliseconds(0));
                 } },
                { "invalid", true, YamlParser::Scalar, [&]() {
                     std::chrono::seconds d;
                     QVERIFY_THROWS_EXCEPTION(YamlParserException, d = yp.parseDurationAsSec());
                 } },
            };
            yp.parseFields(durationsFields);
        });

        yp.parseFields(fields);

        QVERIFY(!yp.nextDocument());

    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
}


static const QVariantMap testHeaderDoc = {
    { u"formatVersion"_s, 42 }, { u"formatType"_s, u"testfile"_s }
};

static const QVariantMap testMainDoc = {
    { u"dec"_s, 10 },
    { u"hex"_s, 16 },
    { u"bin"_s, 2 },
    { u"oct"_s, 8 },
    { u"octNew"_s, u"0o10"_s },
    { u"float1"_s, 10.1 },
    { u"float2"_s, .1 },
    { u"float3"_s, .1 },
    { u"number-separators"_s, 1234567 },
    { u"bool-true"_s, true },
    { u"bool-yes"_s, true },
    { u"bool-false"_s, false },
    { u"bool-no"_s, false },
    { u"null-literal"_s, vnull },
    { u"null-tilde"_s, vnull },
    { u"null-empty"_s, vnull },
    { u"string-unquoted"_s, u"unquoted"_s },
    { u"string-singlequoted"_s, u"singlequoted"_s },
    { u"string-doublequoted"_s, u"doublequoted"_s },
    { u"list-int"_s, QVariantList { 1, 2, 3 } },
    { u"list-mixed"_s, QVariantList { 1, u"two"_s, QVariantList { true, vnull } } },
    { u"map1"_s, QVariantMap { { u"a"_s, 1 }, { u"b"_s, u"two"_s },
                                 { u"c"_s, QVariantList { 1, 2, 3 } } } },


    { u"extended"_s, QVariantMap { { u"ext-string"_s, u"ext string"_s } } },

    { u"stringlist-string"_s, u"string"_s },
    { u"stringlist-list1"_s, QVariantList { u"string"_s } },
    { u"stringlist-list2"_s, QVariantList { u"string1"_s, u"string2"_s } },

    { u"list-of-maps"_s, QVariantList { QVariantMap { { u"index"_s, 1 }, { u"name"_s, u"1"_s } },
                                          QVariantMap { { u"index"_s, 2 }, { u"name"_s, u"2"_s } } } },
    { u"durations"_s, QVariantMap {
                                 { u"h"_s, u"-1.5h"_s }, { u"min"_s, u" 1.5 min "_s },
                                 { u"s"_s, u"1.5  s"_s }, { u"ms"_s, u"1.5 ms"_s },
                                 { u"us"_s, u"1.5us"_s }, { u"default"_s, 1500 },
                                 { u"offas0"_s, false }, { u"invalid"_s, u"1.5x"_s } } },
};

void tst_Yaml::documentParser()
{
    try {
        QFile f(u":/data/test.yaml"_s);
        QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
        QByteArray ba = f.readAll();
        QVERIFY(!ba.isEmpty());
        ba.prepend("%YAML 1.1\n---\n");
        QVector<QVariant> docs = YamlParser::parseAllDocuments(ba);
        QCOMPARE(docs.size(), 2);
        QCOMPARE(docs.at(0).toMap().size(), 2);

        QCOMPARE(docs.at(0).toMap(), testHeaderDoc);
        QCOMPARE(docs.at(1).toMap(), testMainDoc);

    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
}
struct CacheTest
{
    QString name;
    QString file;
    QString value;
};

template<> class QtAM::ConfigCacheAdaptor<CacheTest>
{
public:
    CacheTest *loadFromSource(QIODevice *source, const QString &fileName)
    {
        std::unique_ptr<CacheTest> ct(new CacheTest);
        YamlParser yp(source->readAll(), fileName);
        yp.nextDocument();
        yp.parseFields({
            { "name", true, YamlParser::Scalar, [&]() { ct->name = yp.parseString(); } },
            { "file", true, YamlParser::Scalar, [&]() { ct->file = yp.parseString(); } },
            { "value", false, YamlParser::Scalar, [&]() { ct->value = yp.parseString(); } }
        });
        return ct.release();
    }
    CacheTest *loadFromCache(QDataStream &ds)
    {
        CacheTest *ct = new CacheTest;
        ds >> ct->name >> ct->file >> ct->value;
        return ct;
    }
    void saveToCache(QDataStream &ds, const CacheTest *ct)
    {
        ds << ct->name << ct->file << ct->value;
    }

    void merge(CacheTest *ct1, const CacheTest *ct2)
    {
        ct1->name = ct2->name;
        ct1->file = ct1->file + u","_s + ct2->file;
        ct1->value.append(ct2->value);
    }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName)
    {
        sourceContent.replace("${FILE}", fileName.toUtf8());
    }
};

void tst_Yaml::cache()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#endif
    QStringList files = { u":/data/cache1.yaml"_s, u":/data/cache2.yaml"_s };

    for (int step = 0; step < 2; ++step) {
        try {
            ConfigCache<CacheTest> cache(files, u"cache-test"_s, { 'C','T','S','T' }, 1,
                                         step == 0 ? AbstractConfigCache::ClearCache
                                                   : AbstractConfigCache::None);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step == 1));
            QVERIFY(cache.parseWroteToCache() == (step == 0));
            CacheTest *ct1 = cache.takeResult(0);
            QVERIFY(ct1);
            QCOMPARE(ct1->name, u"cache1"_s);
            QCOMPARE(ct1->file, u":/data/cache1.yaml"_s);
            CacheTest *ct2 = cache.takeResult(1);
            QVERIFY(ct2);
            QCOMPARE(ct2->name, u"cache2"_s);
            QCOMPARE(ct2->file, u":/data/cache2.yaml"_s);

            delete ct1;
            delete ct2;
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    ConfigCache<CacheTest> wrongVersion(files, u"cache-test"_s, { 'C','T','S','T' }, 2,
                                        AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongVersion.parse();
    QVERIFY(!wrongVersion.parseReadFromCache());

    ConfigCache<CacheTest> wrongType(files, u"cache-test"_s, { 'X','T','S','T' }, 1,
                                     AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongType.parse();
    QVERIFY(!wrongType.parseReadFromCache());

    ConfigCache<CacheTest> duplicateCache({ u":/cache1.yaml"_s, u":/cache1.yaml"_s },
                                          u"cache-test"_s, { 'D','T','S','T' }, 1,
                                          AbstractConfigCache::None);
    try {
        duplicateCache.parse();
        QVERIFY(false);
    }  catch (const Exception &e) {
        QVERIFY(e.errorString().contains(u"duplicate"_s));
    }
}

void tst_Yaml::mergedCache()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#endif
    // we need cache2 modifieable, so we copy it to a temp file
    QTemporaryFile cache2File(u"cache2"_s);
    QVERIFY(cache2File.open());
    QFile cache2Resource(u":/data/cache2.yaml"_s);
    QVERIFY(cache2Resource.open(QIODevice::ReadOnly));
    QVERIFY(cache2File.write(cache2Resource.readAll()) > 0);
    QVERIFY(cache2File.flush());

    const QString cache2FileName = QFileInfo(cache2File).absoluteFilePath();
    QStringList files = { u":/data/cache1.yaml"_s, cache2FileName };

    for (int step = 0; step < 4; ++step) {
        AbstractConfigCache::Options options = AbstractConfigCache::MergedResult;
        if (step % 2 == 0)
            options |= AbstractConfigCache::ClearCache;
        if (step == 2)
            std::reverse(files.begin(), files.end());

        try {
            ConfigCache<CacheTest> cache(files, u"cache-test"_s, { 'M','T','S','T' }, 1, options);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step % 2 == 1));
            QVERIFY(cache.parseWroteToCache() == (step % 2 == 0));
            CacheTest *ct = cache.takeMergedResult();
            QVERIFY(ct);
            QCOMPARE(ct->name, QFileInfo(files.last()).baseName());
            QCOMPARE(ct->file, files.join(u","_s));

            delete ct;
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    // modify one of the YAML files to see if the merged result gets invalidated

    QVERIFY(cache2File.seek(0));
    QByteArray ba = cache2File.readAll();
    QVERIFY(ba.size() > 0);
    QByteArray ba2 = ba;
    ba2.replace("FOOBAR", "foobar");
    QVERIFY(ba != ba2);
    QVERIFY(cache2File.seek(0));
    QCOMPARE(cache2File.write(ba2), ba2.size());
    QVERIFY(cache2File.flush());

    ConfigCache<CacheTest> brokenCache(files, u"cache-test"_s, { 'M','T','S','T' }, 1,
                                       AbstractConfigCache::MergedResult);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read Cache: cached file checksums do not match");
    brokenCache.parse();
    QVERIFY(brokenCache.parseReadFromCache());
    CacheTest *ct = brokenCache.takeMergedResult();
    QCOMPARE(ct->value, u"foobar"_s);
    delete ct;
}

class YamlRunnable : public QRunnable
{
public:
    YamlRunnable(const QByteArray &yaml, QAtomicInt &success, QAtomicInt &fail)
        : m_yaml(yaml)
        , m_success(success)
        , m_fail(fail)
    { }

    void run() override
    {
        QVector<QVariant> docs;
        try {
            docs = YamlParser::parseAllDocuments(m_yaml);
        } catch (...) {
            docs.clear();
        }
        if ((docs.size() == 2)
            && (docs.at(0).toMap().size() == 2)
            && (testHeaderDoc == docs.at(0).toMap())
            && (testMainDoc == docs.at(1).toMap())) {
            m_success.fetchAndAddOrdered(1);
        } else {
            m_fail.fetchAndAddOrdered(1);
        }
    }
private:
    const QByteArray m_yaml;
    QAtomicInt &m_success;
    QAtomicInt &m_fail;
};

void tst_Yaml::parallel()
{
    QFile f(u":/data/test.yaml"_s);
    QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
    QByteArray ba = f.readAll();
    QVERIFY(!ba.isEmpty());
    ba.prepend("%YAML 1.1\n---\n");

    constexpr int threadCount = 16;

    QAtomicInt success;
    QAtomicInt fail;

    QThreadPool tp;
    if (tp.maxThreadCount() < threadCount)
        tp.setMaxThreadCount(threadCount);

    for (int i = 0; i < threadCount; ++i)
        tp.start(new YamlRunnable(ba, success, fail));

    QVERIFY(tp.waitForDone(5000));
    QCOMPARE(fail.loadAcquire(), 0);
    QCOMPARE(success.loadAcquire(), threadCount);
}

void tst_Yaml::tests_data()
{
    QTest::addColumn<QString>("yaml");
    QTest::addColumn<int>("version");  // 0 = default, 1 = 1.1, 2 = 1.2
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QVariant>("result"); // result or exception text when valid == false

    auto V = [](QVariant v) { return v; };

    QTest::newRow("no-1.1") << "no: yes" << V11 << false << V(u"Only strings are supported as mapping keys"_s);
    QTest::newRow("no-1.2") << "no: yes" << V12 << true  << V(QVariantMap{{u"no"_s, u"yes"_s}});
    QTest::newRow("no-def") << "no: yes" << DEF << false << V(u"Only strings are supported as mapping keys"_s);

    QTest::newRow("bool-key")  << "true: a" << ALL << false << V(u"Only strings are supported as mapping keys"_s);
    QTest::newRow("null-key")  << "~: a"    << ALL << false << V(u"Only strings are supported as mapping keys"_s);
    QTest::newRow("+int-key")  << "1: a"    << ALL << false << V(u"Only strings are supported as mapping keys"_s);
    QTest::newRow("-int-key")  << "-1: a"   << ALL << false << V(u"Only strings are supported as mapping keys"_s);
    QTest::newRow("float-key") << ".1: a"   << ALL << false << V(u"Only strings are supported as mapping keys"_s);

    QTest::newRow("quote-1") << R"('a''a')" << ALL << true << V(uR"(a'a)"_s);
    QTest::newRow("quote-2") << R"('a"a')"  << ALL << true << V(uR"(a"a)"_s);
    QTest::newRow("quote-3") << R"("a'a")"  << ALL << true << V(uR"(a'a)"_s);
    QTest::newRow("quote-4") << R"("a\"a")" << ALL << true << V(uR"(a"a)"_s);

    QTest::newRow("literal-str") << ">\n  a\n" << ALL << true << V(u"a\n"_s);
    QTest::newRow("literal-int") << ">\n 42\n" << ALL << true << V(42);
    QTest::newRow("folded-str")  << "|\n a\n b \"c\" 'd'\n e\n \n f\n   g\n \n " << ALL << true << V(u"a\nb \"c\" 'd'\ne\n\nf\n  g\n"_s);

    QTest::newRow("tag-str-str") << "!!str str" << ALL << true << V(u"str"_s);
    QTest::newRow("tag-str-int") << "!!str 42" << ALL << true << V(u"42"_s);
    QTest::newRow("tag-str-bool") << "!!str true" << ALL << true << V(u"true"_s);
    QTest::newRow("tag-str-null") << "!!str null" << ALL << true << V(u"null"_s);
    QTest::newRow("tag-str-float") << "!!str .1" << ALL << true << V(u".1"_s);
    QTest::newRow("tag-str-empty") << "!!str" << ALL << true << V(u""_s);

    QTest::newRow("tag-int-str") << "!!int 'str'" << ALL << false << V(u"expected an integer value"_s);
    QTest::newRow("tag-int-int") << "!!int 42" << ALL << true << V(42);
    QTest::newRow("tag-int-intstr") << "!!int '42'" << ALL << true << V(42);
    QTest::newRow("tag-int-bool") << "!!int false" << ALL << false << V(u"expected an integer value"_s);
    QTest::newRow("tag-int-null") << "!!int ~" << ALL << false << V(u"expected an integer value"_s);
    QTest::newRow("tag-int-float") << "!!int .1" << ALL << false << V(u"expected an integer value"_s);
    QTest::newRow("tag-int-empty") << "!!int" << ALL << false << V(u"expected an integer value"_s);

    QTest::newRow("tag-bool-int") << "!!bool 42" << ALL << false << V(u"expected a boolean value"_s);
    QTest::newRow("tag-bool-boolstr") << "!!bool 'true'" << ALL << true << V(true);
    QTest::newRow("tag-bool-str") << "!!bool 'str'" << ALL << false << V(u"expected a boolean value"_s);
    QTest::newRow("tag-bool-bool") << "!!bool true" << ALL << true << V(true);
    QTest::newRow("tag-bool-null") << "!!bool ~" << ALL << false << V(u"expected a boolean value"_s);
    QTest::newRow("tag-bool-float") << "!!bool .1" << ALL << false << V(u"expected a boolean value"_s);
    QTest::newRow("tag-bool-empty") << "!!bool" << ALL << false << V(u"expected a boolean value"_s);

    QTest::newRow("tag-null-int") << "!!null 42" << ALL << false << V(u"expected a null value"_s);
    QTest::newRow("tag-null-nullstr") << "!!null 'null'" << ALL << true << V(vnull);
    QTest::newRow("tag-null-str") << "!!null 'str'" << ALL << false << V(u"expected a null value"_s);
    QTest::newRow("tag-null-bool") << "!!null true" << ALL << false << V(u"expected a null value"_s);
    QTest::newRow("tag-null-null") << "!!null ~" << ALL << true << V(vnull);
    QTest::newRow("tag-null-float") << "!!null .1" << ALL << false << V(u"expected a null value"_s);
    QTest::newRow("tag-null-empty") << "!!null" << ALL << true << V(vnull);

    QTest::newRow("tag-float-int") << "!!float 42" << ALL << true << V(42.);
    QTest::newRow("tag-float-floatstr") << "!!float '3.14'" << ALL << true << V(3.14);
    QTest::newRow("tag-float-str") << "!!float 'str'" << ALL << false << V(u"expected a float value"_s);
    QTest::newRow("tag-float-bool") << "!!float true" << ALL << false << V(u"expected a float value"_s);
    QTest::newRow("tag-float-null") << "!!float ~" << ALL << false << V(u"expected a float value"_s);
    QTest::newRow("tag-float-float") << "!!float .1" << ALL << true << V(.1);
    QTest::newRow("tag-float-empty") << "!!float" << ALL << false << V(u"expected a float value"_s);

    QTest::newRow("tag-xxx") << "!!xxx str" << ALL << false << V(u"unsupported tag: tag:yaml.org,2002:xxx"_s);
}

void tst_Yaml::tests()
{
    QFETCH(QString, yaml);
    QFETCH(int, version);
    QFETCH(bool, valid);
    QFETCH(QVariant, result);

    QVERIFY((version >= DEF) && (version <= V12));

    if (version == DEF)
        version = V11;

    int beg = (version == ALL) ? V11 : version;
    int end = (version == ALL) ? V12 : version;

    for (int v = beg; v <= end; ++v) {
        QByteArray yamlStr = yaml.toUtf8();

         // we need a map to make it a document
        if (yamlStr.contains(':'))
            yamlStr.prepend("k:\n ");
        else
            yamlStr.prepend("k: ");

        switch (v) {
        case 0: break;
        case 1: yamlStr.prepend("%YAML 1.1\n---\n"); break;
        case 2: yamlStr.prepend("%YAML 1.2\n---\n"); break;
        }

        if (valid) {
            QVector<QVariant> docs;
            QVERIFY_THROWS_NO_EXCEPTION(docs = YamlParser::parseAllDocuments(yamlStr));
            QVERIFY(docs.size() == 1);
            QVERIFY(docs.at(0).userType() == QMetaType::QVariantMap);
            QVariant value = docs.at(0).toMap().value(u"k"_s);
            QCOMPARE(value, result);
        } else {
            // QVERIFY_THROWS_EXCEPTION does not give access to the what() the text
            try {
                auto docs = YamlParser::parseAllDocuments(yamlStr);
                QString msg = u"Expected to throw an exception, but returned result: "_s
                              + docs.at(0).toMap().value(u"k"_s).toString();
                QFAIL(qPrintable(msg));
            } catch (const Exception &e) {
                QVERIFY2(e.errorString().contains(result.toString()), e.what());
            } catch (...) {
                QFAIL("Expected exception was thrown, but of incorrect type");
            }
        }
    }
}

void tst_Yaml::generate()
{
    QFile f(u":/data/tricky.yaml"_s);
    QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
    QByteArray ba = f.readAll().replace('\r', "");
    QVERIFY(!ba.isEmpty());

    QVector<QVariant> docs;
    QVERIFY_THROWS_NO_EXCEPTION(docs = YamlParser::parseAllDocuments(ba));
    QCOMPARE(docs.size(), 1);

    QByteArray baGen = YamlEmitter::fromVariantDocuments(docs, YamlVersion::V1_1);
    QCOMPARE(ba, baGen);
}

void tst_Yaml::emitter()
{
    QVariantMap m {
        { u"no"_s, false },
        { u"on"_s, true },
        { u"key"_s, u"value"_s }
    };

    auto yamlDef = YamlEmitter::fromVariantDocuments({m});
    auto yamlV11 = YamlEmitter::fromVariantDocuments({m}, YamlVersion::V1_1);
    auto yamlV12 = YamlEmitter::fromVariantDocuments({m}, YamlVersion::V1_2);

#if QT_AM_VERSION < QT_VERSION_CHECK(6, 13, 0)
    QCOMPARE(yamlV11, yamlDef);
#else
    QCOMPARE(yamlV12, yamlDef);
#endif

    const QByteArray expectedV11 =
        "%YAML 1.1\n"
        "---\n"
        "key: 'value'\n"
        "'no': false\n"
        "'on': true\n";

    const QByteArray expectedV12 =
        "%YAML 1.2\n"
        "---\n"
        "key: 'value'\n"
        "no: false\n"
        "on: true\n";

    QCOMPARE(yamlV11, expectedV11);
    QCOMPARE(yamlV12, expectedV12);
}

QTEST_GUILESS_MAIN(tst_Yaml)

#include "tst_yaml.moc"
