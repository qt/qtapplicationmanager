// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <memory>

#include "exception.h"
#include "libcryptofunction.h"
#include "signature_p.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static
// AXIVION DISABLE Qt-NonPodGlobalStatic

// dummy structures
struct ASN1_BIT_STRING;
struct ASN1_INTEGER;
struct ASN1_OBJECT;
struct ASN1_STRING;
struct ASN1_TIME;
struct BIGNUM;
struct BIO;
struct BIO_METHOD;
struct EVP_CIPHER;
struct EVP_MD;
struct EVP_MD;
struct EVP_PKEY;
struct GENERAL_NAME;
struct OPENSSL_STACK;
struct PKCS12;
struct PKCS7;
struct STACK_OF_GENERAL_NAME;
struct STACK_OF_X509;
struct X509;
struct X509_CRL;
struct X509_NAME;
struct X509_NAME_ENTRY;
struct X509_STORE;
struct X509_STORE_CTX;
using X509_STORE_CTX_verify_cb = int (*)(int, X509_STORE_CTX *);
struct d2i_of_void;
struct i2d_of_void;
using pem_password_cb = int (*)(char *, int, int, void *);

static QT_AM_LIBCRYPTO_FUNCTION(ASN1_BIT_STRING_get_bit, int(*)(ASN1_BIT_STRING *, int), 0);
static QT_AM_LIBCRYPTO_FUNCTION(ASN1_INTEGER_to_BN, BIGNUM *(*)(const ASN1_INTEGER *, BIGNUM *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(ASN1_STRING_to_UTF8, int (*)(unsigned char **, const ASN1_STRING *), -1);
static QT_AM_LIBCRYPTO_FUNCTION(ASN1_TIME_to_tm, int (*)(const ASN1_TIME *, struct tm *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(BIO_ctrl, long(*)(BIO *, int, long, void *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(BIO_free, int(*)(BIO *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(BIO_new, BIO *(*)(BIO_METHOD *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(BIO_new_mem_buf, BIO *(*)(const void *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(BIO_s_mem, BIO_METHOD *(*)(), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(BN_bn2hex, char *(*)(const BIGNUM *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(BN_free, void(*)(BIGNUM *));
static QT_AM_LIBCRYPTO_FUNCTION(CRYPTO_free, void(*)(void *, const char *, int));
static QT_AM_LIBCRYPTO_FUNCTION(ERR_clear_error, void(*)());
static QT_AM_LIBCRYPTO_FUNCTION(ERR_get_error_all, unsigned long(*)(const char **, int *, const char **, const char **, int *), 4|64 /*ERR_R_INTERNAL_ERROR*/);
static QT_AM_LIBCRYPTO_FUNCTION(ERR_reason_error_string, const char *(*)(unsigned long), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(EVP_PKEY_free, void(*)(EVP_PKEY *));
static QT_AM_LIBCRYPTO_FUNCTION(EVP_sha1, const EVP_MD *(*)(), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(EVP_sha256, const EVP_MD *(*)(), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(GENERAL_NAME_free, void (*)(GENERAL_NAME *));
static QT_AM_LIBCRYPTO_FUNCTION(GENERAL_NAME_get0_value, void *(*)(const GENERAL_NAME *, int *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OBJ_obj2txt, int(*)(char *, int, const ASN1_OBJECT *, int), -1);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_sk_num, int (*)(const OPENSSL_STACK *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_sk_pop_free, void (*)(OPENSSL_STACK *st, void (*func) (void *)));
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_sk_value, void *(*)(const OPENSSL_STACK *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PEM_ASN1_read_bio, void *(*)(d2i_of_void *, const char *, BIO *, void **, pem_password_cb, void *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PEM_read_bio_X509, X509 *(*)(BIO *, X509 **, pem_password_cb, void *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PEM_read_bio_X509_CRL, X509_CRL *(*)(BIO *, X509_CRL **, pem_password_cb, void *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS12_free, void(*)(PKCS12 *));
static QT_AM_LIBCRYPTO_FUNCTION(PKCS12_parse, int (*)(PKCS12 *, const char *, EVP_PKEY **, X509 **, STACK_OF_X509 **ca), 0);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_final, int (*)(PKCS7 *, BIO *, int), 0);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_free, void(*)(PKCS7 *));
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_get0_signers, STACK_OF_X509 * (*)(PKCS7 *, STACK_OF_X509 *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_sign, PKCS7 *(*)(X509 *, EVP_PKEY *, STACK_OF_X509 *, BIO *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_sign_add_signer, void *(*)(PKCS7 *, X509 *, EVP_PKEY *, const EVP_MD *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(PKCS7_verify, int (*)(PKCS7 *, STACK_OF_X509 *, X509_STORE *, BIO *, BIO *, int), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_CRL_free, void(*)(X509_CRL *));
static QT_AM_LIBCRYPTO_FUNCTION(X509_NAME_ENTRY_get_data, ASN1_STRING *(*)(const X509_NAME_ENTRY *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_NAME_ENTRY_get_object, ASN1_OBJECT *(*)(const X509_NAME_ENTRY *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_NAME_entry_count, int (*)(const X509_NAME *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_NAME_get_entry, X509_NAME_ENTRY *(*)(const X509_NAME *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_NAME_get_index_by_NID, int (*)(const X509_NAME *, int, int), -1);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_free, void(*)(X509_STORE_CTX *));
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_get_current_cert, X509 *(*)(X509_STORE_CTX *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_get_error, int (*)(X509_STORE_CTX *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_get0_chain, STACK_OF_X509 *(*)(X509_STORE_CTX *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_get0_store, X509_STORE *(*)(const X509_STORE_CTX *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_init, int(*)(X509_STORE_CTX *, X509_STORE *, X509 *, STACK_OF_X509 *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_CTX_new, X509_STORE_CTX *(*)(), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_add_cert, int (*)(X509_STORE *, X509 *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_add_crl, int (*)(X509_STORE *, X509_CRL *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_free, void(*)(X509_STORE *));
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_get_ex_data, void *(*)(const X509_STORE *, int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_new, X509_STORE *(*)(), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_set_ex_data, int(*)(X509_STORE *, int, void *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_set_flags, int(*)(X509_STORE *, unsigned long), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_STORE_set_verify_cb, void(*)(X509_STORE *, X509_STORE_CTX_verify_cb));
static QT_AM_LIBCRYPTO_FUNCTION(X509_digest, int *(*) (const X509 *, const EVP_MD *, unsigned char *, unsigned int *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_free, void(*)(X509 *));
static QT_AM_LIBCRYPTO_FUNCTION(X509_get0_notAfter, const ASN1_TIME *(*)(const X509 *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_get0_notBefore, const ASN1_TIME *(*)(const X509 *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_get0_serialNumber, ASN1_INTEGER *(*)(const X509 *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_get_ext_d2i, void * (*)(const X509 *, int, int *, int *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_get_issuer_name, X509_NAME *(*)(const X509 *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_get_subject_name, X509_NAME *(*)(const X509 *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(X509_up_ref, int (*)(X509 *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(X509_verify_cert_error_string, const char *(*)(long n), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(d2i_PKCS12, PKCS12 *(*)(PKCS12 **, const unsigned char **, long), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(d2i_PKCS7, PKCS7 *(*)(PKCS7 **, const unsigned char **, long), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(i2d_PKCS7, int (*)(const PKCS7 *, unsigned char **), -1);

// AXIVION ENABLE Qt-NonPodGlobalStatic

struct OpenSslDeleter {
    inline void operator()(BIGNUM *bn)
    { am_BN_free(bn); }
    inline void operator()(BIO *bio)
    { am_BIO_free(bio); }
    inline void operator()(EVP_PKEY *pkey)
    { am_EVP_PKEY_free(pkey); }
    inline void operator()(PKCS12 *pkcs12)
    { am_PKCS12_free(pkcs12); }
    inline void operator()(PKCS7 *pkcs7)
    { am_PKCS7_free(pkcs7); }
    inline void operator()(X509 *x509)
    { am_X509_free(x509); }
    inline void operator()(X509_CRL *x509crl)
    { am_X509_CRL_free(x509crl); }
    inline void operator()(X509_STORE *x509Store)
    { am_X509_STORE_free(x509Store); }
    inline void operator()(X509_STORE_CTX *x509StoreCtx)
    { am_X509_STORE_CTX_free(x509StoreCtx); }
    inline void operator()(STACK_OF_GENERAL_NAME *stackOfGeneralName) {
         if (auto fp = am_GENERAL_NAME_free.functionPointer()) {
             am_OPENSSL_sk_pop_free(reinterpret_cast<OPENSSL_STACK *>(stackOfGeneralName),
                                    reinterpret_cast<void (*)(void *)>(fp));
         }
    }
    inline void operator()(STACK_OF_X509 *stackOfX509) {
        if (auto fp = am_X509_free.functionPointer()) {
            am_OPENSSL_sk_pop_free(reinterpret_cast<OPENSSL_STACK *>(stackOfX509),
                                   reinterpret_cast<void (*)(void *)>(fp));
        }
    }
    inline void operator()(char *memory)
    { am_CRYPTO_free(memory, __FILE__, __LINE__); }
    inline void operator()(unsigned char *memory)
    { am_CRYPTO_free(memory, __FILE__, __LINE__); }
};

template <typename T> using OpenSslPointer = std::unique_ptr<T, OpenSslDeleter>;

class OpenSslException : public Exception  // clazy:exclude=copyable-polymorphic
{
public:
    OpenSslException(const char *errorString)
        : Exception(Error::Cryptography, errorString)
    {
        const char *data = nullptr;
        int flags = 0;
        // get the most recent error code together with any potential additional data
        unsigned long code = am_ERR_get_error_all(nullptr /*file*/, nullptr /*line*/,
                                                  nullptr /*func*/, &data, &flags);
        if (code) {
            const char *reason = am_ERR_reason_error_string(code);
            m_errorString += u": " + (reason ? QString::fromUtf8(reason) : QString::number(code, 16));
            if ((flags & 0x02 /*ERR_TXT_STRING*/) && data && *data)
                m_errorString += u" (" + QString::fromUtf8(data) + u')';
            am_ERR_clear_error(); // ignore all the other errors
        }
    }
};

class CertificateParser {
public:
    static Certificate parseX509(X509 *x509);

private:
    static QString parseASN1String(const ASN1_STRING *asn1);
    static QDateTime parseASN1Time(const ASN1_TIME *asn1);
    static QString parseASN1BigInteger(const ASN1_INTEGER *asn1);
    static QVariantMap parseX509Name(X509_NAME *name);
};

QString CertificateParser::parseASN1String(const ASN1_STRING *asn1)
{
    if (!asn1)
        throw Exception("cannot parse a null ASN1_STRING");

    unsigned char *utf8Ptr = nullptr;
    int utf8Len = am_ASN1_STRING_to_UTF8(&utf8Ptr, asn1);
    OpenSslPointer<unsigned char> utf8 { utf8Ptr }; // free utf8Ptr regardless of return value
    if (utf8Len < 0)
        throw OpenSslException("failed to convert ASN1_STRING to UTF8");

    return QString::fromUtf8(QByteArrayView(utf8Ptr, utf8Len));
}

QDateTime CertificateParser::parseASN1Time(const ASN1_TIME *asn1)
{
    if (!asn1)
        throw Exception("cannot parse a null ASN1_TIME");

    struct ::tm tm { };
    if (!am_ASN1_TIME_to_tm(asn1, &tm))
        throw OpenSslException("could not parse ASN1 time");

    return { QDate(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday),
            QTime(tm.tm_hour, tm.tm_min, tm.tm_sec), QTimeZone::UTC };
}

QString CertificateParser::parseASN1BigInteger(const ASN1_INTEGER *asn1)
{
    if (!asn1)
        throw Exception("cannot parse a null ASN1_INTEGER");

    OpenSslPointer<BIGNUM> bn { am_ASN1_INTEGER_to_BN(asn1, nullptr) };
    if (!bn)
        throw OpenSslException("could not parse ASN1 integer");
    OpenSslPointer<char> hex { am_BN_bn2hex(bn.get()) };
    if (!hex)
        throw OpenSslException("could not convert ASN1 integer into a hex string");

    return QString::fromLatin1(hex.get());
}

QVariantMap CertificateParser::parseX509Name(X509_NAME *name)
{
    if (!name)
        throw Exception("cannot parse a null X509_NAME");

    QVariantMap result;
    const int cnt = am_X509_NAME_entry_count(name);

    for (int i = 0; i < cnt; ++i) {
        X509_NAME_ENTRY *entry = am_X509_NAME_get_entry(name, i); // returns internal pointer
        if (!entry)
            throw OpenSslException("could not get X509_NAME_ENTRY from X509_NAME");
        auto *asn1Obj = am_X509_NAME_ENTRY_get_object(entry);
        if (!asn1Obj)
            throw OpenSslException("could not get X509_NAME_ENTRY object");
        auto *asn1Str = am_X509_NAME_ENTRY_get_data(entry);
        if (!asn1Str)
            throw OpenSslException("could not get X509_NAME_ENTRY data");

        std::array<char, 128> buffer { }; // the docs say "80 is plenty"
        int bufferLen = am_OBJ_obj2txt(buffer.data(), buffer.size() - 1, asn1Obj, 1 /*prefer oid*/);
        if (bufferLen <= 0)
            throw OpenSslException("could not get OID from ASN1_OBJECT");
        const QString oid = QString::fromLatin1(buffer.data(), -1);
        if (oid.isEmpty())
            throw Exception("OID of name entry is empty");

        const QString value = parseASN1String(asn1Str);

        SignaturePrivate::setDNByOid(result, oid, value);
    }
    return result;
}

Certificate CertificateParser::parseX509(X509 *x509)
{
    if (!x509)
        return { };

    Certificate info;
    // not named get0, but returns an internal pointer nonetheless
    info.m_subject = parseX509Name(am_X509_get_subject_name(x509));
    info.m_validityNotAfter = parseASN1Time(am_X509_get0_notAfter(x509));
    info.m_validityNotBefore = parseASN1Time(am_X509_get0_notBefore(x509));
    info.m_serialNumber = parseASN1BigInteger(am_X509_get0_serialNumber(x509));

    uint keyUsage = 0;
    if (auto kubs = static_cast<ASN1_BIT_STRING *>(am_X509_get_ext_d2i(x509, 83 /*NID_key_usage*/, nullptr, nullptr))) {
        for (int i = 0; i < 9; ++i) { // there are 9 bits defined: see Certificate::KeyUsage
            if (am_ASN1_BIT_STRING_get_bit(kubs, i))
                keyUsage |= (1u << i);
        }
    }
    info.m_keyUsages = Certificate::KeyUsages::fromInt(keyUsage);

    unsigned int sha256Len = 256 / 8;
    QByteArray sha256(sha256Len, '\0');
    if (!am_X509_digest(x509, am_EVP_sha256(), reinterpret_cast<unsigned char *>(sha256.data()), &sha256Len))
        throw OpenSslException("could not calculate SHA-256 fingerprint");
    info.m_fingerprint = sha256;

    QStringList subjectAlternativeNames;
    if (auto sans = static_cast<OPENSSL_STACK *>(am_X509_get_ext_d2i(x509, 85 /*NID_subject_alt_name*/, nullptr, nullptr))) {
        OpenSslPointer<STACK_OF_GENERAL_NAME> sansPtr { reinterpret_cast<STACK_OF_GENERAL_NAME *>(sans) };

        for (int i = 0; i < am_OPENSSL_sk_num(sans); ++i) {
            if (auto san = static_cast<GENERAL_NAME *>(am_OPENSSL_sk_value(sans, i))) {
                int type = 0;
                if (auto *dnsName = static_cast<ASN1_STRING *>(am_GENERAL_NAME_get0_value(san, &type))) {
                    if (type == 6 /*GEN_URI*/)
                        subjectAlternativeNames << parseASN1String(dnsName);
                }
            }
        }
    }
    info.m_subjectAlternativeNames = subjectAlternativeNames;

    return info;
}

QByteArray SignaturePrivate::create(const QByteArray &signingCertificatePkcs12,
                                    const QByteArray &signingCertificatePassword,
                                    const std::function<void(const Certificate &)> &checkCertificate)
{
    LibCryptoFunctionBase::initialize();

    auto pkcs12Data = reinterpret_cast<const unsigned char *>(signingCertificatePkcs12.constData());
    OpenSslPointer<PKCS12> pkcs12(am_d2i_PKCS12(nullptr, &pkcs12Data, signingCertificatePkcs12.size()));
    if (!pkcs12)
        throw OpenSslException("Could not read PKCS#12 data");

    EVP_PKEY *tempSignKey = nullptr;
    X509 *tempSignCert = nullptr;
    STACK_OF_X509 *tempCaCerts = nullptr;
    int parseOk = am_PKCS12_parse(pkcs12.get(), signingCertificatePassword.constData(), &tempSignKey, &tempSignCert, &tempCaCerts);
    OpenSslPointer<EVP_PKEY> signKey(tempSignKey);
    OpenSslPointer<X509> signCert(tempSignCert);
    OpenSslPointer<STACK_OF_X509> caCerts(tempCaCerts);

    if (!parseOk)
        throw OpenSslException("Could not parse PKCS#12 data");

    if (!signKey)
        throw OpenSslException("Could not find the private key within the PKCS#12 data");
    if (!signCert)
        throw OpenSslException("Could not find the certificate within the PKCS#12 data");

    if (checkCertificate)
        checkCertificate(CertificateParser::parseX509(signCert.get()));

    OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf(hash.constData(), hash.size()));
    if (!bioHash)
        throw OpenSslException("Could not create a BIO buffer for the hash");

    OpenSslPointer<PKCS7> signature(am_PKCS7_sign(nullptr, nullptr, caCerts.get(),
                                                  nullptr, 0x4000 /*PKCS7_PARTIAL*/ | 0x40 /*PKCS7_DETACHED*/));
    if (!signature)
        throw OpenSslException("Could not create the PKCS#7 signature");

    if (!am_PKCS7_sign_add_signer(signature.get(), signCert.get(), signKey.get(), am_EVP_sha256(), 0))
        throw OpenSslException("Could not add the signer to the PKCS#7 signature");

    if (!am_PKCS7_final(signature.get(), bioHash.get(), 0x80 /*PKCS7_BINARY*/))
        throw OpenSslException("Could not add the data to the PKCS#7 signature");

    unsigned char *signatureDataRaw = nullptr;
    int signatureSize = am_i2d_PKCS7(signature.get(), &signatureDataRaw);
    if ((signatureSize < 0) || !signatureDataRaw)
        throw OpenSslException("Could not write the PKCS#7 signature to memory");
    OpenSslPointer<unsigned char> signatureData { signatureDataRaw };

    return QByteArrayView(signatureDataRaw, signatureSize).toByteArray();
}

Signature::VerificationResult SignaturePrivate::verify(const QByteArray &signaturePkcs7,
                                                       const QByteArrayList &trustedCertsData)
{
    LibCryptoFunctionBase::initialize();

    QHash<X509 *, Signature::CertificateRole> x509ToRole;

    auto cleanup = qScopeGuard([&] {
        const QList<X509 *> keys = x509ToRole.keys();
        for (auto cert : keys)
            am_X509_free(cert);
    });

    auto signatureData = reinterpret_cast<const unsigned char *>(signaturePkcs7.constData());
    OpenSslPointer<PKCS7> signature(am_d2i_PKCS7(nullptr, &signatureData, signaturePkcs7.size()));
    if (!signature)
        throw OpenSslException("Could not read PKCS#7 data");

    OpenSslPointer<BIO> bioHash(am_BIO_new_mem_buf(hash.constData(), hash.size()));
    if (!bioHash)
        throw OpenSslException("Could not create BIO buffer for the hash");

    OpenSslPointer<X509_STORE> trustedStore(am_X509_STORE_new());
    if (!trustedStore)
        throw OpenSslException("Could not create a X509 certificate store");

    for (int i = 0; i < trustedCertsData.size(); ++i) {
        const QByteArray &trustedCertData = trustedCertsData.at(i);
        OpenSslPointer<BIO> trustedCertBio(am_BIO_new_mem_buf(trustedCertData.constData(), trustedCertData.size()));
        if (!trustedCertBio)
            throw OpenSslException("Could not create BIO buffer for a certificate");

        while (!am_BIO_ctrl(trustedCertBio.get(), 2 /*BIO_CTRL_EOF*/, 0, nullptr)) {
            OpenSslPointer<X509> trustedCert(am_PEM_read_bio_X509(trustedCertBio.get(), nullptr, nullptr, nullptr));
            if (!trustedCert)
                throw OpenSslException("Could not load a certificate from the trusted certificates");
            if (!am_X509_STORE_add_cert(trustedStore.get(), trustedCert.get()))
                throw OpenSslException("Could not add a certificate from the trusted certificates to the certificate store");

            if (auto it = requiredRoles.constFind(trustedCertData); it != requiredRoles.constEnd()) {
                // Keep this certificate for role checking - increase refcount
                if (!am_X509_up_ref(trustedCert.get()))
                    throw OpenSslException("Could not increase reference count on certificate");
                x509ToRole.insert(trustedCert.get(), it.value());
            }
            // OpenSslPointer will free trustedCert when it goes out of scope
        }
    }

    bool hasCRLs = false;
    for (const QByteArray &requiredCRL : std::as_const(requiredCRLs)) {
        OpenSslPointer<BIO> bioCRL(am_BIO_new_mem_buf(requiredCRL.constData(), requiredCRL.size()));
        if (!bioCRL)
            throw OpenSslException("Could not create BIO buffer for a CRL");

        while (!am_BIO_ctrl(bioCRL.get(), 2 /*BIO_CTRL_EOF*/, 0, nullptr)) {
            OpenSslPointer<X509_CRL> crl(am_PEM_read_bio_X509_CRL(bioCRL.get(), nullptr, nullptr, nullptr));
            if (!crl)
                throw OpenSslException("Could not load a CRL");
            if (!am_X509_STORE_add_crl(trustedStore.get(), crl.get()))
                throw OpenSslException("Could not add a CRL to the certificate store");
            hasCRLs = true;
        }
    }

    unsigned long verificationFlags = 0x20 /*X509_V_FLAG_X509_STRICT*/;
    if (hasCRLs)
        verificationFlags |= (0x04 /*X509_V_FLAG_CRL_CHECK*/ | 0x08 /*X509_V_FLAG_CRL_CHECK_ALL*/);
    if (!am_X509_STORE_set_flags(trustedStore.get(), verificationFlags))
        throw OpenSslException("Could not set verification flags on the certificate store");

    // the ex_data mechanism is a way to "capture" variables for the verification callback
    QString verificationError;
    if (!am_X509_STORE_set_ex_data(trustedStore.get(), 0, &verificationError))
        throw OpenSslException("Could not set extended data on the certificate store");

    QList<X509 *> verificationChain;
    if (!am_X509_STORE_set_ex_data(trustedStore.get(), 1, &verificationChain))
        throw OpenSslException("Could not set extended data for verification chain on the certificate store");

    am_X509_STORE_set_verify_cb(trustedStore.get(), [](int ok, X509_STORE_CTX *ctx) -> int {
        // Capture the verification chain on success
        if (ok) {
            if (void *exdata = am_X509_STORE_get_ex_data(am_X509_STORE_CTX_get0_store(ctx), 1)) {
                auto *verificationChain = static_cast<QList<X509 *> *>(exdata);
                if (X509 *cert = am_X509_STORE_CTX_get_current_cert(ctx)) {
                    am_X509_up_ref(cert);
                    verificationChain->prepend(cert);
                } else {
                    return 0;
                }
            } else {
                return 0;
            }
        }

        // Capture verification errors
        if (int n = am_X509_STORE_CTX_get_error(ctx)) {
            if (void *exdata = am_X509_STORE_get_ex_data(am_X509_STORE_CTX_get0_store(ctx), 0)) {
                if (const char *str = am_X509_verify_cert_error_string(n))
                    *static_cast<QString *>(exdata) = QString::fromUtf8(str);
            }
        }
        return ok;
    });

    if (am_PKCS7_verify(signature.get(), nullptr, trustedStore.get(), bioHash.get(), nullptr,
                        0x10000 /*PKCS7_NO_DUAL_CONTENT*/ | 0x8 /*PKCS7_NOCHAIN*/) == 0) {
        if (!verificationError.isEmpty())
            throw Exception("Failed to verify signature: %1").arg(verificationError);
        else
            throw OpenSslException("Failed to verify signature");
    }

    OpenSslPointer<STACK_OF_X509> signers { am_PKCS7_get0_signers(signature.get(), nullptr, 0x08 /*PKCS7_NOCHAIN*/) };
    if (!signers || am_OPENSSL_sk_num(reinterpret_cast<OPENSSL_STACK *>(signers.get())) != 1)
        throw Exception("The PKCS#7 signature does not contain exactly one signer");

    // Use the captured verification chain
    if (verificationChain.isEmpty())
        throw Exception("Verification chain was not captured during certificate verification");

    // The chain contains: [0]=signer, [1..n-1]=intermediates, [n-1]=root
    if (verificationChain.size() < 2)
        throw Exception("Expected a verification chain with at least a signer and an issuer certificate");

    // Parse all certificates in the verification chain
    // The chain contains: [0]=signer, [1..n-1]=intermediates, [n-1]=root
    QList<Certificate> chain;

    for (qsizetype i = 0; i < verificationChain.size(); ++i) {
        OpenSslPointer<X509> chainCert { verificationChain.at(i) }; // takes ownership (we called X509_up_ref earlier)
        try {
            const auto c = CertificateParser::parseX509(chainCert.get());
            if ((i >= 1) && !requiredRoles.isEmpty()) {
                // Find the matching trusted certificate by comparing X509 pointers
                auto it = x509ToRole.constFind(chainCert.get());
                if (it == x509ToRole.constEnd())
                    throw Exception("Certificate at position %1 in the verification chain is not trusted").arg(i);
                verifyCertificateRole(c.subjectAsString(), i, verificationChain.size(), it.value());
            }
            chain.append(c);
        } catch (const Exception &e) {
            throw Exception("Could not parse certificate at position %1 in the verification chain: %2")
                .arg(i).arg(e.errorString());
        }
    }
    return { chain };
}

QT_END_NAMESPACE_AM
