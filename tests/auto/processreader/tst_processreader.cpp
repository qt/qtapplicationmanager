// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>
#include <QtAppManMonitor/processreader.h>

QT_USE_NAMESPACE_AM

class tst_ProcessReader : public QObject
{
    Q_OBJECT

public:
    tst_ProcessReader();

private slots:
    void memInvalid_data();
    void memInvalid();
    void memTestProcess();
    void memBasic();
    void memAdvanced();

private:
    void printMem();
    ProcessReader reader;
};

tst_ProcessReader::tst_ProcessReader()
{}

void tst_ProcessReader::memInvalid_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("arbitrary") << QFINDTESTDATA("tst_processreader.cpp");
    QTest::newRow("binary") << QFINDTESTDATA("tst_processreader");
    QTest::newRow("missingvalue") << QFINDTESTDATA("invalid.smaps");
}

void tst_ProcessReader::memInvalid()
{
    QFETCH(QString, file);

    reader.testReadSmaps(file.toLocal8Bit());

    QCOMPARE(reader.memory.totalVm, 0u);
    QCOMPARE(reader.memory.totalRss, 0u);
    QCOMPARE(reader.memory.totalPss, 0u);
    QCOMPARE(reader.memory.textVm, 0u);
    QCOMPARE(reader.memory.textRss, 0u);
    QCOMPARE(reader.memory.textPss, 0u);
    QCOMPARE(reader.memory.heapVm, 0u);
    QCOMPARE(reader.memory.heapRss, 0u);
    QCOMPARE(reader.memory.heapPss, 0u);
}

void tst_ProcessReader::memTestProcess()
{
    const QByteArray file = "/proc/" + QByteArray::number(QCoreApplication::applicationPid()) + "/smaps";

    QVERIFY(reader.testReadSmaps(file));
    //printMem();
    QVERIFY(reader.memory.totalVm >= reader.memory.totalRss);
    QVERIFY(reader.memory.totalRss >= reader.memory.totalPss);
    QVERIFY(reader.memory.textVm >= reader.memory.textRss);
    QVERIFY(reader.memory.textRss >= reader.memory.textPss);
    QVERIFY(reader.memory.heapVm >= reader.memory.heapRss);
    QVERIFY(reader.memory.heapRss >= reader.memory.heapPss);
}

void tst_ProcessReader::memBasic()
{
    QVERIFY(reader.testReadSmaps(QFINDTESTDATA("basic.smaps").toLocal8Bit()));
    //printMem();
    QCOMPARE(reader.memory.totalVm, 107384u);
    QCOMPARE(reader.memory.totalRss, 20352u);
    QCOMPARE(reader.memory.totalPss, 13814u);
    QCOMPARE(reader.memory.textVm, 7800u);
    QCOMPARE(reader.memory.textRss, 5884u);
    QCOMPARE(reader.memory.textPss, 2318u);
    QCOMPARE(reader.memory.heapVm, 24376u);
    QCOMPARE(reader.memory.heapRss, 7556u);
    QCOMPARE(reader.memory.heapPss, 7556u);
}

void tst_ProcessReader::memAdvanced()
{
    QVERIFY(reader.testReadSmaps(QFINDTESTDATA("advanced.smaps").toLocal8Bit()));
    //printMem();
    QCOMPARE(reader.memory.totalVm, 77728u);
    QCOMPARE(reader.memory.totalRss, 17612u);
    QCOMPARE(reader.memory.totalPss, 17547u);
    QCOMPARE(reader.memory.textVm, 2104u);
    QCOMPARE(reader.memory.textRss, 1772u);
    QCOMPARE(reader.memory.textPss, 1707u);
    QCOMPARE(reader.memory.heapVm, 16032u);
    QCOMPARE(reader.memory.heapRss, 15740u);
    QCOMPARE(reader.memory.heapPss, 15740u);
}

void tst_ProcessReader::printMem()
{
    qDebug() << "totalVm:" << reader.memory.totalVm;
    qDebug() << "totalRss:" << reader.memory.totalRss;
    qDebug() << "totalPss:" << reader.memory.totalPss;
    qDebug() << "textVm:" << reader.memory.textVm;
    qDebug() << "textRss:" << reader.memory.textRss;
    qDebug() << "textPss:" << reader.memory.textPss;
    qDebug() << "heapVm:" << reader.memory.heapVm;
    qDebug() << "heapRss:" << reader.memory.heapRss;
    qDebug() << "heapPss:" << reader.memory.heapPss;
}

QTEST_APPLESS_MAIN(tst_ProcessReader)

#include "tst_processreader.moc"
