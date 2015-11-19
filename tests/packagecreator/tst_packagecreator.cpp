/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QtTest>

#include "installationreport.h"
#include "utilities.h"
#include "packagecreator.h"

#include "../error-checking.h"


class tst_PackageCreator : public QObject
{
    Q_OBJECT

public:
    tst_PackageCreator();

private slots:
    void initTestCase();

    void createAndVerify_data();
    void createAndVerify();

private:
    QDir m_baseDir;
    bool m_tarAvailable = false;
};

tst_PackageCreator::tst_PackageCreator()
    : m_baseDir(AM_TESTDATA_DIR)
{ }

void tst_PackageCreator::initTestCase()
{
    // check if tar command is available at all

    QProcess tar;
    tar.start("tar", { "--version" });
    m_tarAvailable = tar.waitForStarted(3000)
            && tar.waitForFinished(3000)
            && (tar.exitStatus() == QProcess::NormalExit);
}

void tst_PackageCreator::createAndVerify_data()
{
    QTest::addColumn<QStringList>("files");
    QTest::addColumn<bool>("expectedSuccess");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("basic") << QStringList { "testfile" } << true << QString();
    QTest::newRow("no-such-file") << QStringList { "tastfile" } << false << "~file not found: .*";
}

void tst_PackageCreator::createAndVerify()
{
    QFETCH(QStringList, files);
    QFETCH(bool, expectedSuccess);
    QFETCH(QString, errorString);

    QTemporaryFile output;
    QVERIFY(output.open());

    InstallationReport report("com.pelagicore.test");
    report.addFiles(files);

    PackageCreator creator(m_baseDir, &output, report);
    bool result = creator.create();
    output.close();

    if (expectedSuccess) {
        QVERIFY2(result, qPrintable(creator.errorString()));
    } else {
        QVERIFY(creator.errorCode() != Error::None);
        QVERIFY(creator.errorCode() != Error::Canceled);
        QVERIFY(!creator.wasCanceled());

        AM_CHECK_ERRORSTRING(creator.errorString(), errorString);
        return;
    }

    // check the tar listing
    if (!m_tarAvailable)
        QSKIP("No tar command found in PATH - skipping the verification part of the test!");

    QProcess tar;
    tar.start("tar", { "-taf", output.fileName() });
    QVERIFY2(tar.waitForStarted(3000) &&
             tar.waitForFinished(3000) &&
             (tar.exitStatus() == QProcess::NormalExit) &&
             (tar.exitCode() == 0), qPrintable(tar.errorString()));

    QStringList expectedContents = files;
    expectedContents.sort();
    expectedContents.prepend("--PACKAGE-HEADER--");
    expectedContents.append("--PACKAGE-FOOTER--");
    QCOMPARE(expectedContents, QString::fromLocal8Bit(tar.readAllStandardOutput()).split('\n', QString::SkipEmptyParts));

    // check the contents of the files

    foreach (const QString &file, files) {
        QFile src(m_baseDir.absoluteFilePath(file));
        QVERIFY2(src.open(QFile::ReadOnly), qPrintable(src.errorString()));
        QByteArray data = src.readAll();

        tar.start("tar", { "-xaOf", output.fileName(), file });
        QVERIFY2(tar.waitForStarted(3000) &&
                 tar.waitForFinished(3000) &&
                 (tar.exitStatus() == QProcess::NormalExit) &&
                 (tar.exitCode() == 0), qPrintable(tar.errorString()));

        QCOMPARE(tar.readAllStandardOutput(), data);
    }
}

QTEST_GUILESS_MAIN(tst_PackageCreator)

#include "tst_packagecreator.moc"
