/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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

typedef QMap<QString, QString> StringMap;
Q_DECLARE_METATYPE(StringMap)

void tst_DebugWrapper::specification_data()
{
    QTest::addColumn<QString>("spec");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<StringMap>("env");
    QTest::addColumn<QStringList>("cmd");

    QMap<QString, QString> noenv;

    QTest::newRow("empty") << "" << false << noenv << QStringList();
    QTest::newRow("empty2") << " " << false << noenv << QStringList();

    QTest::newRow("nocmd") << "foo=bar" << true << StringMap {{ "foo", "bar" }} << QStringList { "%program%", "%arguments%" };
    QTest::newRow("nocmd2") << "foo=bar " << true << StringMap {{ "foo", "bar" }} << QStringList { "%program%", "%arguments%" };

    QTest::newRow("1") << "foo" << true << noenv << QStringList { "foo", "%program%", "%arguments%" };
    QTest::newRow("2") << " foo" << true << noenv << QStringList { "foo", "%program%", "%arguments%" };
    QTest::newRow("3") << "foo " << true << noenv << QStringList { "foo", "%program%", "%arguments%" };
    QTest::newRow("4") << "foo bar" << true << noenv << QStringList { "foo", "bar", "%program%", "%arguments%" };
    QTest::newRow("5") << "foo  bar" << true << noenv << QStringList { "foo", "bar", "%program%", "%arguments%" };
    QTest::newRow("6") << "foo bar baz" << true << noenv << QStringList { "foo", "bar", "baz", "%program%", "%arguments%" };
    QTest::newRow("7") << "fo\\ o b\\nar b\\\\az" << true << noenv << QStringList { "fo o", "b\nar", "b\\az", "%program%", "%arguments%" };
    QTest::newRow("8") << "foo=bar baz" << true << StringMap {{ "foo", "bar" }} << QStringList { "baz", "%program%", "%arguments%" };
    QTest::newRow("9") << "foo=bar a= baz zab" << true << StringMap {{ "foo", "bar" }, { "a", QString() }} << QStringList { "baz", "zab", "%program%", "%arguments%" };
    QTest::newRow("a") << "foo=b\\ a=\\n baz z\\ ab" << true << StringMap {{ "foo", "b a=\n" }} << QStringList { "baz", "z ab", "%program%", "%arguments%" };
    QTest::newRow("b") << "a=b c d=e" << true << StringMap {{ "a", "b" }} << QStringList { "c", "d=e", "%program%", "%arguments%" };

    QTest::newRow("z") << "a=b %program% c %arguments% d" << true << StringMap {{ "a", "b" }} << QStringList { "%program%", "c", "%arguments%", "d" };
    QTest::newRow("y") << "a=b %program% c d" << true << StringMap {{ "a", "b" }} << QStringList { "%program%", "c", "d", "%arguments%" };
    QTest::newRow("x") << "a=b %arguments%" << true << StringMap {{ "a", "b" }} << QStringList { "%arguments%", "%program%" };
    QTest::newRow("w") << "%program% %arguments%" << true << noenv << QStringList { "%program%", "%arguments%" };
    QTest::newRow("w") << "%program% foo-%program% foo-%arguments%-bar %arguments%" << true << noenv << QStringList { "%program%", "foo-%program%", "foo-%arguments%-bar", "%arguments%" };
}

void tst_DebugWrapper::specification()
{
    QFETCH(QString, spec);
    QFETCH(bool, valid);
    QFETCH(StringMap, env);
    QFETCH(QStringList, cmd);

    StringMap resultEnv;
    QStringList resultCmd;
    QCOMPARE(DebugWrapper::parseSpecification(spec, resultCmd, resultEnv), valid);
    QCOMPARE(cmd, resultCmd);
    QCOMPARE(env, resultEnv);
}

void tst_DebugWrapper::substitute_data()
{
    QTest::addColumn<QStringList>("cmd");
    QTest::addColumn<QString>("program");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QStringList>("result");

    QTest::newRow("1") << QStringList { "%program%", "%arguments%" }
                       << QString("prg") << QStringList { "arg1", "arg2" }
                       << QStringList { "prg", "arg1", "arg2" };

    QTest::newRow("2") << QStringList { "%program%" }
                       << QString("prg") << QStringList { "arg1", "arg2" }
                       << QStringList { "prg" };

    QTest::newRow("3") << QStringList { "%program%", "\"x-%program%\"", "%arguments%", "x-%arguments%" }
                       << QString("prg") << QStringList { "arg1", "arg2" }
                       << QStringList { "prg", "\"x-prg\"", "arg1", "arg2", "x-arg1 arg2" };

    QTest::newRow("4") << QStringList { "foo", "%arguments%", "bar", "%program%", "baz", "%arguments%", "foo2" }
                       << QString("prg") << QStringList { "a1", "a2", "a3" }
                       << QStringList { "foo", "a1", "a2", "a3", "bar", "prg", "baz", "a1", "a2", "a3", "foo2" };
}

void tst_DebugWrapper::substitute()
{
    QFETCH(QStringList, cmd);
    QFETCH(QString, program);
    QFETCH(QStringList, arguments);
    QFETCH(QStringList, result);

    QCOMPARE(DebugWrapper::substituteCommand(cmd, program, arguments), result);
}

QTEST_APPLESS_MAIN(tst_DebugWrapper)

#include "tst_debugwrapper.moc"
