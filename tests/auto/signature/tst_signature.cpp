// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/QtCore>
#include <QtTest/QtTest>

#include "global.h"
#include "signature.h"
#include "exception.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Signature : public QObject
{
    Q_OBJECT

public:
    tst_Signature();

private Q_SLOTS:
    void initTestCase();
    void basicCheck();
    void crossPlatform_data();
    void crossPlatform();
    void certificateData();
    void createSignature_data();
    void createSignature();
    void verifySignature_data();
    void verifySignature();
    void verifyCertBinding_data();
    void verifyCertBinding();
    void verifyCertHuge();

private:
    void parseInfo(const QByteArray &openSslInfo, QVariantMap &info);

    bool m_createNativeSignature = false;
    QByteArray m_signingP12;
    QByteArray m_signingNarrowP12;
    QByteArray m_signingHugeP12;
    QByteArray m_signingNoKeyP12;
    QByteArray m_signingPassword;
    QByteArrayList m_verifyingPEM;

    QByteArray m_p7Hash;
    QString m_p7OpenSsl;
    QString m_p7WinCrypt;

    QVariantMap m_caInfo;
    QVariantMap m_devcaInfo;
    QVariantMap m_devInfo;
};

tst_Signature::tst_Signature()
{
    m_createNativeSignature = qEnvironmentVariableIsSet("AM_CREATE_SIGNATURE_FILE");

    m_p7Hash = "p7Hash\n";
    m_p7OpenSsl = u":/signature-openssl.p7"_s;
    m_p7WinCrypt = u":/signature-wincrypt.p7"_s;
}

void tst_Signature::parseInfo(const QByteArray &opensslInfo, QVariantMap &info)
{
    const QStringList lines = QString::fromUtf8(opensslInfo).split(u'\n');
    for (const QString &line : lines) {
        if (line.isEmpty())
            continue;
        const int eq = line.indexOf(u'=');
        QVERIFY2(eq > 0, qPrintable(line));
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        QVERIFY2(!key.isEmpty(), qPrintable(line));
        QVERIFY2(!value.isEmpty(), qPrintable(line));
        info.insert(key, value);
    }
}

void tst_Signature::initTestCase()
{
    QFile s(u":/signing.p12"_s);
    QVERIFY(s.open(QIODevice::ReadOnly));
    m_signingP12 = s.readAll();
    QVERIFY(!m_signingP12.isEmpty());

    QFile sn(u":/signing-narrow.p12"_s);
    QVERIFY(sn.open(QIODevice::ReadOnly));
    m_signingNarrowP12 = sn.readAll();
    QVERIFY(!m_signingNarrowP12.isEmpty());

    QFile sh(u":/signing-huge.p12"_s);
    QVERIFY(sh.open(QIODevice::ReadOnly));
    m_signingHugeP12 = sh.readAll();
    QVERIFY(!m_signingHugeP12.isEmpty());

    QFile snk(u":/signing-no-key.p12"_s);
    QVERIFY(snk.open(QIODevice::ReadOnly));
    m_signingNoKeyP12 = snk.readAll();
    QVERIFY(!m_signingNoKeyP12.isEmpty());

    QFile v(u":/verifying.crt"_s);
    QVERIFY(v.open(QIODevice::ReadOnly));
    m_verifyingPEM << v.readAll();
    QVERIFY(!m_verifyingPEM.first().isEmpty());

    QFile cai(u":/root-ca.info"_s);
    QVERIFY(cai.open(QIODevice::ReadOnly));
    parseInfo(cai.readAll(), m_caInfo);
    QVERIFY(!m_caInfo.isEmpty());

    QFile dcai(u":/dev-ca.info"_s);
    QVERIFY(dcai.open(QIODevice::ReadOnly));
    parseInfo(dcai.readAll(), m_devcaInfo);
    QVERIFY(!m_devcaInfo.isEmpty());

    QFile di(u":/dev.info"_s);
    QVERIFY(di.open(QIODevice::ReadOnly));
    parseInfo(di.readAll(), m_devInfo);
    QVERIFY(!m_devInfo.isEmpty());

    m_signingPassword = "password";

    if (m_createNativeSignature) {
        QString nativeFile;
#if defined(QT_AM_USE_LIBCRYPTO)
        nativeFile = m_p7OpenSsl;
#elif defined(Q_OS_WINDOWS)
        nativeFile = m_p7WinCrypt;
#else
        static_assert(false);
#endif
        QVERIFY(!nativeFile.isEmpty());
        QFile f(QString::fromLatin1(AM_TESTSOURCE_DIR "/../signature") + nativeFile.mid(1));
        QVERIFY2(f.open(QFile::WriteOnly | QFile::Truncate), qPrintable(f.errorString()));

        Signature s(m_p7Hash);
        QByteArray signature;
        QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(m_signingP12, m_signingPassword));
        QVERIFY(!signature.isEmpty());
        QCOMPARE(f.write(signature), signature.size());

        qInfo() << "Creating signature file" << f.fileName() << "because $AM_CREATE_SIGNATURE_FILE is set.";
        QSKIP("Only creating signature");
        return;
    }
}

