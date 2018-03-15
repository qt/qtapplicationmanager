/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include "exception.h"
#include "cryptography.h"
#include "libcryptofunction.h"
#include "signature_p.h"

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static

// dummy structures
struct BIO;
struct BIO_METHOD;
struct PKCS7;
struct PKCS12;
struct EVP_PKEY;
struct EVP_CIPHER;
struct X509;
struct X509_STORE;
struct STACK_OF_X509;
struct i2d_of_void;
struct d2i_of_void;
typedef int pem_password_cb (char *, int, int, void *);

// deleter
static AM_LIBCRYPTO_FUNCTION(X509_free, void(*)(X509 *));
static AM_LIBCRYPTO_FUNCTION(BIO_free, int(*)(BIO *), 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_free, void(*)(PKCS7 *));
static AM_LIBCRYPTO_FUNCTION(EVP_PKEY_free, void(*)(EVP_PKEY *));
static AM_LIBCRYPTO_FUNCTION(PKCS12_free, void(*)(PKCS12 *));
static AM_LIBCRYPTO_FUNCTION(X509_STORE_free, void(*)(X509_STORE *));
static AM_LIBCRYPTO_FUNCTION(sk_pop_free, void(*)(STACK_OF_X509 *, void(*)(void *)));         // 1.0
static AM_LIBCRYPTO_FUNCTION(OPENSSL_sk_pop_free, void(*)(STACK_OF_X509 *, void(*)(void *))); // 1.1

// error handling
static AM_LIBCRYPTO_FUNCTION(ERR_get_error, unsigned long(*)(), 4|64 /*ERR_R_INTERNAL_ERROR*/);

// create
static AM_LIBCRYPTO_FUNCTION(BIO_ctrl, long(*)(BIO *, int, long, void *), 0);
static AM_LIBCRYPTO_FUNCTION(d2i_PKCS12_bio, PKCS12 *(*)(BIO *, PKCS12 **), nullptr);
static AM_LIBCRYPTO_FUNCTION(PKCS12_parse, int (*)(PKCS12 *, const char *, EVP_PKEY **, X509 **, STACK_OF_X509 **ca), 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_sign, PKCS7 *(*)(X509 *, EVP_PKEY *, STACK_OF_X509 *, BIO *, int), nullptr);
static AM_LIBCRYPTO_FUNCTION(BIO_new, BIO *(*)(BIO_METHOD *), nullptr);
static AM_LIBCRYPTO_FUNCTION(BIO_s_mem, BIO_METHOD *(*)(), nullptr);
static AM_LIBCRYPTO_FUNCTION(PEM_ASN1_write_bio, int (*)(i2d_of_void *, const char *, BIO *, void *, const EVP_CIPHER *, unsigned char *, int, pem_password_cb *, void *), 0);
static AM_LIBCRYPTO_FUNCTION(i2d_PKCS7_bio, int (*)(BIO *, PKCS7 *), 0);
static AM_LIBCRYPTO_FUNCTION(d2i_PKCS7_bio, PKCS7 *(*)(BIO *, PKCS7 **), nullptr);

// verify
static AM_LIBCRYPTO_FUNCTION(BIO_new_mem_buf, BIO *(*)(const void *, int), nullptr);
static AM_LIBCRYPTO_FUNCTION(PEM_ASN1_read_bio, void *(*)(d2i_of_void *, const char *, BIO *, void **, pem_password_cb *, void *), nullptr);
static AM_LIBCRYPTO_FUNCTION(d2i_X509, X509 *(*)(X509 **, const unsigned char **, long), nullptr);     // 1.0
static AM_LIBCRYPTO_FUNCTION(d2i_X509_AUX, X509 *(*)(X509 **, const unsigned char **, long), nullptr); // 1.1
static AM_LIBCRYPTO_FUNCTION(X509_STORE_new, X509_STORE *(*)(), nullptr);
static AM_LIBCRYPTO_FUNCTION(X509_STORE_add_cert, int (*)(X509_STORE *, X509 *), 0);
static AM_LIBCRYPTO_FUNCTION(PKCS7_verify, int (*)(PKCS7 *, STACK_OF_X509 *, X509_STORE *, BIO *, BIO *, int), 0);

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
    static inline void cleanup(STACK_OF_X509 *stackOfX509)
    { (Cryptography::LibCryptoFunctionBase::isOpenSSL11() ? am_OPENSSL_sk_pop_free
                                                          : am_sk_pop_free)
                (stackOfX509, (void(*)(void *)) am_X509_free.functionPointer()); }
};

