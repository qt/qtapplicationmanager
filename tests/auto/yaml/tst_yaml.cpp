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

QT_USE_NAMESPACE_AM

class tst_Yaml : public QObject
{
    Q_OBJECT

public:
    tst_Yaml();

private slots:
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
        { "string-unquoted", QVariant::fromValue<QString>(qSL("unquoted")) },
        { "string-singlequoted", QVariant::fromValue<QString>(qSL("singlequoted")) },
        { "string-doublequoted", QVariant::fromValue<QString>(qSL("doublequoted")) },
        { "list-int", QVariantList { 1, 2, 3 } },
        { "list-mixed", QVariantList { 1, qSL("two"), QVariantList { true, vnull } } },
        { "map1", QVariantMap { { qSL("a"), 1 }, { qSL("b"), qSL("two") },
                                { qSL("c"), QVariantList { 1, 2, 3 } } } }
    };

    try {
        QFile f(qSL(":/data/test.yaml"));
        QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
        QByteArray ba = f.readAll();
        QVERIFY(!ba.isEmpty());
        YamlParser p(ba);
        auto header = p.parseHeader();

        QCOMPARE(header.first, qSL("testfile"));
        QCOMPARE(header.second, 42);

        QVERIFY(p.nextDocument());

        YamlParser::Fields fields;
        for (const auto &pair : tests) {
            YamlParser::FieldType type = YamlParser::Scalar;
            if (pair.second.metaType() == QMetaType::fromType<QVariantList>())
                type = YamlParser::List;
            else if (pair.second.metaType() == QMetaType::fromType<QVariantMap>())
                type = YamlParser::Map;
            QVariant value = pair.second;

            fields.emplace_back(pair.first, true, type, [type, value](YamlParser *p) {
                switch (type) {
                case YamlParser::Scalar: {
                    QVERIFY(p->isScalar());
                    QVariant v = p->parseScalar();
                    QCOMPARE(int(v.metaType().id()), value.metaType().id());
                    QVERIFY(v == value);
                    break;
                }
                case YamlParser::List: {
                    QVERIFY(p->isList());
                    QVariantList vl = p->parseList();
                    QVERIFY(vl == value.toList());
                    break;
                }
                case YamlParser::Map: {
                    QVERIFY(p->isMap());
                    QVariantMap vm = p->parseMap();
                    QVERIFY(vm == value.toMap());
                    break;
                }
                }
            });
        }
        fields.emplace_back("extended", true, YamlParser::Map, [](YamlParser *p) {
            YamlParser::Fields extFields = {
                { "ext-string", true, YamlParser::Scalar, [](YamlParser *p) {
                      QVERIFY(p->isScalar());
                      QVariant v = p->parseScalar();
                      QCOMPARE(v.metaType(), QMetaType::fromType<QString>());
                      QCOMPARE(v.toString(), qSL("ext string"));
                  } }
            };
            p->parseFields(extFields);
        });

        fields.emplace_back("stringlist-string", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList { qSL("string") });
        });
        fields.emplace_back("stringlist-list1", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList { qSL("string") });
        });
        fields.emplace_back("stringlist-list2", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList({ qSL("string1"), qSL("string2") }));
        });

        fields.emplace_back("list-of-maps", true, YamlParser::List, [](YamlParser *p) {
            int index = 0;
            p->parseList([&index](YamlParser *p) {
                ++index;
                YamlParser::Fields lomFields = {
                    { "index", true, YamlParser::Scalar, [&index](YamlParser *p) {
                          QCOMPARE(p->parseScalar().toInt(), index);
                      } },
                    { "name", true, YamlParser::Scalar, [&index](YamlParser *p) {
                          QCOMPARE(p->parseScalar().toString(), QString::number(index));
                      } }
                };
                p->parseFields(lomFields);
            });
            QCOMPARE(index, 2);
        });

        p.parseFields(fields);

        QVERIFY(!p.nextDocument());

    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
}

static const QVariant vnull = QVariant::fromValue(nullptr);

static const QVariantMap testHeaderDoc = {
    { qSL("formatVersion"), 42 }, { qSL("formatType"), qSL("testfile") }
};

static const QVariantMap testMainDoc = {
    { qSL("dec"), 10 },
    { qSL("hex"), 16 },
    { qSL("bin"), 2 },
    { qSL("oct"), 8 },
    { qSL("float1"), 10.1 },
    { qSL("float2"), .1 },
    { qSL("float3"), .1 },
    { qSL("number-separators"), 1234567 },
    { qSL("bool-true"), true },
    { qSL("bool-yes"), true },
    { qSL("bool-false"), false },
    { qSL("bool-no"), false },
    { qSL("null-literal"), vnull },
    { qSL("null-tilde"), vnull },
    { qSL("null-empty"), vnull },
    { qSL("string-unquoted"), qSL("unquoted") },
    { qSL("string-singlequoted"), qSL("singlequoted") },
    { qSL("string-doublequoted"), qSL("doublequoted") },
    { qSL("list-int"), QVariantList { 1, 2, 3 } },
    { qSL("list-mixed"), QVariantList { 1, qSL("two"), QVariantList { true, vnull } } },
    { qSL("map1"), QVariantMap { { qSL("a"), 1 }, { qSL("b"), qSL("two") },
                                 { qSL("c"), QVariantList { 1, 2, 3 } } } },


    { qSL("extended"), QVariantMap { { qSL("ext-string"), qSL("ext string") } } },

    { qSL("stringlist-string"), qSL("string") },
    { qSL("stringlist-list1"), QVariantList { qSL("string") } },
    { qSL("stringlist-list2"), QVariantList { qSL("string1"), qSL("string2") } },

    { qSL("list-of-maps"), QVariantList { QVariantMap { { qSL("index"), 1 }, { qSL("name"), qSL("1") } },
                                          QVariantMap { { qSL("index"), 2 }, { qSL("name"), qSL("2") } } } }
};