#define AM_VERIFY_THROWS_EXCEPTION(error_string, ...) \
do {\
        bool qverify_throws_exception_did_not_throw = false; \
        QT_TRY {\
            __VA_ARGS__; \
            QTest::qFail("Expected Exception with error string " #error_string " to be thrown" \
                         " but no exception caught", __FILE__, __LINE__); \
            qverify_throws_exception_did_not_throw = true; \
    } QT_CATCH (const Exception &e) { \
        if (!e.errorString().contains(error_string)) { \
            QString es = u"Exception was caught, but the error string does not match:\n" \
                         "  Expected: \"*" #error_string "*\"\n" \
                         "  Caught  : \"%1\""_s.arg(e.errorString()); \
            QTest::qFail(qPrintable(es), __FILE__, __LINE__); \
        } \
        /* success */ \
    } QT_CATCH (...) {\
            QTest::qCaught("Exception", __FILE__, __LINE__); \
            QTEST_FAIL_ACTION; \
    }\
    if (qverify_throws_exception_did_not_throw) \
        QTEST_FAIL_ACTION; \
} while (false)

void tst_Signature::basicCheck()
{
    if (m_createNativeSignature)
        QSKIP("Only creating signature");

    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;
    QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(m_signingP12, m_signingPassword));
    QVERIFY(!signature.isEmpty());

    Signature s2(hash + "bar");
    QByteArray signature2;
    QVERIFY_THROWS_NO_EXCEPTION(signature2 = s2.create(m_signingP12, m_signingPassword));
    QVERIFY(!signature2.isEmpty());
    QVERIFY(signature != signature2);

    Signature::VerificationResult result;
    QVERIFY_THROWS_NO_EXCEPTION(result = s.verify(signature, m_verifyingPEM));
    QVERIFY(result.isValid());
    QVERIFY_THROWS_NO_EXCEPTION(result = s2.verify(signature2, m_verifyingPEM));
    QVERIFY(result.isValid());
    AM_VERIFY_THROWS_EXCEPTION(u"Failed to verify", s.verify(signature2, m_verifyingPEM));
    AM_VERIFY_THROWS_EXCEPTION(u"Failed to verify", s2.verify(signature, m_verifyingPEM));

    AM_VERIFY_THROWS_EXCEPTION(u"not parse", s.create(m_signingP12, m_signingPassword + "not"));
    AM_VERIFY_THROWS_EXCEPTION(u"not read", s.create(QByteArray(), m_signingPassword).isEmpty());

    Signature s3(QByteArray(4096, 'x'));
    QVERIFY_THROWS_NO_EXCEPTION(QVERIFY(!s3.create(m_signingP12, m_signingPassword).isEmpty()));

    AM_VERIFY_THROWS_EXCEPTION(u"Failed to verify", s.verify(signature, QByteArrayList()));
    AM_VERIFY_THROWS_EXCEPTION(u"not load", s.verify(signature, QByteArrayList() << m_signingP12));
#if defined(Q_OS_WINDOWS)
    // Windows reads and verifies the PKCS#7 data in one function call
    const QString brokenSigError = u"ASN1 bad tag"_s;
#else
    const QString brokenSigError = u"not read"_s;
#endif
    AM_VERIFY_THROWS_EXCEPTION(brokenSigError, s.verify(hash, m_verifyingPEM));

    Signature s4 { QByteArray() };
    AM_VERIFY_THROWS_EXCEPTION(u"cannot sign an empty hash value", s4.create(m_signingP12, m_signingPassword));

    AM_VERIFY_THROWS_EXCEPTION(u"private key", s.create(m_signingNoKeyP12, m_signingPassword));
}

