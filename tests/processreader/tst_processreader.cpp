/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
    void printMem(const ProcessReader &reader);
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
    //printMem(reader);
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
    //printMem(reader);
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
    //printMem(reader);
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

void tst_ProcessReader::printMem(const ProcessReader &reader)
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