template <typename T> using OpenSslPointer = QScopedPointer<T, OpenSslDeleter>;


class OpenSslException : public Exception
{
public:
    OpenSslException(const char *errorString)
        : Exception(Error::Cryptography)
    {
        m_errorString = Cryptography::errorString(am_ERR_get_error(), errorString);
    }
};


QByteArray SignaturePrivate::create(const QByteArray &signingCertificatePkcs12,
                                    const QByteArray &signingCertificatePassword) Q_DECL_NOEXCEPT_EXPR(false)
{
    // Although OpenSSL could, the OSX Security Framework cannot process empty detached data
    if (hash.isEmpty())
        throw OpenSslException("cannot sign an empty hash value");

    OpenSslPointer<BIO> bioPkcs12(am_BIO_new_mem_buf((void *) signingCertificatePkcs12.constData(), signingCertificatePkcs12.size()));
    if (!bioPkcs12)
        throw OpenSslException("Could not create BIO buffer for PKCS#12 data");

    //PKCS12 *d2i_PKCS12_bio(BIO *bp, PKCS12 **p12);
    OpenSslPointer<PKCS12> pkcs12(am_d2i_PKCS12_bio(bioPkcs12.data(), nullptr));
    if (!pkcs12)
        throw OpenSslException("Could not read PKCS#12 data from BIO buffer");

    //int PKCS12_parse(PKCS12 *p12, const char *pass, EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca);
    EVP_PKEY *tempSignKey = nullptr;
    X509 *tempSignCert = nullptr;
    STACK_OF_X509 *tempCaCerts = nullptr;
    int parseOk = am_PKCS12_parse(pkcs12.data(), signingCertificatePassword.constData(), &tempSignKey, &tempSignCert, &tempCaCerts);
    OpenSslPointer<EVP_PKEY> signKey(tempSignKey);
    OpenSslPointer<X509> signCert(tempSignCert);
    OpenSslPointer<STACK_OF_X509> caCerts(tempCaCerts);

    if (!parseOk)
        throw OpenSslException("Could not parse PKCS#12 data");

    if (!signKey)
        throw OpenSslException("Could not find the private key within the PKCS#12 data");
    if (!signCert)
        throw OpenSslException("Could not find the certificate within the PKCS#12 data");

    OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf((void *) hash.constData(), hash.size()));
    if (!bioHash)
        throw OpenSslException("Could not create a BIO buffer for the hash");

    //PKCS7 *PKCS7_sign(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs, BIO *data, int flags);
    OpenSslPointer<PKCS7> signature(am_PKCS7_sign(signCert.data(), signKey.data(), caCerts.data(),
                                                  bioHash.data(), 0x40 /*PKCS7_DETACHED*/));
    if (!signature)
        throw OpenSslException("Could not create the PKCS#7 signature");

    OpenSslPointer<BIO> bioSignature(am_BIO_new(am_BIO_s_mem()));
    if (!bioSignature)
        throw OpenSslException("Could not create a BIO buffer for the PKCS#7 signature");

    // int PEM_write_bio_PKCS7(BIO *bp, PKCS7 *x);
    //if (!am_PEM_ASN1_write_bio((i2d_of_void *) am_i2d_PKCS7.functionPointer(), PEM_STRING_PKCS7, bioSignature.data(), signature.data(), nullptr, nullptr, 0, nullptr, nullptr))
    if (!am_i2d_PKCS7_bio(bioSignature.data(), signature.data()))
        throw OpenSslException("Could not write the PKCS#7 signature to the BIO buffer");

    char *data = nullptr;
    // long size = BIO_get_mem_data(bioSignature.data(), &data);
    long size = am_BIO_ctrl(bioSignature.data(), 3 /*BIO_CTRL_INFO*/, 0, (char *) &data);
    if (size <= 0 || !data)
        throw OpenSslException("The BIO buffer for the PKCS#7 signature is invalid");

    return QByteArray(data, size);
}

