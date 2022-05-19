/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QtCore/QtCore>
#include <QtTest/QtTest>

#include "global.h"
#include "signature.h"
#include "cryptography.h"

QT_USE_NAMESPACE_AM

class tst_Signature : public QObject
{
    Q_OBJECT

public:
    tst_Signature();

private slots:
    void initTestCase();
    void check();
    void crossPlatform();

private:
    QByteArray m_signingP12;
    QByteArray m_signingNoKeyP12;
    QByteArray m_signingPassword;
    QList<QByteArray> m_verifyingPEM;
};

tst_Signature::tst_Signature()
{
    // OpenSSL3 changed a few defaults and it will not accept old PKCS12 certificates
    // anymore. Regenerating "signing.p12" doesn't help, because the macOS/iOS
    // SecurityFramework cannot deal with the new algorithms used by OpenSSL3.
    // The only way out for this cross-platform test case is to enable the so called
    // "legacy provider" in OpenSSL3 and continue testing with the old certificate.
    Cryptography::enableOpenSsl3LegacyProvider();
}

void tst_Signature::initTestCase()
{
    QFile s(qSL(":/signing.p12"));
    QVERIFY(s.open(QIODevice::ReadOnly));
    m_signingP12 = s.readAll();
    QVERIFY(!m_signingP12.isEmpty());

    QFile snk(qSL(":/signing-no-key.p12"));
    QVERIFY(snk.open(QIODevice::ReadOnly));
    m_signingNoKeyP12 = snk.readAll();
    QVERIFY(!m_signingNoKeyP12.isEmpty());

    QFile v(qSL(":/verifying.crt"));
    QVERIFY(v.open(QIODevice::ReadOnly));
    m_verifyingPEM << v.readAll();
    QVERIFY(!m_verifyingPEM.first().isEmpty());

    m_signingPassword = "password";
}

void tst_Signature::check()
{
    QByteArray hash("foo");
    Signature s(hash);
    QVERIFY(s.errorString().isEmpty());
    QByteArray signature = s.create(m_signingP12, m_signingPassword);
    QVERIFY2(!signature.isEmpty(), qPrintable(s.errorString()));

    Signature s2(hash + "bar");
    QByteArray signature2 = s2.create(m_signingP12, m_signingPassword);
    QVERIFY2(!signature2.isEmpty(), qPrintable(s2.errorString()));
    QVERIFY(signature != signature2);

    QVERIFY2(s.verify(signature, m_verifyingPEM), qPrintable(s.errorString()));
    QVERIFY2(s2.verify(signature2, m_verifyingPEM), qPrintable(s2.errorString()));
    QVERIFY(!s.verify(signature2, m_verifyingPEM));
    QVERIFY(!s2.verify(signature, m_verifyingPEM));

    QVERIFY(s.create(m_signingP12, m_signingPassword + "not").isEmpty());
    QVERIFY2(s.errorString().contains(qSL("not parse")), qPrintable(s.errorString()));

    QVERIFY(s.create(QByteArray(), m_signingPassword).isEmpty());
    QVERIFY2(s.errorString().contains(qSL("not read")), qPrintable(s.errorString()));

    Signature s3(QByteArray(4096, 'x'));
    QVERIFY(!s3.create(m_signingP12, m_signingPassword).isEmpty());

    QVERIFY(!s.verify(signature, QList<QByteArray>()));
    QVERIFY2(s.errorString().contains(qSL("Failed to verify")), qPrintable(s.errorString()));
    QVERIFY(!s.verify(signature, QList<QByteArray>() << m_signingP12));
    QVERIFY2(s.errorString().contains(qSL("not load")), qPrintable(s.errorString()));
    QVERIFY(!s.verify(hash, QList<QByteArray>() << m_signingP12));
    QVERIFY2(s.errorString().contains(qSL("not read")), qPrintable(s.errorString()));

    Signature s4 { QByteArray() };
    QVERIFY(s4.create(m_signingP12, m_signingPassword).isEmpty());

    QVERIFY(s.create(m_signingNoKeyP12, m_signingPassword).isEmpty());
    QVERIFY2(s.errorString().contains(qSL("private key")), qPrintable(s.errorString()));
}

void tst_Signature::crossPlatform()
{
    QByteArray hash = "hello\nworld!";

    QFile fileOpenSsl(qSL(":/signature-openssl.p7"));
    QFile fileWinCrypt(qSL(":/signature-wincrypt.p7"));
    QFile fileSecurityFramework(qSL(":/signature-securityframework.p7"));

    if (qEnvironmentVariableIsSet("AM_CREATE_SIGNATURE_FILE")) {
        QFile *nativeFile = nullptr;
#if defined(AM_USE_LIBCRYPTO)
        nativeFile = &fileOpenSsl;
#elif defined(Q_OS_WIN)
        nativeFile = &fileWinCrypt;
#elif defined(Q_OS_OSX)
        nativeFile = &fileSecurityFramework;
#endif
        QVERIFY(nativeFile);
        QFile f(qL1S(AM_TESTDATA_DIR "/../signature") + nativeFile->fileName().mid(1));
        QVERIFY2(f.open(QFile::WriteOnly | QFile::Truncate), qPrintable(f.errorString()));

        Signature s(hash);
        QByteArray signature = s.create(m_signingP12, m_signingPassword);
        QVERIFY2(!signature.isEmpty(), qPrintable(s.errorString()));
        QCOMPARE(f.write(signature), signature.size());

        qInfo() << "Only creating signature file" << f.fileName() << "because $AM_CREATE_SIGNATURE_FILE is set.";
        return;
    }

    QVERIFY(fileOpenSsl.open(QIODevice::ReadOnly));
    QByteArray sigOpenSsl = fileOpenSsl.readAll();
    QVERIFY(!sigOpenSsl.isEmpty());
    QVERIFY(fileWinCrypt.open(QIODevice::ReadOnly));
    QByteArray sigWinCrypt = fileWinCrypt.readAll();
    QVERIFY(!sigWinCrypt.isEmpty());
    QVERIFY(fileSecurityFramework.open(QIODevice::ReadOnly));
    QByteArray sigSecurityFramework = fileSecurityFramework.readAll();
    QVERIFY(!sigSecurityFramework.isEmpty());

    Signature s(hash);
    QVERIFY2(s.verify(sigOpenSsl, m_verifyingPEM), qPrintable(s.errorString()));
    QVERIFY2(s.verify(sigWinCrypt, m_verifyingPEM), qPrintable(s.errorString()));
    QVERIFY2(s.verify(sigSecurityFramework, m_verifyingPEM), qPrintable(s.errorString()));
}

QTEST_APPLESS_MAIN(tst_Signature)

#include "tst_signature.moc"

