// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "exception.h"
#include "qtyaml.h"
#include "installationreport.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_InstallationReport : public QObject
{
    Q_OBJECT

public:
    tst_InstallationReport();

private Q_SLOTS:
    void test();
    void rejectsWrongSizeManifestDigest();
    void rejectsMissingManifestDigestOnV4();
    void serializeRefusesWithoutManifestDigest();
    void acceptsV3();
    void rejectsV3WithBadHmac();
};

tst_InstallationReport::tst_InstallationReport()
{ }

void tst_InstallationReport::test()
{
    QStringList files { u"test"_s, u"more/test"_s, u"another/test/file"_s };
    const QByteArray infoDigest = QCryptographicHash::hash("info.yaml-bytes", QCryptographicHash::Sha256);

    InstallationReport ir(u"test-pkg"_s);
    QVERIFY(!ir.isValid());
    ir.addFile(files.first());
    QVERIFY(!ir.isValid());
    ir.setDiskSpaceUsed(42);
    QVERIFY(!ir.isValid());
    ir.setDigest("##digest##");
    QVERIFY(ir.isValid());
    ir.setManifestDigest(infoDigest);
    ir.addFiles(files.mid(1));
    ir.setDeveloperSignature("%%dev-sig%%");
    ir.setStoreSignature("$$store-sig$$");

    QVERIFY(ir.isValid());
    QCOMPARE(ir.packageId(), u"test-pkg"_s);
    QCOMPARE(ir.files(), files);
    QCOMPARE(ir.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir.digest().constData(), "##digest##");
    QCOMPARE(ir.manifestDigest(), infoDigest);
    QCOMPARE(ir.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir.storeSignature().constData(), "$$store-sig$$");

    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
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
    QCOMPARE(ir2.packageId(), u"test-pkg"_s);
    QCOMPARE(ir2.files(), files);
    QCOMPARE(ir2.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir2.digest().constData(), "##digest##");
    QCOMPARE(ir2.manifestDigest(), infoDigest);
    QCOMPARE(ir2.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir2.storeSignature().constData(), "$$store-sig$$");

    // v4 reports are 2 YAML documents (header + content) with no HMAC trailer.
    auto docs = YamlParser::parseAllDocuments(buffer.buffer());
    QCOMPARE(docs.size(), 2);
}

void tst_InstallationReport::serializeRefusesWithoutManifestDigest()
{
    InstallationReport ir(u"test-pkg"_s);
    ir.addFile(u"f"_s);
    ir.setDigest("##d##");
    QVERIFY(ir.isValid()); // isValid does not require manifestDigest (v3 reports come in empty)

    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    QVERIFY(!ir.serialize(&buffer)); // but serialize MUST refuse without a v4 digest
}

void tst_InstallationReport::rejectsWrongSizeManifestDigest()
{
    InstallationReport ir(u"test-pkg"_s);
    ir.addFile(u"f"_s);
    ir.setDigest("##d##");
    ir.setManifestDigest(QByteArray(16, 'x')); // 16 bytes, not 32

    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    QVERIFY(!ir.serialize(&buffer));
}

void tst_InstallationReport::rejectsMissingManifestDigestOnV4()
{
    // Generate a v4 doc, then strip the manifestDigest field and re-emit.
    InstallationReport ir(u"test-pkg"_s);
    ir.addFile(u"f"_s);
    ir.setDigest("##d##");
    ir.setManifestDigest(QCryptographicHash::hash("x", QCryptographicHash::Sha256));

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QVERIFY(ir.serialize(&buffer));

    auto docs = YamlParser::parseAllDocuments(buffer.buffer());
    QCOMPARE(docs.size(), 2);
    QVariantMap root = docs[1].toMap();
    root.remove(u"manifestDigest"_s);
    docs[1] = root;

    QByteArray out = YamlEmitter::fromVariantDocuments({ docs[0], docs[1] }, YamlVersion::V1_1,
                                                       YamlEmitter::Style::Block);

    QBuffer in;
    in.setData(out);
    in.open(QIODevice::ReadOnly);

    InstallationReport ir2;
    bool threw = false;
    try {
        ir2.deserialize(&in);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY(threw);
}

void tst_InstallationReport::acceptsV3()
{
    QFile f(u":/legacy-installation-report-v3.yaml"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray bytes = f.readAll();
    QVERIFY(!bytes.isEmpty());

    QBuffer in;
    in.setData(bytes);
    in.open(QIODevice::ReadOnly);

    InstallationReport ir;
    try {
        ir.deserialize(&in);
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
    QCOMPARE(ir.packageId(), u"test-pkg"_s);
    QCOMPARE(ir.digest().constData(), "##d##");
    QVERIFY(ir.manifestDigest().isEmpty()); // v3 reports have no digest until migration fills it in
}

void tst_InstallationReport::rejectsV3WithBadHmac()
{
    QFile f(u":/legacy-installation-report-v3.yaml"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray bytes = f.readAll();
    QVERIFY(!bytes.isEmpty());

    // Flip the first hex character of the HMAC trailer.
    const QByteArray needle = "\nhmac: '";
    const qsizetype hmacStart = bytes.indexOf(needle) + needle.size();
    QVERIFY(hmacStart > needle.size());
    bytes[hmacStart] = (bytes[hmacStart] == '0') ? '1' : '0';

    QBuffer in;
    in.setData(bytes);
    in.open(QIODevice::ReadOnly);

    InstallationReport ir;
    bool threw = false;
    try {
        ir.deserialize(&in);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY(threw);
}

QTEST_APPLESS_MAIN(tst_InstallationReport)

#include "tst_installationreport.moc"
