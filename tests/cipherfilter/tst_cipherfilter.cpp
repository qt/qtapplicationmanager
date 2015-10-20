/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include <QtCore>
#include <QtTest>

#include "cipherfilter.h"

/*
 # create 373 bytes of random data
 $ dd if=/dev/urandom bs=373 count=1 >plain.bin

 # create encrypted files
 $ for i in cbc cfb ofb; do openssl enc -in plain.bin -out aes-128-$i.bin -aes-128-$i -K 00112233445566778899aabbccddeeff -iv ffeeddccbbaa99887766554433221100; done
 $ for i in cbc cfb ofb; do openssl enc -in plain.bin -out aes-256-$i.bin -aes-256-$i -K 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff -iv ffeeddccbbaa99887766554433221100; done
 */

class tst_CipherFilter : public QObject
{
    Q_OBJECT

public:
    tst_CipherFilter();

private slots:
    void initTestCase();

    void ciphers_data();
    void ciphers();
    void misc();

private:
    QByteArray m_plain;
};

Q_DECLARE_METATYPE(CipherFilter::Cipher)

tst_CipherFilter::tst_CipherFilter()
{
}

void tst_CipherFilter::initTestCase()
{
    QFile plain(":/plain.bin");
    QVERIFY(plain.open(QIODevice::ReadOnly));
    m_plain = plain.readAll();
    QCOMPARE(plain.size(), 373);
}

void tst_CipherFilter::ciphers_data()
{
    QTest::addColumn<CipherFilter::Cipher>("cipher");
    QTest::addColumn<int>("keySize");
    QTest::addColumn<int>("ivSize");
    QTest::addColumn<int>("blockSize");
    QTest::addColumn<QString>("name");

    QTest::newRow("aes-128-cfb") << CipherFilter::AES_128_CFB << 16 << 16 << 1 << "aes-128-cfb";
    QTest::newRow("aes-128-ofb") << CipherFilter::AES_128_OFB << 16 << 16 << 1 << "aes-128-ofb";
    QTest::newRow("aes-128-cbc") << CipherFilter::AES_128_CBC << 16 << 16 << 16 << "aes-128-cbc";
    QTest::newRow("aes-256-cfb") << CipherFilter::AES_256_CFB << 32 << 16 << 1 << "aes-256-cfb";
    QTest::newRow("aes-256-ofb") << CipherFilter::AES_256_OFB << 32 << 16 << 1 << "aes-256-ofb";
    QTest::newRow("aes-256-cbc") << CipherFilter::AES_256_CBC << 32 << 16 << 16 << "aes-256-cbc";
}


void tst_CipherFilter::ciphers()
{
    QFETCH(CipherFilter::Cipher, cipher);
    QFETCH(int, keySize);
    QFETCH(int, ivSize);
    QFETCH(int, blockSize);
    QFETCH(QString, name);

    CipherFilter enc(CipherFilter::Encrypt, cipher);
    CipherFilter dec(CipherFilter::Decrypt, cipher);

    QCOMPARE(enc.keySize(), keySize);
    QCOMPARE(enc.ivSize(), ivSize);
    QCOMPARE(enc.blockSize(), blockSize);
    QCOMPARE(dec.keySize(), keySize);
    QCOMPARE(dec.ivSize(), ivSize);
    QCOMPARE(dec.blockSize(), blockSize);

    QString fileName = QString::fromLatin1(":/%1.bin").arg(name);
    QFile f(fileName);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray encryptedFile = f.readAll();
    QVERIFY(!encryptedFile.isEmpty());

    QVERIFY((encryptedFile.size() % blockSize) == 0);

    QByteArray key(keySize, 0);
    QByteArray iv(ivSize, 0);
    for (int i = 0; i < keySize; ++i)
        key[i] = ((i % 16) << 4) | (i % 16);
    for (int i = 0; i < ivSize; ++i)
        iv[i] = (((ivSize - i - 1) % 16) << 4) | ((ivSize - i - 1) % 16);

    QVERIFY(enc.start(key.constData(), keySize, iv.constData(), ivSize));

    QByteArray encrypted;
    int piped = 0;
    while (piped < m_plain.size()) {
        QByteArray src = m_plain.mid(piped, 23);
        QByteArray dst;
        QVERIFY(enc.processData(src, dst));
        piped += src.size();
        encrypted += dst;
    }
    QByteArray dst;
    QVERIFY(enc.finish(dst));
    encrypted += dst;

    QCOMPARE(encrypted, encryptedFile);

    QVERIFY(dec.start(key.constData(), keySize, iv.constData(), ivSize));

    QByteArray decrypted;
    piped = 0;
    while (piped < encrypted.size()) {
        QByteArray src = encrypted.mid(piped, 23);
        QByteArray dst;
        QVERIFY(dec.processData(src, dst));
        piped += src.size();
        decrypted += dst;
    }
    dst.clear();
    QVERIFY(dec.finish(dst));
    decrypted += dst;

    QCOMPARE(decrypted, m_plain);
}


void tst_CipherFilter::misc()
{
    QByteArray key(16, 'A');
    QByteArray iv(16, 'B');

    {
        CipherFilter enc(CipherFilter::Encrypt, CipherFilter::AES_128_CBC);
        QVERIFY(!enc.start(0, key.size(), iv.constData(), iv.size()));
        QVERIFY(!enc.start(key.constData(), 0, iv.constData(), iv.size()));
        QVERIFY(!enc.start(key.constData(), key.size(), 0, iv.size()));
        QVERIFY(!enc.start(key.constData(), key.size(), iv.constData(), 0));
        QVERIFY(enc.start(key.constData(), key.size(), iv.constData(), iv.size()));
        QVERIFY(!enc.start(key.constData(), key.size(), iv.constData(), iv.size()));
        QVERIFY(enc.errorString().contains("already start"));
    }
    {
        CipherFilter enc(CipherFilter::Encrypt, CipherFilter::AES_128_CBC);
        QByteArray dst;
        QVERIFY(!enc.processData(QByteArray(), dst));
        QVERIFY(enc.errorString().contains("not start"));
        QVERIFY(enc.start(key.constData(), key.size(), iv.constData(), iv.size()));
        QVERIFY(enc.processData(QByteArray(), dst));
    }
    {
        CipherFilter enc(CipherFilter::Encrypt, CipherFilter::AES_128_CBC);
        QByteArray dst;
        QVERIFY(!enc.finish(dst));
        QVERIFY(enc.errorString().contains("not start"));
    }
    {
        CipherFilter enc(CipherFilter::Encrypt, (CipherFilter::Cipher) -1);
        QVERIFY(!enc.start(key.constData(), key.size(), iv.constData(), iv.size()));
        QVERIFY(enc.errorString().contains("invalid"));
        QCOMPARE(enc.keySize(), 0);
    }
}

QTEST_APPLESS_MAIN(tst_CipherFilter)

#include "tst_cipherfilter.moc"