void tst_Signature::crossPlatform_data()
{
    QTest::addColumn<QString>("p7File");
    QTest::newRow("OpenSSL") << m_p7OpenSsl;
    QTest::newRow("WinCrypt") << m_p7WinCrypt;
    QTest::newRow("Legacy-SHA1") << u":/signature-legacy-sha1.p7"_s;
}

void tst_Signature::crossPlatform()
{
    QFETCH(QString, p7File);

    QFile f(p7File);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray sig = f.readAll();
    QVERIFY(!sig.isEmpty());

    Signature s(m_p7Hash);
    QVERIFY_THROWS_NO_EXCEPTION(s.verify(sig, m_verifyingPEM));
}

void tst_Signature::certificateData()
{
    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;
    QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(m_signingP12, m_signingPassword));
    QVERIFY(!signature.isEmpty());

    Signature::VerificationResult result;
    QVERIFY_THROWS_NO_EXCEPTION(result = s.verify(signature, m_verifyingPEM));

    QVERIFY(result.isValid());
    const auto signer = result.signer();
    QVERIFY(signer.isValid());
    QVERIFY(!result.issuers().isEmpty());
    const auto issuer = result.issuers().constFirst();
    QVERIFY(issuer.isValid());

    const QString expectedIssuerSerial = m_devcaInfo.value(u"serial"_s).toString();
    const QDateTime expectedIssuerNotBefore = QDateTime::fromString(m_devcaInfo.value(u"notBefore"_s).toString().simplified(), u"MMM d HH:mm:ss yyyy t"_s);
    const QDateTime expectedIssuerNotAfter = QDateTime::fromString(m_devcaInfo.value(u"notAfter"_s).toString().simplified(), u"MMM d HH:mm:ss yyyy t"_s);
    const QString expectedIssuerSHA256 = m_devcaInfo.value(u"sha256 Fingerprint"_s).toString().toLower();


    // these 2 are always set this way for issuer certificates
    QCOMPARE(int(issuer.keyUsages()),
             int(Certificate::KeyUsage::KeyCertSign | Certificate::KeyUsage::CRLSign));
    QCOMPARE(issuer.subjectAlternativeNames(), QStringList { });

    QCOMPARE(issuer.serialNumber(), expectedIssuerSerial);
    QCOMPARE(issuer.validityNotBefore(), expectedIssuerNotBefore);
    QCOMPARE(issuer.validityNotAfter(), expectedIssuerNotAfter);

    QCOMPARE(issuer.fingerprintAsString(), expectedIssuerSHA256);

    const QVariantMap issuerSubject {
        { u"commonName"_s, u"Pelagicore Developer CA"_s },
        { u"countryName"_s, u"DE"_s },
        { u"organizationName"_s, u"Pelagicore AG"_s },
        { u"organizationUnitName"_s, u"Developer Relations"_s }
    };
    QCOMPARE(issuer.subject(), issuerSubject);

    const QString expectedSignerSerial = m_devInfo.value(u"serial"_s).toString();
    const QDateTime expectedSignerNotBefore = QDateTime::fromString(m_devInfo.value(u"notBefore"_s).toString().simplified(), u"MMM d HH:mm:ss yyyy t"_s);
    const QDateTime expectedSignerNotAfter = QDateTime::fromString(m_devInfo.value(u"notAfter"_s).toString().simplified(), u"MMM d HH:mm:ss yyyy t"_s);
    const QString expectedSignerSHA256 = m_devInfo.value(u"sha256 Fingerprint"_s).toString().toLower();

    QCOMPARE(signer.keyUsages(), Certificate::KeyUsage::DigitalSignature
                                     | Certificate::KeyUsage::NonRepudiation
                                     | Certificate::KeyUsage::KeyEncipherment
                                     | Certificate::KeyUsage::DecipherOnly);
    QCOMPARE(signer.serialNumber(), expectedSignerSerial);
    QStringList ssans { u"qtam://packageid/other-*"_s, u"qtam://packageid/test-pkg"_s,
                        u"qtam://applicationid/*"_s, u"qtam://category/*"_s,
                        u"qtam://capability/*"_s };
    QCOMPARE(signer.subjectAlternativeNames(), ssans);
    QCOMPARE(signer.validityNotBefore(), expectedSignerNotBefore);
    QCOMPARE(signer.validityNotAfter(), expectedSignerNotAfter);

    QCOMPARE(signer.fingerprintAsString(), expectedSignerSHA256);

    const QVariantMap signerSubject {
        { u"commonName"_s, u"Developer 1"_s },
        { u"countryName"_s, u"DE"_s },
        { u"organizationName"_s, u"Developer 1 GmbH"_s }
    };
    QCOMPARE(signer.subject(), signerSubject);
}

