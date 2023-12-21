// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "exception.h"
#include "installationreport.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_InstallationReport : public QObject
{
    Q_OBJECT

public:
    tst_InstallationReport();

private slots:
    void test();
};

tst_InstallationReport::tst_InstallationReport()
{ }

void tst_InstallationReport::test()
{
    QStringList files { u"test"_s, u"more/test"_s, u"another/test/file"_s };

    InstallationReport ir(u"com.pelagicore.test"_s);
    QVERIFY(!ir.isValid());
    ir.addFile(files.first());
    QVERIFY(!ir.isValid());
    ir.setDiskSpaceUsed(42);
    QVERIFY(!ir.isValid());
    ir.setDigest("##digest##");
    QVERIFY(ir.isValid());
    ir.addFiles(files.mid(1));
    ir.setDeveloperSignature("%%dev-sig%%");
    ir.setStoreSignature("$$store-sig$$");

    QVERIFY(ir.isValid());
    QCOMPARE(ir.packageId(), u"com.pelagicore.test"_s);
    QCOMPARE(ir.files(), files);
    QCOMPARE(ir.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir.digest().constData(), "##digest##");
    QCOMPARE(ir.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir.storeSignature().constData(), "$$store-sig$$");

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QVERIFY(ir.serialize(&buffer));
    buffer.seek(0);

    InstallationReport ir2;
    try {
        ir2.deserialize(&buffer);
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
    buffer.seek(0);

    QVERIFY(ir2.isValid());
    QCOMPARE(ir2.packageId(), u"com.pelagicore.test"_s);
    QCOMPARE(ir2.files(), files);
    QCOMPARE(ir2.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir2.digest().constData(), "##digest##");
    QCOMPARE(ir2.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir2.storeSignature().constData(), "$$store-sig$$");

    QByteArray &yaml = buffer.buffer();
    QVERIFY(!yaml.isEmpty());

    qsizetype pos = yaml.lastIndexOf("\n---\nhmac: '");
    QVERIFY(pos > 0);
    pos += 12;
    QByteArray hmac = QMessageAuthenticationCode::hash("data", "key", QCryptographicHash::Sha256).toHex();
    yaml.replace(pos, hmac.size(), hmac);
    QCOMPARE(yaml.mid(pos + hmac.size(), 2).constData(), "'\n");

    try {
        ir2.deserialize(&buffer);
        QVERIFY(false);
    } catch (...) {
    }
}

QTEST_APPLESS_MAIN(tst_InstallationReport)

#include "tst_installationreport.moc"
