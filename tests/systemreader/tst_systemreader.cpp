/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include "systemreader.h"

QT_USE_NAMESPACE_AM

class tst_SystemReader : public QObject
{
    Q_OBJECT

public:
    tst_SystemReader();

private slots:
    void cgroupProcessInfo();
    void memoryReaderReadUsedValue();
    void memoryReaderGroupLimit();
};

tst_SystemReader::tst_SystemReader()
{
    g_systemRootDir = QFINDTESTDATA("root");
}

void tst_SystemReader::cgroupProcessInfo()
{
    auto map = fetchCGroupProcessInfo(1234);
    QCOMPARE(map["memory"], "/system.slice/run-u5853.scope");
}

void tst_SystemReader::memoryReaderReadUsedValue()
{
    MemoryReader memoryReader(qSL("/system.slice/run-u5853.scope"));
    quint64 value = memoryReader.readUsedValue();
    QCOMPARE(value, UINT64_C(66809856));
}

void tst_SystemReader::memoryReaderGroupLimit()
{
    MemoryReader memoryReader(qSL("/system.slice/run-u5853.scope"));
    quint64 value = memoryReader.groupLimit();
    QCOMPARE(value, UINT64_C(524288000));
}

QTEST_APPLESS_MAIN(tst_SystemReader)

#include "tst_systemreader.moc"