void tst_Signature::verifySignature_data()
{
    QTest::addColumn<int>("keyUsage");
    QTest::addColumn<QString>("subjectAlternativeName");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("none") << 0 << QString { } << QString { };
    QTest::newRow("full") << int(Certificate::KeyUsage::DigitalSignature
                                 | Certificate::KeyUsage::NonRepudiation
                                 | Certificate::KeyUsage::KeyEncipherment
                                 | Certificate::KeyUsage::DecipherOnly) // == 0x107
                          << u"test-pkg"_s
                          << QString { };
    QTest::newRow("multiple-shas") << 0x107 << u"test-pkg"_s
                                   << QString { };
    QTest::newRow("wrong-keyusage") << 0x7 << u"test-pkg"_s
                                    << u"Key usage mismatch on certificate: expected 0x007, but got 0x107"_s;
    QTest::newRow("wrong-san") << 0x107 << u"test+pkg"_s
                               << u"Package ID mismatch: the certificate only allows"_s;
}

void tst_Signature::verifySignature()
{
    QFETCH(int, keyUsage);
    QFETCH(QString, subjectAlternativeName);
    QFETCH(QString, errorString);

    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;
    QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(m_signingP12, m_signingPassword));
    QVERIFY(!signature.isEmpty());

    if (keyUsage)
        s.requireKeyUsage(Certificate::KeyUsages(keyUsage));
    if (!subjectAlternativeName.isEmpty())
        s.requirePackageId(subjectAlternativeName);

    Signature::VerificationResult result;
    try {
        result = s.verify(signature, m_verifyingPEM);
        QVERIFY2(errorString.isEmpty(), "Verification should have failed");

        QVERIFY(result.isValid());
        QVERIFY(result.signer().isValid());
        QVERIFY(!result.issuers().isEmpty());
        QVERIFY(result.issuers().constFirst().isValid());
    } catch (const Exception &e) {
        QVERIFY2(!errorString.isEmpty(), qPrintable(u"Verification should have succeeded, but failed with: %1"_s
                                                        .arg(e.errorString())));
        QVERIFY2(e.errorString().contains(errorString), qPrintable(e.errorString()));
    }
}

void tst_Signature::createSignature_data()
{
    QTest::addColumn<int>("keyUsage");
    QTest::addColumn<QString>("subjectAlternativeName");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("none") << 0 << QString { } << QString { };
    QTest::newRow("full") << int(Certificate::KeyUsage::DigitalSignature
                                 | Certificate::KeyUsage::NonRepudiation
                                 | Certificate::KeyUsage::KeyEncipherment
                                 | Certificate::KeyUsage::DecipherOnly) // == 0x107
                          << u"test-pkg"_s
                          << QString { };
    QTest::newRow("other") << 0x107 << u"other-foo"_s
                           << u""_s;
    QTest::newRow("wrong-keyusage") << 0x7 << u"test-pkg"_s
                                    << u"Key usage mismatch on certificate: expected 0x007, but got 0x107"_s;
    QTest::newRow("wrong-san") << 0x107 << u"test+pkg"_s
                               << u"Package ID mismatch: the certificate only allows"_s;
}

void tst_Signature::createSignature()
{
    QFETCH(int, keyUsage);
    QFETCH(QString, subjectAlternativeName);
    QFETCH(QString, errorString);

    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;

    if (keyUsage)
        s.requireKeyUsage(Certificate::KeyUsages(keyUsage));
    if (!subjectAlternativeName.isEmpty())
        s.requirePackageId(subjectAlternativeName);

    try {
        signature = s.create(m_signingP12, m_signingPassword);
        QVERIFY2(errorString.isEmpty(), "Creation should have failed");

        QVERIFY(!signature.isEmpty());
    } catch (const Exception &e) {
        QVERIFY2(!errorString.isEmpty(), qPrintable(u"Creation should have succeeded, but failed with: %1"_s
                                                        .arg(e.errorString())));
        QVERIFY2(e.errorString().contains(errorString), qPrintable(e.errorString()));
    }

}

