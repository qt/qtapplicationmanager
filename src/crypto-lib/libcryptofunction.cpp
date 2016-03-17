/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QLibrary>
#include <QString>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "libcryptofunction.h"

// we want at least openssl 1.0.1
#define AM_MINIMUM_OPENSSL_VERSION 0x1000100fL

#if OPENSSL_VERSION_NUMBER < AM_MINIMUM_OPENSSL_VERSION
#  error "Your OpenSSL version is too old - the minimum supported version is 1.0.1"
#endif

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
        }
        s_library->unload();
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
