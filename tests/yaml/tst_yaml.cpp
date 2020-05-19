/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

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
        { "string-unquoted", QVariant::fromValue<QString>("unquoted") },
        { "string-singlequoted", QVariant::fromValue<QString>("singlequoted") },
        { "string-doublequoted", QVariant::fromValue<QString>("doublequoted") },
        { "list-int", QVariantList { 1, 2, 3 } },
        { "list-mixed", QVariantList { 1, "two", QVariantList { true, vnull } } },
        { "map1", QVariantMap { { "a", 1 }, { "b", "two" }, { "c", QVariantList { 1, 2, 3 } } } }
    };

    try {
        QFile f(":/data/test.yaml");
        QVERIFY2(f.open(QFile::ReadOnly), qPrintable(f.errorString()));
        QByteArray ba = f.readAll();
        QVERIFY(!ba.isEmpty());
        YamlParser p(ba);
        auto header = p.parseHeader();

        QCOMPARE(header.first, "testfile");
        QCOMPARE(header.second, 42);

        QVERIFY(p.nextDocument());

        YamlParser::Fields fields;
        for (const auto &pair : tests) {
            YamlParser::FieldType type = YamlParser::Scalar;
            if (pair.second.type() == QVariant::List)
                type = YamlParser::List;
            else if (pair.second.type() == QVariant::Map)
                type = YamlParser::Map;
            QVariant value = pair.second;

            fields.emplace_back(pair.first, true, type, [type, value](YamlParser *p) {
                switch (type) {
                case YamlParser::Scalar: {
                    QVERIFY(p->isScalar());
                    QVariant v = p->parseScalar();
                    QCOMPARE(int(v.type()), int(value.type()));
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
                      QCOMPARE(v.type(), QVariant::String);
                      QCOMPARE(v.toString(), "ext string");
                  } }
            };
            p->parseFields(extFields);
        });

        fields.emplace_back("stringlist-string", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList { "string" });
        });
        fields.emplace_back("stringlist-list1", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList { "string" });
        });
        fields.emplace_back("stringlist-list2", true, YamlParser::Scalar | YamlParser::List, [](YamlParser *p) {
            QCOMPARE(p->parseStringOrStringList(), QStringList({ "string1", "string2" }));
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
    { "formatVersion", 42 }, { "formatType", "testfile" }
};

static const QVariantMap testMainDoc = {
    { "dec", 10 },
    { "hex", 16 },
    { "bin", 2 },
    { "oct", 8 },
    { "float1", 10.1 },
    { "float2", .1 },
    { "float3", .1 },
    { "number-separators", 1234567 },
    { "bool-true", true },
    { "bool-yes", true },
    { "bool-false", false },
    { "bool-no", false },
    { "null-literal", vnull },
    { "null-tilde", vnull },
    { "string-unquoted", "unquoted" },
    { "string-singlequoted", "singlequoted" },
    { "string-doublequoted", "doublequoted" },
    { "list-int", QVariantList { 1, 2, 3 } },
    { "list-mixed", QVariantList { 1, qSL("two"), QVariantList { true, vnull } } },
    { "map1", QVariantMap { { "a", 1 }, { "b", "two" }, { "c", QVariantList { 1, 2, 3 } } } },


    { "extended", QVariantMap { { "ext-string", "ext string" } } },

    { "stringlist-string", "string" },
    { "stringlist-list1", QVariantList { "string" } },
    { "stringlist-list2", QVariantList { "string1", "string2" } },

    { "list-of-maps", QVariantList { QVariantMap { { "index", 1 }, { "name", "1" } },
                                     QVariantMap { { "index", 2 }, { "name", "2" } } } }
};

void tst_Yaml::documentParser()
{
    try {
        QFile f(":/data/test.yaml");
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
        QScopedPointer<CacheTest> ct(new CacheTest);
        YamlParser p(source->readAll(), fileName);
        p.nextDocument();
        p.parseFields({ { "name", true, YamlParser::Scalar, [&ct](YamlParser *p) {
                          ct->name = p->parseScalar().toString(); } },
                        { "file", true, YamlParser::Scalar, [&ct](YamlParser *p) {
                          ct->file = p->parseScalar().toString(); } }
                      });
        return ct.take();
    }
    CacheTest *loadFromCache(QDataStream &ds)
    {
        CacheTest *ct = new CacheTest;
        ds >> ct->name >> ct->file;
        return ct;
    }
    void saveToCache(QDataStream &ds, const CacheTest *ct)
    {
        ds << ct->name << ct->file;
    }

    void merge(CacheTest *ct1, const CacheTest *ct2)
    {
        ct1->name = ct2->name;
        ct1->file = ct1->file + qSL(",") + ct2->file;
    }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName)
    {
        sourceContent.replace("${FILE}", fileName.toUtf8());
    }
};
QT_END_NAMESPACE_AM

void tst_Yaml::cache()
{
    QStringList files = { ":/data/cache1.yaml", ":/data/cache2.yaml" };

    for (int step = 0; step < 2; ++step) {
        try {
            ConfigCache<CacheTest> cache(files, "cache-test", "CTST", 1,
                                         step == 0 ? AbstractConfigCache::ClearCache
                                                   : AbstractConfigCache::None);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step == 1));
            QVERIFY(cache.parseWroteToCache() == (step == 0));
            CacheTest *ct1 = cache.takeResult(0);
            QVERIFY(ct1);
            QCOMPARE(ct1->name, "cache1");
            QCOMPARE(ct1->file, ":/data/cache1.yaml");
            CacheTest *ct2 = cache.takeResult(1);
            QVERIFY(ct2);
            QCOMPARE(ct2->name, "cache2");
            QCOMPARE(ct2->file, ":/data/cache2.yaml");
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    ConfigCache<CacheTest> wrongVersion(files, "cache-test", "CTST", 2, AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongVersion.parse();
    QVERIFY(!wrongVersion.parseReadFromCache());

    ConfigCache<CacheTest> wrongType(files, "cache-test", "XTST", 1, AbstractConfigCache::None);
    QTest::ignoreMessage(QtWarningMsg, "Failed to read cache: failed to parse cache header");
    wrongType.parse();
    QVERIFY(!wrongType.parseReadFromCache());
}

void tst_Yaml::mergedCache()
{
    QStringList files = { ":/data/cache1.yaml", ":/data/cache2.yaml" };

    for (int step = 0; step < 4; ++step) {
        AbstractConfigCache::Options options = AbstractConfigCache::MergedResult;
        if (step % 2 == 0)
            options |= AbstractConfigCache::ClearCache;
        if (step == 2)
            std::reverse(files.begin(), files.end());

        try {
            ConfigCache<CacheTest> cache(files, "cache-test", "MTST", 1, options);
            cache.parse();
            QVERIFY(cache.parseReadFromCache() == (step % 2 == 1));
            QVERIFY(cache.parseWroteToCache() == (step % 2 == 0));
            CacheTest *ct = cache.takeMergedResult();
            QVERIFY(ct);
            QCOMPARE(ct->name, QFileInfo(files.last()).baseName());
            QCOMPARE(ct->file, files.join(qSL(",")));
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }
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
    QFile f(":/data/test.yaml");
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