void tst_Signature::verifyCertBinding_data()
{
    // The "narrow" cert grants exactly:
    //   applicationid/test-app, capability/cap-allowed, category/test-category
    // The "permissive" cert (dev-1) grants:
    //   applicationid/*, capability/*, category/*

    QTest::addColumn<bool>("usePermissiveCert");
    QTest::addColumn<QString>("kind"); // "applicationid" | "capability" | "category"
    QTest::addColumn<QStringList>("required");
    QTest::addColumn<QString>("errorString");

    // applicationid
    QTest::newRow("appid-exact-match")     << false << u"applicationid"_s << QStringList { u"test-app"_s }                << QString { };
    QTest::newRow("appid-no-match")        << false << u"applicationid"_s << QStringList { u"rogue-app"_s }               << u"Application ID mismatch"_s;
    QTest::newRow("appid-empty-bypass")    << false << u"applicationid"_s << QStringList { }                              << QString { };
    QTest::newRow("appid-one-of-two-bad")  << false << u"applicationid"_s << QStringList { u"test-app"_s, u"rogue-app"_s } << u"Application ID mismatch"_s;
    QTest::newRow("appid-wildcard")        << true  << u"applicationid"_s << QStringList { u"anything-goes"_s }           << QString { };

    // capability
    QTest::newRow("cap-exact-match")       << false << u"capability"_s    << QStringList { u"cap-allowed"_s }             << QString { };
    QTest::newRow("cap-no-match")          << false << u"capability"_s    << QStringList { u"forbidden-cap"_s }           << u"Capabilities mismatch"_s;
    QTest::newRow("cap-empty-bypass")      << false << u"capability"_s    << QStringList { }                              << QString { };
    QTest::newRow("cap-one-of-two-bad")    << false << u"capability"_s    << QStringList { u"cap-allowed"_s, u"forbidden-cap"_s } << u"Capabilities mismatch"_s;
    QTest::newRow("cap-wildcard")          << true  << u"capability"_s    << QStringList { u"fancy-cap"_s }               << QString { };

    // category
    QTest::newRow("cat-exact-match")       << false << u"category"_s      << QStringList { u"test-category"_s }           << QString { };
    QTest::newRow("cat-no-match")          << false << u"category"_s      << QStringList { u"wrong-category"_s }          << u"Categories mismatch"_s;
    QTest::newRow("cat-empty-bypass")      << false << u"category"_s      << QStringList { }                              << QString { };
    QTest::newRow("cat-one-of-two-bad")    << false << u"category"_s      << QStringList { u"test-category"_s, u"wrong-category"_s } << u"Categories mismatch"_s;
    QTest::newRow("cat-wildcard")          << true  << u"category"_s      << QStringList { u"fancy-category"_s }          << QString { };
}

void tst_Signature::verifyCertBinding()
{
    QFETCH(bool, usePermissiveCert);
    QFETCH(QString, kind);
    QFETCH(QStringList, required);
    QFETCH(QString, errorString);

    const QByteArray &signingP12 = usePermissiveCert ? m_signingP12 : m_signingNarrowP12;

    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;
    QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(signingP12, m_signingPassword));
    QVERIFY(!signature.isEmpty());

    if (kind == u"applicationid"_s)
        s.requireApplicationIds(required);
    else if (kind == u"capability"_s)
        s.requireCapabilities(required);
    else if (kind == u"category"_s)
        s.requireCategories(required);
    else
        QFAIL("unknown kind");

    try {
        (void) s.verify(signature, m_verifyingPEM);
        QVERIFY2(errorString.isEmpty(), "Verification should have failed");
    } catch (const Exception &e) {
        QVERIFY2(!errorString.isEmpty(), qPrintable(u"Verification should have succeeded, but failed with: %1"_s
                                                        .arg(e.errorString())));
        QVERIFY2(e.errorString().contains(errorString), qPrintable(e.errorString()));
    }
}

