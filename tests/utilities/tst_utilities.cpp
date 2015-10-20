/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore>
#include <QtTest>

#include "utilities.h"


class tst_Utilities : public QObject
{
    Q_OBJECT

public:
    tst_Utilities();

private slots:
    void validDnsName_data();
    void validDnsName();
    void versionComparison_data();
    void versionComparison();
};


tst_Utilities::tst_Utilities()
{ }


void tst_Utilities::validDnsName_data()
{
    QTest::addColumn<QString>("dnsName");
    QTest::addColumn<bool>("valid");

    // passes
    QTest::newRow("normal") << "com.pelagicore.test" << true;
    QTest::newRow("shortest") << "c.p.t" << true;
    QTest::newRow("valid-chars") << "1-2.c-d.3.z" << true;
    QTest::newRow("longest-part") << "com.012345678901234567890123456789012345678901234567890123456789012.test" << true;
    QTest::newRow("longest-name") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.test" << true;
    QTest::newRow("max-part-cnt") << "a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.0.12" << true;

    // failures
    QTest::newRow("too-few-parts") << "com.pelagicore" << false;
    QTest::newRow("empty-part") << "com..test" << false;
    QTest::newRow("empty") << "" << false;
    QTest::newRow("dot-only") << "." << false;
    QTest::newRow("invalid-char1") << "com.pelagi_core.test" << false;
    QTest::newRow("invalid-char2") << "com.pelagi#core.test" << false;
    QTest::newRow("invalid-char3") << "com.pelagi$core.test" << false;
    QTest::newRow("invalid-char3") << "com.pelagi@core.test" << false;
    QTest::newRow("unicode-char") << QString::fromUtf8("c\xc3\xb6m.pelagicore.test") << false;
    QTest::newRow("upper-case") << "com.Pelagicore.test" << false;
    QTest::newRow("dash-at-start") << "com.-pelagicore.test" << false;
    QTest::newRow("dash-at-end") << "com.pelagicore-.test" << false;
    QTest::newRow("part-too-long") << "com.x012345678901234567890123456789012345678901234567890123456789012.test" << false;
    QTest::newRow("name-too-long") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.xtest" << false;
}

void tst_Utilities::validDnsName()
{
    QFETCH(QString, dnsName);
    QFETCH(bool, valid);

    QString errorString;
    bool result = isValidDnsName(dnsName, &errorString);

    QVERIFY2(valid == result, qPrintable(errorString));
}

void tst_Utilities::versionComparison_data()
{
    QTest::addColumn<QString>("version1");
    QTest::addColumn<QString>("version2");
    QTest::addColumn<int>("result");


    QTest::newRow("1") << "" << "" << 0;
    QTest::newRow("2") << "0" << "0" << 0;
    QTest::newRow("3") << "foo" << "foo" << 0;
    QTest::newRow("4") << "1foo" << "1foo" << 0;
    QTest::newRow("5") << "foo1" << "foo1" << 0;
    QTest::newRow("6") << "13.403.51-alpha2+git" << "13.403.51-alpha2+git" << 0;
    QTest::newRow("7") << "1" << "2" << -1;
    QTest::newRow("8") << "2" << "1" << 1;
    QTest::newRow("9") << "1.0" << "2.0" << -1;
    QTest::newRow("10") << "1.99" << "2.0" << -1;
    QTest::newRow("11") << "1.9" << "11" << -1;
    QTest::newRow("12") << "9" << "10" << -1;
    QTest::newRow("12") << "9a" << "10" << -1;
    QTest::newRow("13") << "9-a" << "10" << -1;
    QTest::newRow("14") << "13.403.51-alpha2+gi" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("15") << "13.403.51-alpha1+git" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("16") << "13.403.51-alpha2+git" << "13.403.51-beta1+git" << -1;
    QTest::newRow("17") << "13.403.51-alpha2+git" << "13.403.52" << -1;
    QTest::newRow("18") << "13.403.51-alpha2+git" << "13.403.52-alpha2+git" << -1;
    QTest::newRow("19") << "13.403.51-alpha2+git" << "13.404" << -1;
    QTest::newRow("20") << "13.402" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("21") << "12.403.51-alpha2+git" << "13.403.51-alpha2+git" << -1;
}

void tst_Utilities::versionComparison()
{
    QFETCH(QString, version1);
    QFETCH(QString, version2);
    QFETCH(int, result);

    int cmp = versionCompare(version1, version2);
    QCOMPARE(cmp, result);

    if (result) {
        cmp = versionCompare(version2, version1);
        QCOMPARE(cmp, -result);
    }
}

QTEST_APPLESS_MAIN(tst_Utilities)

#include "tst_utilities.moc"
