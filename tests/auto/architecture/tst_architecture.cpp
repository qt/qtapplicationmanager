// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>
#include <QtEndian>
#include <QString>

#include "architecture.h"

QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;

class tst_Architecture : public QObject
{
    Q_OBJECT

public:
    tst_Architecture() = default;

private slots:
    void initTestCase();
    void architecture_data();
    void architecture();
};


void tst_Architecture::initTestCase()
{
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/binaries")).exists())
        QSKIP("No test binaries available in the data/ directory");
}

void tst_Architecture::architecture_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("archId");

    QTest::newRow("android-elf")     << "android-elf.so.zz"  << "android_arm_32";
    QTest::newRow("windows-pe")      << "windows-pe.exe.zz"  << "windows_x86_64";
    QTest::newRow("macos-macho")     << "macos-macho.zz"     << "macos_arm_64";
    QTest::newRow("macos-universal") << "macos-universal.zz" << "macos_universal_arm_64+x86_64";
}

void tst_Architecture::architecture()
{
    QFETCH(QString, path);
    QFETCH(QString, archId);

    QString fullPath = QString::fromLatin1(AM_TESTDATA_DIR) + u"/binaries/"_s + path;
    QFile src(fullPath);
    QVERIFY(src.open(QIODevice::ReadOnly));
    auto content = src.readAll();
    quint32 contentLen = content.size();
    QVERIFY(contentLen > 0);
    content = qUncompress(content);
    QVERIFY(content.size() > 0);
    QTemporaryFile dst;
    QVERIFY(dst.open());
    QVERIFY(dst.write(content) == content.size());
    dst.close();
    QString id = Architecture::identify(dst.fileName());

    QCOMPARE(id, archId);
}

QTEST_APPLESS_MAIN(tst_Architecture)

#include "tst_architecture.moc"
