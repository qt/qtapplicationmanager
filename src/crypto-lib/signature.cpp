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
#include "libcryptofunction.h"

// deleter
static AM_LIBCRYPTO_FUNCTION(X509_free);
static AM_LIBCRYPTO_FUNCTION(BIO_free, 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_free);
static AM_LIBCRYPTO_FUNCTION(EVP_PKEY_free);
static AM_LIBCRYPTO_FUNCTION(PKCS12_free);
static AM_LIBCRYPTO_FUNCTION(X509_STORE_free);
static AM_LIBCRYPTO_FUNCTION(sk_pop_free);

// error handling
static AM_LIBCRYPTO_FUNCTION(ERR_get_error, ERR_R_INTERNAL_ERROR);
static AM_LIBCRYPTO_FUNCTION(ERR_error_string_n);

// create
static AM_LIBCRYPTO_FUNCTION(BIO_ctrl, 0);
static AM_LIBCRYPTO_FUNCTION(d2i_PKCS12_bio, nullptr);
static AM_LIBCRYPTO_FUNCTION(PKCS12_parse, 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_sign, nullptr);
static AM_LIBCRYPTO_FUNCTION(BIO_new, nullptr);
static AM_LIBCRYPTO_FUNCTION(BIO_s_mem, nullptr);
static AM_LIBCRYPTO_FUNCTION(i2d_PKCS7, 0);
static AM_LIBCRYPTO_FUNCTION(PEM_ASN1_write_bio, 0);

// verify
static AM_LIBCRYPTO_FUNCTION(BIO_new_mem_buf, nullptr);
static AM_LIBCRYPTO_FUNCTION(PEM_ASN1_read_bio, nullptr);
static AM_LIBCRYPTO_FUNCTION(d2i_PKCS7, nullptr);
static AM_LIBCRYPTO_FUNCTION(d2i_X509, nullptr);
static AM_LIBCRYPTO_FUNCTION(X509_STORE_new, nullptr);
static AM_LIBCRYPTO_FUNCTION(X509_STORE_add_cert, 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_verify, 0);

class SignaturePrivate
{
public:
    QByteArray hash;
    QString error;
};


struct OpenSslDeleter {
    static inline void cleanup(X509 *x509)
    { am_X509_free(x509); }
    static inline void cleanup(BIO *bio)
    { am_BIO_free(bio); }
    static inline void cleanup(PKCS7 *pkcs7)
    { am_PKCS7_free(pkcs7); }
    static inline void cleanup(EVP_PKEY *pkey)
    { am_EVP_PKEY_free(pkey); }
    static inline void cleanup(PKCS12 *pkcs12)
    { am_PKCS12_free(pkcs12); }
    static inline void cleanup(X509_STORE *x509Store)
    { am_X509_STORE_free(x509Store); }
    static inline void cleanup(STACK_OF(X509) *stackOfX509)
    { am_sk_pop_free(CHECKED_STACK_OF(X509, stackOfX509), CHECKED_SK_FREE_FUNC(X509, am_X509_free.functionPointer())); }
};

template <typename T> using OpenSslPointer = QScopedPointer<T, OpenSslDeleter>;

