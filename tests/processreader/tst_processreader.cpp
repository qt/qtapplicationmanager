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

    reader.readSmaps(file.toLocal8Bit());

    QCOMPARE(reader.totalVm.load(), 0u);
    QCOMPARE(reader.totalRss.load(), 0u);
    QCOMPARE(reader.totalPss.load(), 0u);
    QCOMPARE(reader.textVm.load(), 0u);
    QCOMPARE(reader.textRss.load(), 0u);
    QCOMPARE(reader.textPss.load(), 0u);
    QCOMPARE(reader.heapVm.load(), 0u);
    QCOMPARE(reader.heapRss.load(), 0u);
    QCOMPARE(reader.heapPss.load(), 0u);
}

void tst_ProcessReader::memTestProcess()
{
    const QByteArray file = "/proc/" + QByteArray::number(QCoreApplication::applicationPid()) + "/smaps";

    QVERIFY(reader.readSmaps(file));
    //printMem(reader);
    QVERIFY(reader.totalVm.load() >= reader.totalRss.load());
    QVERIFY(reader.totalRss.load() >= reader.totalPss.load());
    QVERIFY(reader.textVm.load() >= reader.textRss.load());
    QVERIFY(reader.textRss.load() >= reader.textPss.load());
    QVERIFY(reader.heapVm.load() >= reader.heapRss.load());
    QVERIFY(reader.heapRss.load() >= reader.heapPss.load());
}

void tst_ProcessReader::memBasic()
{
    QVERIFY(reader.readSmaps(QFINDTESTDATA("basic.smaps").toLocal8Bit()));
    //printMem(reader);
    QCOMPARE(reader.totalVm.load(), 107384u);
    QCOMPARE(reader.totalRss.load(), 20352u);
    QCOMPARE(reader.totalPss.load(), 13814u);
    QCOMPARE(reader.textVm.load(), 7800u);
    QCOMPARE(reader.textRss.load(), 5884u);
    QCOMPARE(reader.textPss.load(), 2318u);
    QCOMPARE(reader.heapVm.load(), 24376u);
    QCOMPARE(reader.heapRss.load(), 7556u);
    QCOMPARE(reader.heapPss.load(), 7556u);
}

void tst_ProcessReader::memAdvanced()
{
    QVERIFY(reader.readSmaps(QFINDTESTDATA("advanced.smaps").toLocal8Bit()));
    //printMem(reader);
    QCOMPARE(reader.totalVm.load(), 77728u);
    QCOMPARE(reader.totalRss.load(), 17612u);
    QCOMPARE(reader.totalPss.load(), 17547u);
    QCOMPARE(reader.textVm.load(), 2104u);
    QCOMPARE(reader.textRss.load(), 1772u);
    QCOMPARE(reader.textPss.load(), 1707u);
    QCOMPARE(reader.heapVm.load(), 16032u);
    QCOMPARE(reader.heapRss.load(), 15740u);
    QCOMPARE(reader.heapPss.load(), 15740u);
}

void tst_ProcessReader::printMem(const ProcessReader &reader)
{
    qDebug() << "totalVm:" << reader.totalVm.load();
    qDebug() << "totalRss:" << reader.totalRss.load();
    qDebug() << "totalPss:" << reader.totalPss.load();
    qDebug() << "textVm:" << reader.textVm.load();
    qDebug() << "textRss:" << reader.textRss.load();
    qDebug() << "textPss:" << reader.textPss.load();
    qDebug() << "heapVm:" << reader.heapVm.load();
    qDebug() << "heapRss:" << reader.heapRss.load();
    qDebug() << "heapPss:" << reader.heapPss.load();
}

QTEST_APPLESS_MAIN(tst_ProcessReader)

#include "tst_processreader.moc"