void tst_Signature::verifyCertHuge()
{
#if defined(Q_OS_MACOS)
    // libressl limits SAN entries to 256
    QSKIP("libressl cannot load the huge test certificate");
#endif

    // The "huge" cert grants exactly:
    //   100 package-ids   : pkg-huge-001 .. pkg-huge-100
    //   100 app-ids       : app-huge-001 .. app-huge-100
    //   1000 capabilities : cap-huge-0001 .. cap-huge-1000
    //   200 categories    : cat-huge-001 .. cat-huge-200
    // The cert is used to confirm that there are no length limits anywhere in the
    // signing/verification chain.

    QStringList allPackageIds;
    for (int i = 1; i <= 100; ++i)
        allPackageIds << u"pkg-huge-%1"_s.arg(i, 3, 10, u'0');
    QStringList allApplicationIds;
    for (int i = 1; i <= 100; ++i)
        allApplicationIds << u"app-huge-%1"_s.arg(i, 3, 10, u'0');
    QStringList allCapabilities;
    for (int i = 1; i <= 1000; ++i)
        allCapabilities << u"cap-huge-%1"_s.arg(i, 4, 10, u'0');
    QStringList allCategories;
    for (int i = 1; i <= 200; ++i)
        allCategories << u"cat-huge-%1"_s.arg(i, 3, 10, u'0');

    QByteArray hash("foo");
    Signature s(hash);
    QByteArray signature;
    QVERIFY_THROWS_NO_EXCEPTION(signature = s.create(m_signingHugeP12, m_signingPassword));
    QVERIFY(!signature.isEmpty());

    // 1) the cert really exposes all 1400 binding entries
    Signature inspect(hash);
    Signature::VerificationResult plain;
    QVERIFY_THROWS_NO_EXCEPTION(plain = inspect.verify(signature, m_verifyingPEM));
    QCOMPARE(plain.signer().packageIds().size(), allPackageIds.size());
    QCOMPARE(plain.signer().applicationIds().size(), allApplicationIds.size());
    QCOMPARE(plain.signer().capabilities().size(), allCapabilities.size());
    QCOMPARE(plain.signer().categories().size(), allCategories.size());
    QCOMPARE(plain.signer().packageIds(), allPackageIds);
    QCOMPARE(plain.signer().applicationIds(), allApplicationIds);
    QCOMPARE(plain.signer().capabilities(), allCapabilities);
    QCOMPARE(plain.signer().categories(), allCategories);

    // 2) requiring all 1400 entries at once must verify without truncation
    s.requireApplicationIds(allApplicationIds);
    s.requireCapabilities(allCapabilities);
    s.requireCategories(allCategories);
    Signature::VerificationResult result;
    QVERIFY_THROWS_NO_EXCEPTION(result = s.verify(signature, m_verifyingPEM));
    QVERIFY(result.isValid());

    // packageId is a single value: check the first, last and one in between
    for (const QString &pkg : { u"pkg-huge-001"_s, u"pkg-huge-050"_s, u"pkg-huge-100"_s }) {
        Signature sp(hash);
        QByteArray sigp;
        QVERIFY_THROWS_NO_EXCEPTION(sigp = sp.create(m_signingHugeP12, m_signingPassword));
        sp.requirePackageId(pkg);
        QVERIFY_THROWS_NO_EXCEPTION(sp.verify(sigp, m_verifyingPEM));
    }

    // 3) an entry just outside each range must still be rejected
    {
        Signature sn(hash);
        QByteArray sign;
        QVERIFY_THROWS_NO_EXCEPTION(sign = sn.create(m_signingHugeP12, m_signingPassword));
        sn.requirePackageId(u"pkg-huge-101"_s);
        AM_VERIFY_THROWS_EXCEPTION(u"Package ID mismatch", sn.verify(sign, m_verifyingPEM));
    }
    {
        Signature sn(hash);
        QByteArray sign;
        QVERIFY_THROWS_NO_EXCEPTION(sign = sn.create(m_signingHugeP12, m_signingPassword));
        sn.requireApplicationIds(allApplicationIds + QStringList { u"app-huge-101"_s });
        AM_VERIFY_THROWS_EXCEPTION(u"Application ID mismatch", sn.verify(sign, m_verifyingPEM));
    }
    {
        Signature sn(hash);
        QByteArray sign;
        QVERIFY_THROWS_NO_EXCEPTION(sign = sn.create(m_signingHugeP12, m_signingPassword));
        sn.requireCapabilities(allCapabilities + QStringList { u"cap-huge-1001"_s });
        AM_VERIFY_THROWS_EXCEPTION(u"Capabilities mismatch", sn.verify(sign, m_verifyingPEM));
    }
    {
        Signature sn(hash);
        QByteArray sign;
        QVERIFY_THROWS_NO_EXCEPTION(sign = sn.create(m_signingHugeP12, m_signingPassword));
        sn.requireCategories(allCategories + QStringList { u"cat-huge-201"_s });
        AM_VERIFY_THROWS_EXCEPTION(u"Categories mismatch", sn.verify(sign, m_verifyingPEM));
    }
}

QTEST_GUILESS_MAIN(tst_Signature)

#include "tst_signature.moc"

