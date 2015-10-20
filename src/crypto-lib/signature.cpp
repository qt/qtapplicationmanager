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

#include <QDebug>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "exception.h"

#include "cryptography.h"
#include "signature.h"

class SignaturePrivate
{
public:
    QByteArray hash;
    QString error;
};


struct OpenSslDeleter {
    static inline void cleanup(X509 *x509)
    { X509_free(x509); }
    static inline void cleanup(BIO *bio)
    { BIO_free(bio); }
    static inline void cleanup(PKCS7 *pkcs7)
    { PKCS7_free(pkcs7); }
    static inline void cleanup(EVP_PKEY *pkey)
    { EVP_PKEY_free(pkey); }
    static inline void cleanup(PKCS12 *pkcs12)
    { PKCS12_free(pkcs12); }
    static inline void cleanup(X509_STORE *x509Store)
    { X509_STORE_free(x509Store); }
    static inline void cleanup(STACK_OF(X509) *stackOfX509)
    { sk_X509_pop_free(stackOfX509, X509_free); }
};

template <typename T> using OpenSslPointer = QScopedPointer<T, OpenSslDeleter>;

class CryptoException : public Exception
{
public:
    CryptoException(const char *message)
        : Exception(Error::Cryptography, message)
        , m_openSslErrorCode(ERR_get_error())
    {
        if (m_openSslErrorCode) {
            char buffer[512];
            buffer[sizeof(buffer) - 1] = 0;

            //void ERR_error_string_n(unsigned long e, char *buf, size_t len);
            ERR_error_string_n(m_openSslErrorCode, buffer, sizeof(buffer));

            m_errorString.append(": ");
            m_errorString.append(buffer);
        }
    }

    unsigned long openSslErrorCode() const
    {
        return m_openSslErrorCode;
    }

private:
    unsigned long m_openSslErrorCode;
};


Signature::Signature(const QByteArray &hash)
    : d(new SignaturePrivate())
{
    d->hash = hash;
    Cryptography::initialize();
}

Signature::~Signature()
{
    delete d;
}

QString Signature::errorString() const
{
    return d->error;
}


QByteArray Signature::create(const QByteArray &signingCertificatePkcs12, const QByteArray &signingCertificatePassword)
{
    d->error.clear();

    try {
        OpenSslPointer<BIO> bioPkcs12(BIO_new_mem_buf((void *) signingCertificatePkcs12.constData(), signingCertificatePkcs12.size()));
        if (!bioPkcs12)
            throw CryptoException("Could not create BIO buffer for PKCS#12 data");

        //PKCS12 *d2i_PKCS12_bio(BIO *bp, PKCS12 **p12);
        OpenSslPointer<PKCS12> pkcs12(d2i_PKCS12_bio(bioPkcs12.data(), 0));
        if (!pkcs12)
            throw CryptoException("Could not read PKCS#12 data from BIO buffer");

        //int PKCS12_parse(PKCS12 *p12, const char *pass, EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca);
        EVP_PKEY *tempSignKey = 0;
        X509 *tempSignCert = 0;
        STACK_OF(X509) *tempCaCerts = 0;
        int parseOk = PKCS12_parse(pkcs12.data(), signingCertificatePassword.constData(), &tempSignKey, &tempSignCert, &tempCaCerts);
        OpenSslPointer<EVP_PKEY> signKey(tempSignKey);
        OpenSslPointer<X509> signCert(tempSignCert);
        OpenSslPointer<STACK_OF(X509)> caCerts(tempCaCerts);

        if (!parseOk)
            throw CryptoException("Could not parse PKCS#12 data");

        if (!signKey)
            throw CryptoException("Could not find the private key within the PKCS#12 data");
        if (!signCert)
            throw CryptoException("Could not find the certificate within the PKCS#12 data");

        OpenSslPointer<BIO> bioHash(BIO_new_mem_buf((void *) d->hash.constData(), d->hash.size()));
        if (!bioHash)
            throw CryptoException("Could not create a BIO buffer for the hash");

        //PKCS7 *PKCS7_sign(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs, BIO *data, int flags);
        OpenSslPointer<PKCS7> signature(PKCS7_sign(signCert.data(), signKey.data(), caCerts.data(), bioHash.data(), PKCS7_DETACHED | PKCS7_BINARY));
        if (!signature)
            throw CryptoException("Could not create the PKCS#7 signature");

        OpenSslPointer<BIO> bioSignature(BIO_new(BIO_s_mem()));
        if (!bioSignature)
            throw CryptoException("Could not create a BIO buffer for the PKCS#7 signature");

        // int PEM_write_bio_PKCS7(BIO *bp, PKCS7 *x);
        if (!PEM_write_bio_PKCS7(bioSignature.data(), signature.data()))
            throw CryptoException("Could not write the PKCS#7 signature to the BIO buffer");

        char *data = 0;
        long size = BIO_get_mem_data(bioSignature.data(), &data);
        if (size <= 0 || !data)
            throw CryptoException("The BIO buffer for the PKCS#7 signature is invalid");

        return QByteArray(data, size);
    } catch (const std::exception &e) {
        d->error = QString::fromLocal8Bit(e.what());
        return QByteArray();
    }
}


bool Signature::verify(const QByteArray &signaturePkcs7, const QList<QByteArray> &chainOfTrust)
{
    d->error.clear();

    try {
        OpenSslPointer<BIO> bioSignature(BIO_new_mem_buf((void *) signaturePkcs7.constData(), signaturePkcs7.size()));
        if (!bioSignature)
            throw CryptoException("Could not create BIO buffer for PKCS#7 data");

        // PKCS7 *PEM_read_bio_PKCS7(BIO *bp, PKCS7 **x, pem_password_cb *cb, void *u);
        OpenSslPointer<PKCS7> signature(PEM_read_bio_PKCS7(bioSignature.data(), 0, 0, 0));
        if (!signature)
            throw CryptoException("Could not read PKCS#7 data from BIO buffer");

        OpenSslPointer<BIO> bioHash(BIO_new_mem_buf((void *) d->hash.constData(), d->hash.size()));
        if (!bioHash)
            throw CryptoException("Could not create BIO buffer for the hash");

        OpenSslPointer<X509_STORE> certChain(X509_STORE_new());
        if (!certChain)
            throw CryptoException("Could not create a X509 certificate store");

        foreach (const QByteArray &trustedCert, chainOfTrust) {
            OpenSslPointer<BIO> bioCert(BIO_new_mem_buf((void *) trustedCert.constData(), trustedCert.size()));
            if (!bioCert)
                throw CryptoException("Could not create BIO buffer for a certificate");

            while (!BIO_eof(bioCert.data())) {
                OpenSslPointer<X509> cert(PEM_read_bio_X509(bioCert.data(), 0, 0, 0));
                if (!cert)
                    throw CryptoException("Could not load a certificate from the chain of trust");
                if (!X509_STORE_add_cert(certChain.data(), cert.data()))
                    throw CryptoException("Could not add a certificate from the chain of trust to the certificate store");
                cert.take();
            }
        }

        // int PKCS7_verify(PKCS7 *p7, STACK_OF(X509) *certs, X509_STORE *store, BIO *indata, BIO *out, int flags);
        if (PKCS7_verify(signature.data(), 0, certChain.data(), bioHash.data(), 0, PKCS7_NOCHAIN) != 1) {
            CryptoException e("Failed to verify signature");
            if (e.openSslErrorCode())
                throw e;
            return false;
        } else {
            return true;
        }
    } catch (const std::exception &e) {
        d->error = QString::fromLocal8Bit(e.what());
        return false;
    }
}
