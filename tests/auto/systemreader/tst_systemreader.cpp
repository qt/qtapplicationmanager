// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "systemreader.h"

using namespace Qt::StringLiterals;

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
    g_systemRootDir = u":/root"_s;
}

void tst_SystemReader::cgroupProcessInfo()
{
    auto map = fetchCGroupProcessInfo(1234);
    QCOMPARE(map["memory"], QByteArray("/system.slice/run-u5853.scope"));
}

void tst_SystemReader::memoryReaderReadUsedValue()
{
    MemoryReader memoryReader(u"/system.slice/run-u5853.scope"_s);
    quint64 value = memoryReader.readUsedValue();
    QCOMPARE(value, Q_UINT64_C(66809856));
}

void tst_SystemReader::memoryReaderGroupLimit()
{
    MemoryReader memoryReader(u"/system.slice/run-u5853.scope"_s);
    quint64 value = memoryReader.groupLimit();
    QCOMPARE(value, Q_UINT64_C(524288000));
}

QTEST_APPLESS_MAIN(tst_SystemReader)

#include "tst_systemreader.moc"