bool SignaturePrivate::verify(const QByteArray &signaturePkcs7,
                              const QList<QByteArray> &chainOfTrust) Q_DECL_NOEXCEPT_EXPR(false)
{
    OpenSslPointer<BIO> bioSignature(am_BIO_new_mem_buf((void *) signaturePkcs7.constData(), signaturePkcs7.size()));
    if (!bioSignature)
        throw OpenSslException("Could not create BIO buffer for PKCS#7 data");

    // PKCS7 *PEM_read_bio_PKCS7(BIO *bp, PKCS7 **x, pem_password_cb *cb, void *u);
    //OpenSslPointer<PKCS7> signature((PKCS7 *) am_PEM_ASN1_read_bio((d2i_of_void *) am_d2i_PKCS7.functionPointer(), PEM_STRING_PKCS7, bioSignature.data(), nullptr, nullptr, nullptr));
    OpenSslPointer<PKCS7> signature((PKCS7 *) am_d2i_PKCS7_bio(bioSignature.data(), nullptr));
    if (!signature)
        throw OpenSslException("Could not read PKCS#7 data from BIO buffer");

    OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf((void *) hash.constData(), hash.size()));
    if (!bioHash)
        throw OpenSslException("Could not create BIO buffer for the hash");

    OpenSslPointer<X509_STORE> certChain(am_X509_STORE_new());
    if (!certChain)
        throw OpenSslException("Could not create a X509 certificate store");

    for (const QByteArray &trustedCert : chainOfTrust) {
        OpenSslPointer<BIO> bioCert(am_BIO_new_mem_buf((void *) trustedCert.constData(), trustedCert.size()));
        if (!bioCert)
            throw OpenSslException("Could not create BIO buffer for a certificate");

        // BIO_eof(b) == (int)BIO_ctrl(b,BIO_CTRL_EOF,0,NULL)
        while (!am_BIO_ctrl(bioCert.data(), 2 /*BIO_CTRL_EOF*/, 0, nullptr)) {
            //OpenSslPointer<X509> cert(PEM_read_bio_X509(bioCert.data(), 0, 0, 0));
            OpenSslPointer<X509> cert((X509 *) am_PEM_ASN1_read_bio((d2i_of_void *) (Cryptography::LibCryptoFunctionBase::isOpenSSL11() ? am_d2i_X509_AUX : am_d2i_X509).functionPointer(),
                                                                    "CERTIFICATE" /*PEM_STRING_X509*/, bioCert.data(),
                                                                    nullptr, nullptr, nullptr));
            if (!cert)
                throw OpenSslException("Could not load a certificate from the chain of trust");
            if (!am_X509_STORE_add_cert(certChain.data(), cert.data()))
                throw OpenSslException("Could not add a certificate from the chain of trust to the certificate store");
            // X509 certs are ref-counted, so we need to "free" the one we got via PEM_read_bio
        }
    }

    // int PKCS7_verify(PKCS7 *p7, STACK_OF(X509) *certs, X509_STORE *store, BIO *indata, BIO *out, int flags);
    if (am_PKCS7_verify(signature.data(), nullptr, certChain.data(), bioHash.data(), nullptr, 0x8 /*PKCS7_NOCHAIN*/) != 1) {
        bool failed = (am_ERR_get_error() != 0);
        if (failed)
            throw OpenSslException("Failed to verify signature");
        return false;
    } else {
        return true;
    }
}

QT_END_NAMESPACE_AM
