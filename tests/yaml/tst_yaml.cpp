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
#include "exception.h"
#include "global.h"

QT_USE_NAMESPACE_AM

class tst_Yaml : public QObject
{
    Q_OBJECT

public:
    tst_Yaml();

private slots:
    void documentParser();
    void parallel();
};


tst_Yaml::tst_Yaml()
{ }

static const QVariant vnull = QVariant();

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
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(ba);
        QCOMPARE(docs.size(), 2);
        QCOMPARE(docs.at(0).toMap().size(), 2);

        QCOMPARE(testHeaderDoc, docs.at(0).toMap());
        QCOMPARE(testMainDoc, docs.at(1).toMap());

    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
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
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(m_yaml);
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
