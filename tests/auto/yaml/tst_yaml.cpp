// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>
#include <QThreadPool>

#include "qtyaml.h"
#include "configcache.h"
#include "exception.h"
#include "global.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Yaml : public QObject
{
    Q_OBJECT

public:
    tst_Yaml();

private Q_SLOTS:
    void parser();
    void documentParser();
    void cache();
    void mergedCache();
    void parallel();
};


tst_Yaml::tst_Yaml()
{ }

void tst_Yaml::parser()
{
    static const QVariant vnull = QVariant::fromValue(nullptr);

    QVector<QPair<const char *, QVariant>> tests = {
        { "dec", QVariant::fromValue<int>(10) },
        { "hex", QVariant::fromValue<int>(16) },
        { "bin", QVariant::fromValue<int>(2) },
        { "oct", QVariant::fromValue<int>(8) },
        { "float1", QVariant::fromValue<double>(10.1) },
        { "float2", QVariant::fromValue<double>(.1) },
        { "float3", QVariant::fromValue<double>(.1) },
        { "number-separators", QVariant::fromValue<int>(1234567) },
        { "bool-true", true },
        { "bool-yes", true },
        { "bool-false", false },
        { "bool-no", false },
        { "null-literal", vnull },
        { "null-tilde", vnull },
        { "null-empty", vnull },
        { "string-unquoted", QVariant::fromValue<QString>(u"unquoted"_s) },
        { "string-singlequoted", QVariant::fromValue<QString>(u"singlequoted"_s) },
        { "string-doublequoted", QVariant::fromValue<QString>(u"doublequoted"_s) },
        { "list-int", QVariantList { 1, 2, 3 } },
        { "list-mixed", QVariantList { 1, u"two"_s, QVariantList { true, vnull } } },
        { "map1", QVariantMap { { u"a"_s, 1 }, { u"b"_s, u"two"_s },
                                { u"c"_s, QVariantList { 1, 2, 3 } } } }
    };

    try {
        QFile f(u":/data/test.yaml"_s);
        QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
        QByteArray ba = f.readAll();
        QVERIFY(!ba.isEmpty());
        YamlParser yp(ba, f.fileName());
        auto header = yp.parseHeader();

        QCOMPARE(header.first, u"testfile"_s);
        QCOMPARE(header.second, 42);

        QVERIFY(yp.nextDocument());

        YamlParser::Fields fields;
        for (const auto &pair : tests) {
            YamlParser::FieldType type = YamlParser::Scalar;
            if (pair.second.metaType() == QMetaType::fromType<QVariantList>())
                type = YamlParser::List;
            else if (pair.second.metaType() == QMetaType::fromType<QVariantMap>())
                type = YamlParser::Map;
            QVariant value = pair.second;

            fields.emplace_back(pair.first, true, type, [&yp, type, value]() {
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

static const QVariant vnull = QVariant::fromValue(nullptr);

static const QVariantMap testHeaderDoc = {
    { u"formatVersion"_s, 42 }, { u"formatType"_s, u"testfile"_s }
};

static const QVariantMap testMainDoc = {
    { u"dec"_s, 10 },
    { u"hex"_s, 16 },
    { u"bin"_s, 2 },
    { u"oct"_s, 8 },
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
        QVector<QVariant> docs = YamlParser::parseAllDocuments(ba);
        QCOMPARE(docs.size(), 2);
        QCOMPARE(docs.at(0).toMap().size(), 2);

        QCOMPARE(testHeaderDoc, docs.at(0).toMap());
        QCOMPARE(testMainDoc, docs.at(1).toMap());

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

QTEST_GUILESS_MAIN(tst_Yaml)

#include "tst_yaml.moc"
