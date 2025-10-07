// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <QLibrary>
#include <QString>
#include <QSslSocket>
#if defined(Q_OS_MACOS)
#  include <QVersionNumber>
#endif

#include "libcryptofunction.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static
// AXIVION DISABLE Qt-NonPodGlobalStatic

static QT_AM_LIBCRYPTO_FUNCTION(SSLeay, unsigned long(*)(), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_add_all_algorithms_noconf, void(*)());
static QT_AM_LIBCRYPTO_FUNCTION(OpenSSL_version_num, unsigned long(*)(), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_init_crypto, int(*)(uint64_t, void *), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OSSL_PROVIDER_load, void *(*)(void *, const char *), nullptr);

// macOS libressl support:
static QT_AM_LIBCRYPTO_FUNCTION(OpenSSL_version, const char *(*)(int), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(ASN1_STRING_data, char *(*)(const void *), nullptr);
static QT_AM_LIBCRYPTO_FUNCTION(ASN1_STRING_length, int (*)(const void *), 0);

// AXIVION ENABLE Qt-NonPodGlobalStatic

QLibrary *Cryptography::LibCryptoFunctionBase::s_library = nullptr;
bool Cryptography::LibCryptoFunctionBase::s_isMacOSLibreSSL = false;

bool Cryptography::LibCryptoFunctionBase::initialize()
{
    if (s_library)
        return true;

    static const std::pair<QString, int> libNameAndVersion =
#if defined(Q_OS_WINDOWS)
        { u"libeay32"_s, 3 };
#elif defined(Q_OS_MACOS)
        // Apple expects you to load a specific major version. Otherwise you will get a shim
        // that just prints "was loaded in an unsafe way" and exits your process.
        // Shlib version 46 corresponds to LibreSSL 3.3.6 and it is available from macOS 12 to 26.
        { u"/usr/lib/libcrypto"_s, 46 };
#else
        { u"crypto"_s, 3 };
#endif

    // Loading libcrypto is a mess, since distros are not creating links for libcrypto.so.1
    // anymore. In order to not duplicate a lot of bad hacks, we simply let QtNetwork do the
    // dirty work.
    if (!QSslSocket::supportsSsl())
        return false;

    s_library = new QLibrary(libNameAndVersion.first, libNameAndVersion.second);
    bool ok = s_library->load();
#if !defined(Q_OS_MACOS)
    if (!ok) {
        s_library->setFileNameAndVersion(libNameAndVersion.first, QString());
        ok = s_library->load();
    }
#endif
    if (ok) {
        unsigned long version = 0;

        if (am_OpenSSL_version_num.functionPointer())
            version = am_OpenSSL_version_num();  // 1.1 and 3.x
        else if (am_SSLeay.functionPointer())
            version = am_SSLeay(); // 1.0

        static const unsigned long MinimumOpenSslVersion = 0x3000000f;
        static const unsigned long LibreSslVersion       = 0x20000000; // libressl always reports 2.0.0

        if (version > 0) {
            if (version >= MinimumOpenSslVersion) {
                return (am_OPENSSL_init_crypto(4 /*OPENSSL_INIT_ADD_ALL_CIPHERS*/
                                                   | 8 /*OPENSSL_INIT_ADD_ALL_DIGESTS*/
                                                   | 2 /*OPENSSL_INIT_LOAD_CRYPTO_STRINGS*/,
                                               nullptr) == 1);
            } else if (version == LibreSslVersion) {
#if defined(Q_OS_MACOS)
                static const QVersionNumber MinimumLibresslVersion(3, 3, 6);
                QVersionNumber libresslVersion;

                if (auto *libresslVersionStr = am_OpenSSL_version(0 /*OPENSSL_VERSION*/)) {
                    if (qstrncmp(libresslVersionStr, "LibreSSL ", 9) == 0) {
                        libresslVersion = QVersionNumber::fromString(libresslVersionStr + 9);
                        if (libresslVersion >= MinimumLibresslVersion) {
                            s_isMacOSLibreSSL = true;
                            return (am_OPENSSL_init_crypto(4 /*OPENSSL_INIT_ADD_ALL_CIPHERS*/
                                                               | 8 /*OPENSSL_INIT_ADD_ALL_DIGESTS*/
                                                               | 2 /*OPENSSL_INIT_LOAD_CRYPTO_STRINGS*/,
                                                           nullptr) == 1);
                        }
                    }
                }
                qCritical("Loaded libcrypto (%s) from LibreSSL, but the version is too old: %s (minimum supported version is: %s)",
                          qPrintable(s_library->fileName()), qPrintable(libresslVersion.toString()),
                          qPrintable(MinimumLibresslVersion.toString()));
#else
                qCritical("Loaded libcrypto (%s) from LibreSSL, but this is only supported on macOS",
                          qPrintable(s_library->fileName()));
#endif
            } else {
                qCritical("Loaded libcrypto (%s), but the version is too old: 0x%08lx (minimum supported version is: 0x%08lx)",
                          qPrintable(s_library->fileName()), version, MinimumOpenSslVersion);
            }
        } else {
            qCritical("Could not get version information from libcrypto: neither of the symbols 'SSLeay' or 'OpenSSL_version_num' were found");
        }
        s_library->unload();
    } else {
        qCritical("Could not find a suitable libcrypto: %s", qPrintable(s_library->errorString()));
    }
    delete s_library;
    s_library = nullptr;
    return false;
}

Cryptography::LibCryptoFunctionBase::LibCryptoFunctionBase(const char *symbol)
    : m_symbol(symbol)
{ }

#if defined(Q_OS_MACOS)
static int libressl_ASN1_TIME_to_tm(const void *asn1Time, struct ::tm *tm)
{
    // LibreSSL's ASN1_TIME_to_tm is not exported as of version 3.3.6, so we provide a very
    // basic replacement that only supports the formats we actually see in certificates.

    Q_ASSERT(asn1Time);
    Q_ASSERT(tm);
    const char *data = am_ASN1_STRING_data(asn1Time);
    int len = am_ASN1_STRING_length(asn1Time);

    if ((len == 13) || (len == 15)) {
        QString format = (len == 13) ? u"yyMMddHHmmsst"_s : u"yyyyMMddHHmmsst"_s;
        if (auto dt = QDateTime::fromString(QString::fromLatin1(data, len), format, 1950); dt.isValid()) {
            ::memset(tm, 0, sizeof(::tm));
            tm->tm_year = dt.date().year() - 1900;
            tm->tm_mon = dt.date().month() - 1;
            tm->tm_mday = dt.date().day();
            tm->tm_hour = dt.time().hour();
            tm->tm_min = dt.time().minute();
            tm->tm_sec = dt.time().second();
            return 1;
        }
    }
    return 0;
}
#endif

void Cryptography::LibCryptoFunctionBase::resolve()
{
    if (!m_tried) {
        if (!s_library) {
            qCritical("Failed to resolve libcrypto symbol %s: library not loaded yet", m_symbol);
            return;
        }
        QByteArrayView symbolToResolve = m_symbol;
#if defined(Q_OS_MACOS)
        // This is a very bad hack that allows us to use macOS' libressl transparently
        if (s_isMacOSLibreSSL) {
            // The 'sk' (stack) functions are not renamed yet in this version
            if (symbolToResolve.startsWith("OPENSSL_sk_"))
                symbolToResolve.slice(8);
            // PKCS7_free crashes, so we simply leak the PKCS7 structures
            if (symbolToResolve == "PKCS7_free")
                m_functionPtr = qt_noop;
            // ASN1_TIME_to_tm is not exported, so we provide our own implementation
            if (symbolToResolve == "ASN1_TIME_to_tm")
                m_functionPtr = reinterpret_cast<void(*)()>(libressl_ASN1_TIME_to_tm);
        }
        if (!m_functionPtr)
#endif
        m_functionPtr = s_library->resolve(symbolToResolve.constData());
        if (!m_functionPtr)
            qCritical("Failed to resolve libcrypto symbol %s: %s", symbolToResolve.constData(), qPrintable(s_library->errorString()));
        m_tried = true;
    }
}

QT_END_NAMESPACE_AM
