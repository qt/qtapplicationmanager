// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <QLibrary>
#include <QString>
#include <QSslSocket>

#include "libcryptofunction.h"

#define QT_AM_MINIMUM_OPENSSL30_VERSION 0x3000000fL

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static
// AXIVION DISABLE Qt-NonPodGlobalStatic

static QT_AM_LIBCRYPTO_FUNCTION(SSLeay, unsigned long(*)(), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_add_all_algorithms_noconf, void(*)());

static QT_AM_LIBCRYPTO_FUNCTION(OpenSSL_version_num, unsigned long(*)(), 0);
static QT_AM_LIBCRYPTO_FUNCTION(OPENSSL_init_crypto, int(*)(uint64_t, void *), 0);

struct OSSL_PROVIDER;
struct OSSL_LIB_CTX;
static QT_AM_LIBCRYPTO_FUNCTION(OSSL_PROVIDER_load, OSSL_PROVIDER *(*)(OSSL_LIB_CTX *, const char *), nullptr);

// AXIVION ENABLE Qt-NonPodGlobalStatic

QLibrary *Cryptography::LibCryptoFunctionBase::s_library = nullptr;

bool Cryptography::LibCryptoFunctionBase::initialize()
{
    if (s_library)
        return true;

    const char *libname =
#ifdef Q_OS_WIN32
            "libeay32";
#else
            "crypto";
#endif

    // Loading libcrypto is a mess, since distros are not creating links for libcrypto.so.1
    // anymore. In order to not duplicate a lot of bad hacks, we simply let QtNetwork do the
    // dirty work.
    if (!QSslSocket::supportsSsl())
        return false;

    s_library = new QLibrary(QString::fromLatin1(libname), 1);
    bool ok = s_library->load();
    if (!ok) {
        s_library->setFileNameAndVersion(QString::fromLatin1(libname), QString());
        ok = s_library->load();
    }
    if (ok) {
        unsigned long version = 0;

        if (am_OpenSSL_version_num.functionPointer())
            version = am_OpenSSL_version_num();  // 1.1 and 3.x
        else if (am_SSLeay.functionPointer())
            version = am_SSLeay(); // 1.0

        if (version > 0) {
            if (version >= QT_AM_MINIMUM_OPENSSL30_VERSION) {
                return (am_OPENSSL_init_crypto(4 /*OPENSSL_INIT_ADD_ALL_CIPHERS*/
                                               | 8 /*OPENSSL_INIT_ADD_ALL_DIGESTS*/
                                               | 2 /*OPENSSL_INIT_LOAD_CRYPTO_STRINGS*/,
                                               nullptr) == 1);
            } else {
                qCritical("Loaded libcrypto (%s), but the version is too old: 0x%08lx (minimum supported version is: 0x%08lx)",
                          qPrintable(s_library->fileName()), version, QT_AM_MINIMUM_OPENSSL30_VERSION);
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

void Cryptography::LibCryptoFunctionBase::resolve()
{
    if (!m_tried) {
        if (!s_library) {
            qCritical("Failed to resolve libcrypto symbol %s: library not loaded yet", m_symbol);
            return;
        }
        m_functionPtr = s_library->resolve(m_symbol);
        if (!m_functionPtr)
            qCritical("Failed to resolve libcrypto symbol %s: %s", m_symbol, qPrintable(s_library->errorString()));
        m_tried = true;
    }
}

QT_END_NAMESPACE_AM
