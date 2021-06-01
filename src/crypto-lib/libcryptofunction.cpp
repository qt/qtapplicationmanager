/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QLibrary>
#include <QString>
#include <QSslSocket>

#include "libcryptofunction.h"

// we want at least openssl 1.0.1 or 1.1.0
#define AM_MINIMUM_OPENSSL10_VERSION 0x1000100fL
#define AM_MINIMUM_OPENSSL11_VERSION 0x1010000fL

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static
static AM_LIBCRYPTO_FUNCTION(SSLeay, unsigned long(*)(), 0);
static AM_LIBCRYPTO_FUNCTION(OPENSSL_add_all_algorithms_noconf, void(*)());
static AM_LIBCRYPTO_FUNCTION(ERR_load_crypto_strings, void(*)());

static AM_LIBCRYPTO_FUNCTION(OpenSSL_version_num, unsigned long(*)(), 0);
static AM_LIBCRYPTO_FUNCTION(OPENSSL_init_crypto, int(*)(uint64_t, void *), 0);

QLibrary *Cryptography::LibCryptoFunctionBase::s_library = nullptr;
bool Cryptography::LibCryptoFunctionBase::s_isOpenSSL11 = false;

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
            version = am_OpenSSL_version_num();  // 1.1
        else if (am_SSLeay.functionPointer())
            version = am_SSLeay(); // 1.0

        if (version > 0) {
            if (version >= AM_MINIMUM_OPENSSL11_VERSION) {
                s_isOpenSSL11 = true;
                return (am_OPENSSL_init_crypto(4 /*OPENSSL_INIT_ADD_ALL_CIPHERS*/
                                               | 8 /*OPENSSL_INIT_ADD_ALL_DIGESTS*/
                                               | 2 /*OPENSSL_INIT_LOAD_CRYPTO_STRINGS*/,
                                               nullptr) == 1);
            } else if (version >= AM_MINIMUM_OPENSSL10_VERSION) {
                am_OPENSSL_add_all_algorithms_noconf();
                am_ERR_load_crypto_strings();
                return true;
            } else {
                qCritical("Loaded libcrypto (%s), but the version is too old: 0x%08lx (minimum supported version is: 0x%08lx)",
                          qPrintable(s_library->fileName()), version, AM_MINIMUM_OPENSSL10_VERSION);
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
