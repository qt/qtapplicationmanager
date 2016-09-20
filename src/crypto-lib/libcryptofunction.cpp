/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QLibrary>
#include <QString>
#include <QSslSocket>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "libcryptofunction.h"

// we want at least openssl 1.0.1
#define AM_MINIMUM_OPENSSL_VERSION 0x1000100fL

#if OPENSSL_VERSION_NUMBER < AM_MINIMUM_OPENSSL_VERSION
#  error "Your OpenSSL version is too old - the minimum supported version is 1.0.1"
#endif

AM_BEGIN_NAMESPACE

// clazy:excludeall=non-pod-global-static
static AM_LIBCRYPTO_FUNCTION(SSLeay, 0);
static AM_LIBCRYPTO_FUNCTION(OPENSSL_add_all_algorithms_noconf);
static AM_LIBCRYPTO_FUNCTION(ERR_load_crypto_strings);


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
    QSslSocket::supportsSsl();

    s_library = new QLibrary(libname, 1);
    bool ok = s_library->load();
    if (!ok) {
        s_library->setFileNameAndVersion(libname, QString());
        ok = s_library->load();
    }
    if (ok) {
        if (am_SSLeay.functionPointer()) {
            auto version = am_SSLeay();
            if (version >= AM_MINIMUM_OPENSSL_VERSION) {
                am_OPENSSL_add_all_algorithms_noconf();
                am_ERR_load_crypto_strings();
                return true;
            } else {
                qCritical("Loaded libcrypto (%s), but the version is too old: 0x%08lx (minimum supported version is: 0x%08lx)",
                          qPrintable(s_library->fileName()), version, AM_MINIMUM_OPENSSL_VERSION);
            }
        } else {
            qCritical("Could not get version information from libcrypto: symbol 'SSLeay' was not found");
        }
        s_library->unload();
    } else {
        qCritical("Could not find a suitable libcrypto: %s", qPrintable(s_library->errorString()));
    }
    delete s_library;
    s_library = 0;
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

AM_END_NAMESPACE
