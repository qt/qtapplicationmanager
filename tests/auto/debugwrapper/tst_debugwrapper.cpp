// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include <debugwrapper.h>

#include "../error-checking.h"

QT_USE_NAMESPACE_AM

class tst_DebugWrapper : public QObject
{
    Q_OBJECT

public:
    tst_DebugWrapper(QObject *parent = nullptr);
    ~tst_DebugWrapper();

private slots:
    void specification_data();
    void specification();

    void substitute_data();
    void substitute();
};


tst_DebugWrapper::tst_DebugWrapper(QObject *parent)
    : QObject(parent)
{ }

tst_DebugWrapper::~tst_DebugWrapper()
{ }

typedef QMap<QByteArray, QByteArray> ByteArrayMap;
Q_DECLARE_METATYPE(ByteArrayMap)

void tst_DebugWrapper::specification_data()
{
    QTest::addColumn<QString>("spec");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<ByteArrayMap>("env");
    QTest::addColumn<QByteArrayList>("cmd");

    ByteArrayMap noenv;

    QTest::newRow("empty") << "" << false << noenv << QByteArrayList();
    QTest::newRow("empty2") << " " << false << noenv << QByteArrayList();

    QTest::newRow("nocmd") << "foo=bar" << true << ByteArrayMap {{ "foo", "bar" }} << QByteArrayList { "%program%", "%arguments%" };
    QTest::newRow("nocmd2") << "foo=bar " << true << ByteArrayMap {{ "foo", "bar" }} << QByteArrayList { "%program%", "%arguments%" };

    QTest::newRow("1") << "foo" << true << noenv << QByteArrayList { "foo", "%program%", "%arguments%" };
    QTest::newRow("2") << " foo" << true << noenv << QByteArrayList { "foo", "%program%", "%arguments%" };
    QTest::newRow("3") << "foo " << true << noenv << QByteArrayList { "foo", "%program%", "%arguments%" };
    QTest::newRow("4") << "foo bar" << true << noenv << QByteArrayList { "foo", "bar", "%program%", "%arguments%" };
    QTest::newRow("5") << "foo  bar" << true << noenv << QByteArrayList { "foo", "bar", "%program%", "%arguments%" };
    QTest::newRow("6") << "foo bar baz" << true << noenv << QByteArrayList { "foo", "bar", "baz", "%program%", "%arguments%" };
    QTest::newRow("7") << "fo\\ o b\\nar b\\\\az" << true << noenv << QByteArrayList { "fo o", "b\nar", "b\\az", "%program%", "%arguments%" };
    QTest::newRow("8") << "foo=bar baz" << true << ByteArrayMap {{ "foo", "bar" }} << QByteArrayList { "baz", "%program%", "%arguments%" };
    QTest::newRow("9") << "foo=bar a= baz zab" << true << ByteArrayMap {{ "foo", "bar" }, { "a", "" }} << QByteArrayList { "baz", "zab", "%program%", "%arguments%" };
    QTest::newRow("a") << "foo=b\\ a=\\n baz z\\ ab" << true << ByteArrayMap {{ "foo", "b a=\n" }} << QByteArrayList { "baz", "z ab", "%program%", "%arguments%" };
    QTest::newRow("b") << "a=b c d=e" << true << ByteArrayMap {{ "a", "b" }} << QByteArrayList { "c", "d=e", "%program%", "%arguments%" };

    QTest::newRow("z") << "a=b %program% c %arguments% d" << true << ByteArrayMap {{ "a", "b" }} << QByteArrayList { "%program%", "c", "%arguments%", "d" };
    QTest::newRow("y") << "a=b %program% c d" << true << ByteArrayMap {{ "a", "b" }} << QByteArrayList { "%program%", "c", "d", "%arguments%" };
    QTest::newRow("x") << "a=b %arguments%" << true << ByteArrayMap {{ "a", "b" }} << QByteArrayList { "%arguments%", "%program%" };
    QTest::newRow("w") << "%program% %arguments%" << true << noenv << QByteArrayList { "%program%", "%arguments%" };
    QTest::newRow("w") << "%program% foo-%program% foo-%arguments%-bar %arguments%" << true << noenv << QByteArrayList { "%program%", "foo-%program%", "foo-%arguments%-bar", "%arguments%" };
}

void tst_DebugWrapper::specification()
{
    QFETCH(QString, spec);
    QFETCH(bool, valid);
    QFETCH(ByteArrayMap, env);
    QFETCH(QByteArrayList, cmd);

    QMap<QString, QString> strEnv;
    QMap<QString, QString> resultEnv;
    QStringList resultCmd;
    QStringList strCmd;

    for (auto it = env.cbegin(); it != env.cend(); ++it)
        strEnv.insert(QString::fromLatin1(it.key()), QString::fromLatin1(it.value()));
    for (const auto &c : qAsConst(cmd))
        strCmd << QString::fromLatin1(c);

    QCOMPARE(DebugWrapper::parseSpecification(spec, resultCmd, resultEnv), valid);
    QCOMPARE(strCmd, resultCmd);
    QCOMPARE(strEnv, resultEnv);
}

void tst_DebugWrapper::substitute_data()
{
    QTest::addColumn<QByteArrayList>("cmd");
    QTest::addColumn<QString>("program");
    QTest::addColumn<QByteArrayList>("arguments");
    QTest::addColumn<QByteArrayList>("result");

    QTest::newRow("1") << QByteArrayList { "%program%", "%arguments%" }
                       << "prg" << QByteArrayList { "arg1", "arg2" }
                       << QByteArrayList { "prg", "arg1", "arg2" };

    QTest::newRow("2") << QByteArrayList { "%program%" }
                       << "prg" << QByteArrayList { "arg1", "arg2" }
                       << QByteArrayList { "prg" };

    QTest::newRow("3") << QByteArrayList { "%program%", "\"x-%program%\"", "%arguments%", "x-%arguments%" }
                       << "prg" << QByteArrayList { "arg1", "arg2" }
                       << QByteArrayList { "prg", "\"x-prg\"", "arg1", "arg2", "x-arg1 arg2" };

    QTest::newRow("4") << QByteArrayList { "foo", "%arguments%", "bar", "%program%", "baz", "%arguments%", "foo2" }
                       << "prg" << QByteArrayList { "a1", "a2", "a3" }
                       << QByteArrayList { "foo", "a1", "a2", "a3", "bar", "prg", "baz", "a1", "a2", "a3", "foo2" };
}

void tst_DebugWrapper::substitute()
{
    QFETCH(QByteArrayList, cmd);
    QFETCH(QString, program);
    QFETCH(QByteArrayList, arguments);
    QFETCH(QByteArrayList, result);

    QStringList strCmd, strArguments, strResult;
    for (const auto &c : qAsConst(cmd))
        strCmd << QString::fromLatin1(c);
    for (const auto &a : qAsConst(arguments))
        strArguments << QString::fromLatin1(a);
    for (const auto &r : qAsConst(result))
        strResult << QString::fromLatin1(r);

    QCOMPARE(DebugWrapper::substituteCommand(strCmd, program, strArguments), strResult);
}

QTEST_APPLESS_MAIN(tst_DebugWrapper)

#include "tst_debugwrapper.moc"