class CryptoException : public Exception
{
public:
    CryptoException(const char *message)
        : Exception(Error::Cryptography, message)
        , m_openSslErrorCode(am_ERR_get_error())
    {
        if (m_openSslErrorCode) {
            char buffer[512];
            buffer[sizeof(buffer) - 1] = 0;

            //void ERR_error_string_n(unsigned long e, char *buf, size_t len);
            am_ERR_error_string_n(m_openSslErrorCode, buffer, sizeof(buffer));

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
        OpenSslPointer<BIO> bioPkcs12(am_BIO_new_mem_buf((void *) signingCertificatePkcs12.constData(), signingCertificatePkcs12.size()));
        if (!bioPkcs12)
            throw CryptoException("Could not create BIO buffer for PKCS#12 data");

        //PKCS12 *d2i_PKCS12_bio(BIO *bp, PKCS12 **p12);
        OpenSslPointer<PKCS12> pkcs12(am_d2i_PKCS12_bio(bioPkcs12.data(), nullptr));
        if (!pkcs12)
            throw CryptoException("Could not read PKCS#12 data from BIO buffer");

        //int PKCS12_parse(PKCS12 *p12, const char *pass, EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca);
        EVP_PKEY *tempSignKey = 0;
        X509 *tempSignCert = 0;
        STACK_OF(X509) *tempCaCerts = 0;
        int parseOk = am_PKCS12_parse(pkcs12.data(), signingCertificatePassword.constData(), &tempSignKey, &tempSignCert, &tempCaCerts);
        OpenSslPointer<EVP_PKEY> signKey(tempSignKey);
        OpenSslPointer<X509> signCert(tempSignCert);
        OpenSslPointer<STACK_OF(X509)> caCerts(tempCaCerts);

        if (!parseOk)
            throw CryptoException("Could not parse PKCS#12 data");

        if (!signKey)
            throw CryptoException("Could not find the private key within the PKCS#12 data");
        if (!signCert)
            throw CryptoException("Could not find the certificate within the PKCS#12 data");

        OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf((void *) d->hash.constData(), d->hash.size()));
        if (!bioHash)
            throw CryptoException("Could not create a BIO buffer for the hash");

        //PKCS7 *PKCS7_sign(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs, BIO *data, int flags);
        OpenSslPointer<PKCS7> signature(am_PKCS7_sign(signCert.data(), signKey.data(), caCerts.data(), bioHash.data(), PKCS7_DETACHED | PKCS7_BINARY));
        if (!signature)
            throw CryptoException("Could not create the PKCS#7 signature");

        OpenSslPointer<BIO> bioSignature(am_BIO_new(am_BIO_s_mem()));
        if (!bioSignature)
            throw CryptoException("Could not create a BIO buffer for the PKCS#7 signature");

        // int PEM_write_bio_PKCS7(BIO *bp, PKCS7 *x);
        if (!am_PEM_ASN1_write_bio((i2d_of_void *) am_i2d_PKCS7.functionPointer(), PEM_STRING_PKCS7, bioSignature.data(), signature.data(), nullptr, nullptr, 0, nullptr, nullptr))
            throw CryptoException("Could not write the PKCS#7 signature to the BIO buffer");

        char *data = 0;
        // long size = BIO_get_mem_data(bioSignature.data(), &data);
        long size = am_BIO_ctrl(bioSignature.data(), BIO_CTRL_INFO, 0, (char *) &data);
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
        OpenSslPointer<BIO> bioSignature(am_BIO_new_mem_buf((void *) signaturePkcs7.constData(), signaturePkcs7.size()));
        if (!bioSignature)
            throw CryptoException("Could not create BIO buffer for PKCS#7 data");

        // PKCS7 *PEM_read_bio_PKCS7(BIO *bp, PKCS7 **x, pem_password_cb *cb, void *u);
        OpenSslPointer<PKCS7> signature((PKCS7 *) am_PEM_ASN1_read_bio((d2i_of_void *) am_d2i_PKCS7.functionPointer(), PEM_STRING_PKCS7, bioSignature.data(), nullptr, nullptr, nullptr));
        if (!signature)
            throw CryptoException("Could not read PKCS#7 data from BIO buffer");

        OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf((void *) d->hash.constData(), d->hash.size()));
        if (!bioHash)
            throw CryptoException("Could not create BIO buffer for the hash");

        OpenSslPointer<X509_STORE> certChain(am_X509_STORE_new());
        if (!certChain)
            throw CryptoException("Could not create a X509 certificate store");

        foreach (const QByteArray &trustedCert, chainOfTrust) {
            OpenSslPointer<BIO> bioCert(am_BIO_new_mem_buf((void *) trustedCert.constData(), trustedCert.size()));
            if (!bioCert)
                throw CryptoException("Could not create BIO buffer for a certificate");

            // BIO_eof(b) == (int)BIO_ctrl(b,BIO_CTRL_EOF,0,NULL)
            while (!am_BIO_ctrl(bioCert.data(), BIO_CTRL_EOF, 0, nullptr)) {
                //OpenSslPointer<X509> cert(PEM_read_bio_X509(bioCert.data(), 0, 0, 0));
                OpenSslPointer<X509> cert((X509 *) am_PEM_ASN1_read_bio((d2i_of_void *) am_d2i_X509.functionPointer(), PEM_STRING_X509, bioCert.data(), nullptr, nullptr, nullptr));
                if (!cert)
                    throw CryptoException("Could not load a certificate from the chain of trust");
                if (!am_X509_STORE_add_cert(certChain.data(), cert.data()))
                    throw CryptoException("Could not add a certificate from the chain of trust to the certificate store");
                cert.take();
            }
        }

        // int PKCS7_verify(PKCS7 *p7, STACK_OF(X509) *certs, X509_STORE *store, BIO *indata, BIO *out, int flags);
        if (am_PKCS7_verify(signature.data(), nullptr, certChain.data(), bioHash.data(), nullptr, PKCS7_NOCHAIN) != 1) {
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
