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
#include "private/processmonitor_p.h"

QT_USE_NAMESPACE_AM

class ReadingTaskTester : public ReadingTask
{
public:
    using ReadingTask::readMemory;
};

class tst_ProcessMonitor : public QObject
{
    Q_OBJECT

public:
    tst_ProcessMonitor();

private slots:
    void memInvalid_data();
    void memInvalid();
    void memTestProcess();
    void memBasic();
    void memAdvanced();

private:
    void printMem(const ReadingTask::Results::Memory &memres);

    ReadingTask task;
    ReadingTask::Results readResults;
    QMutex mutex;
};

tst_ProcessMonitor::tst_ProcessMonitor() : task(mutex, readResults)
{}

void tst_ProcessMonitor::memInvalid_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("arbitrary") << QFINDTESTDATA("tst_processmonitor.cpp");
    QTest::newRow("binary") << QFINDTESTDATA("tst_processmonitor");
    QTest::newRow("missingvalue") << QFINDTESTDATA("invalid.smaps");
}

void tst_ProcessMonitor::memInvalid()
{
    QFETCH(QString, file);

    ReadingTask::Results::Memory memres;
    QVERIFY(!static_cast<ReadingTaskTester*>(&task)->readMemory(file.toLocal8Bit(), memres));
    QCOMPARE(memres.totalVm, 0u);
    QCOMPARE(memres.totalRss, 0u);
    QCOMPARE(memres.totalPss, 0u);
    QCOMPARE(memres.textVm, 0u);
    QCOMPARE(memres.textRss, 0u);
    QCOMPARE(memres.textPss, 0u);
    QCOMPARE(memres.heapVm, 0u);
    QCOMPARE(memres.heapRss, 0u);
    QCOMPARE(memres.heapPss, 0u);
}

void tst_ProcessMonitor::memTestProcess()
{
    ReadingTask::Results::Memory memres;
    const QByteArray file = "/proc/" + QByteArray::number(QCoreApplication::applicationPid()) + "/smaps";
    QVERIFY(static_cast<ReadingTaskTester*>(&task)->readMemory(file, memres));
    //printMem(memres);
    QVERIFY(memres.totalVm >= memres.totalRss);
    QVERIFY(memres.totalRss >= memres.totalPss);
    QVERIFY(memres.textVm >= memres.textRss);
    QVERIFY(memres.textRss >= memres.textPss);
    QVERIFY(memres.heapVm >= memres.heapRss);
    QVERIFY(memres.heapRss >= memres.heapPss);
}

void tst_ProcessMonitor::memBasic()
{
    ReadingTask::Results::Memory memres;
    QVERIFY(static_cast<ReadingTaskTester*>(&task)->readMemory(QFINDTESTDATA("basic.smaps").toLocal8Bit(), memres));
    //printMem(memres);
    QCOMPARE(memres.totalVm, 107384u);
    QCOMPARE(memres.totalRss, 20352u);
    QCOMPARE(memres.totalPss, 13814u);
    QCOMPARE(memres.textVm, 7800u);
    QCOMPARE(memres.textRss, 5884u);
    QCOMPARE(memres.textPss, 2318u);
    QCOMPARE(memres.heapVm, 24376u);
    QCOMPARE(memres.heapRss, 7556u);
    QCOMPARE(memres.heapPss, 7556u);
}

void tst_ProcessMonitor::memAdvanced()
{
    ReadingTask::Results::Memory memres;
    QVERIFY(static_cast<ReadingTaskTester*>(&task)->readMemory(QFINDTESTDATA("advanced.smaps").toLocal8Bit(), memres));
    //printMem(memres);
    QCOMPARE(memres.totalVm, 77728u);
    QCOMPARE(memres.totalRss, 17612u);
    QCOMPARE(memres.totalPss, 17547u);
    QCOMPARE(memres.textVm, 2104u);
    QCOMPARE(memres.textRss, 1772u);
    QCOMPARE(memres.textPss, 1707u);
    QCOMPARE(memres.heapVm, 16032u);
    QCOMPARE(memres.heapRss, 15740u);
    QCOMPARE(memres.heapPss, 15740u);
}

void tst_ProcessMonitor::printMem(const ReadingTask::Results::Memory &memres)
{
    qDebug() << "totalVm:" << memres.totalVm;
    qDebug() << "totalRss:" << memres.totalRss;
    qDebug() << "totalPss:" << memres.totalPss;
    qDebug() << "textVm:" << memres.textVm;
    qDebug() << "textRss:" << memres.textRss;
    qDebug() << "textPss:" << memres.textPss;
    qDebug() << "heapVm:" << memres.heapVm;
    qDebug() << "heapRss:" << memres.heapRss;
    qDebug() << "heapPss:" << memres.heapPss;
}

QTEST_APPLESS_MAIN(tst_ProcessMonitor)

#include "tst_processmonitor.moc"