void tst_Yaml::documentParser()
{
    try {
        QFile f(qSL(":/data/test.yaml"));
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

// GCC < 7 bug, currently still in RHEL7, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
// this should simply be:
// template<> class QT_PREPEND_NAMESPACE_AM(ConfigCacheAdaptor<CacheTest>)

QT_BEGIN_NAMESPACE_AM
template<> class ConfigCacheAdaptor<CacheTest>
{
public:
    CacheTest *loadFromSource(QIODevice *source, const QString &fileName)
    {
        std::unique_ptr<CacheTest> ct(new CacheTest);
        YamlParser p(source->readAll(), fileName);
        p.nextDocument();
        p.parseFields({ { "name", true, YamlParser::Scalar, [&ct](YamlParser *p) {
                          ct->name = p->parseScalar().toString(); } },
                        { "file", true, YamlParser::Scalar, [&ct](YamlParser *p) {
                          ct->file = p->parseScalar().toString(); } },
                        { "value", false, YamlParser::Scalar, [&ct](YamlParser *p) {
                          ct->value = p->parseScalar().toString(); } }
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
        ct1->file = ct1->file + qSL(",") + ct2->file;
        ct1->value.append(ct2->value);
    }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName)
    {
        sourceContent.replace("${FILE}", fileName.toUtf8());
    }
};
QT_END_NAMESPACE_AM

void tst_Yaml::cache()
{
    QStringList files = { qSL(":/data/cache1.yaml"), qSL(":/data/cache2.yaml") };

    for (int step = 0; step < 2; ++step) {
        try {
            ConfigCache<CacheTest> cache(files, qSL("cache-test"), { 'C','T','S','T' }, 1,
                                         step == 0 ? AbstractConfigCache::ClearCache
                                                   : AbstractConfigCache::None);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step == 1));
            QVERIFY(cache.parseWroteToCache() == (step == 0));
            CacheTest *ct1 = cache.takeResult(0);
            QVERIFY(ct1);
            QCOMPARE(ct1->name, qSL("cache1"));
            QCOMPARE(ct1->file, qSL(":/data/cache1.yaml"));
            CacheTest *ct2 = cache.takeResult(1);
            QVERIFY(ct2);
            QCOMPARE(ct2->name, qSL("cache2"));
            QCOMPARE(ct2->file, qSL(":/data/cache2.yaml"));

            delete ct1;
            delete ct2;
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    ConfigCache<CacheTest> wrongVersion(files, qSL("cache-test"), { 'C','T','S','T' }, 2,
                                        AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongVersion.parse();
    QVERIFY(!wrongVersion.parseReadFromCache());

    ConfigCache<CacheTest> wrongType(files, qSL("cache-test"), { 'X','T','S','T' }, 1,
                                     AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongType.parse();
    QVERIFY(!wrongType.parseReadFromCache());

    ConfigCache<CacheTest> duplicateCache({ qSL(":/cache1.yaml"), qSL(":/cache1.yaml") },
                                          qSL("cache-test"), { 'D','T','S','T' }, 1,
                                          AbstractConfigCache::None);
    try {
        duplicateCache.parse();
        QVERIFY(false);
    }  catch (const Exception &e) {
        QVERIFY(e.errorString().contains(qSL("duplicate")));
    }
}

void tst_Yaml::mergedCache()
{
    // we need cache2 modifieable, so we copy it to a temp file
    QTemporaryFile cache2File(qSL("cache2"));
    QVERIFY(cache2File.open());
    QFile cache2Resource(qSL(":/data/cache2.yaml"));
    QVERIFY(cache2Resource.open(QIODevice::ReadOnly));
    QVERIFY(cache2File.write(cache2Resource.readAll()) > 0);
    QVERIFY(cache2File.flush());

    QStringList files = { qSL(":/data/cache1.yaml"), cache2File.fileName() };

    for (int step = 0; step < 4; ++step) {
        AbstractConfigCache::Options options = AbstractConfigCache::MergedResult;
        if (step % 2 == 0)
            options |= AbstractConfigCache::ClearCache;
        if (step == 2)
            std::reverse(files.begin(), files.end());

        try {
            ConfigCache<CacheTest> cache(files, qSL("cache-test"), { 'M','T','S','T' }, 1, options);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step % 2 == 1));
            QVERIFY(cache.parseWroteToCache() == (step % 2 == 0));
            CacheTest *ct = cache.takeMergedResult();
            QVERIFY(ct);
            QCOMPARE(ct->name, QFileInfo(files.last()).baseName());
            QCOMPARE(ct->file, files.join(qSL(",")));

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

    ConfigCache<CacheTest> brokenCache(files, qSL("cache-test"), { 'M','T','S','T' }, 1,
                                       AbstractConfigCache::MergedResult);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read Cache: cached file checksums do not match");
    brokenCache.parse();
    QVERIFY(brokenCache.parseReadFromCache());
    CacheTest *ct = brokenCache.takeMergedResult();
    QCOMPARE(ct->value, qSL("foobar"));
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
    QFile f(qSL(":/data/test.yaml"));
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

QTEST_MAIN(tst_Yaml)

#include "tst_yaml.moc"
