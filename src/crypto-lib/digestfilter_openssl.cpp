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

#include "cryptography.h"
#include "libcryptofunction.h"
#include "digestfilter_p.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

QT_BEGIN_NAMESPACE_AM

// clazy:excludeall=non-pod-global-static

// EVP
static AM_LIBCRYPTO_FUNCTION(EVP_sha512, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_sha256, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_sha1, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_MD_CTX_create, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_DigestInit_ex, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_MD_CTX_destroy);
static AM_LIBCRYPTO_FUNCTION(EVP_DigestUpdate, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_DigestFinal, 0);

// HMAC
static AM_LIBCRYPTO_FUNCTION(HMAC_Init_ex, 0);
static AM_LIBCRYPTO_FUNCTION(HMAC_CTX_cleanup);
static AM_LIBCRYPTO_FUNCTION(HMAC_Update, 0);
static AM_LIBCRYPTO_FUNCTION(HMAC_Final, 0);

// error handling
static AM_LIBCRYPTO_FUNCTION(ERR_get_error, ERR_R_INTERNAL_ERROR);
static AM_LIBCRYPTO_FUNCTION(ERR_put_error);


bool DigestFilterPrivate::init()
{
    const EVP_MD *digest = nullptr;
    switch (type) {
    case DigestFilter::Sha512: digest = am_EVP_sha512(); break;
    case DigestFilter::Sha256: digest = am_EVP_sha256(); break;
    case DigestFilter::Sha1  : digest = am_EVP_sha1(); break;
    default                  : Q_ASSERT(false); return false;
    }

    try {
        if (isHmac) {
            // Although OpenSSL could, WinCrypt cannot cope with empty keys
            if (key.isEmpty()) {
                am_ERR_put_error(ERR_LIB_CRYPTO, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);
                throw "The HMAC key cannot be empty";
            }

            // the HMAC_CTX structure changed between 0.9 and 1.0, so we need to reserve a
            // little bit of space to accommodate compiling against 0.9 and running against 1.0
            // it grew by 3*2*sizeof(void*), but we want to be on the safe side in the future:
            // (and of course there is neither an alloc() or a size() function in the lib)
            hmacCtx = (HMAC_CTX *) malloc(sizeof(HMAC_CTX) + 128);
            if (!hmacCtx) { // inject OOM error
                // void ERR_put_error(int lib, int func, int reason, const char *file, int line);
                am_ERR_put_error(ERR_LIB_CRYPTO, 0, ERR_R_MALLOC_FAILURE, __FILE__, __LINE__);
                throw "Failed to allocate HMAC context";
            }
            memset(hmacCtx, 0, sizeof(HMAC_CTX) + 128);
            if (!am_HMAC_Init_ex(hmacCtx, key.constData(), key.size(), digest, nullptr))
                throw "Failed to initialize HMAC context";
        } else {
            mdCtx = am_EVP_MD_CTX_create();
            if (!mdCtx)
                throw "Failed to create MD context";
            if (!am_EVP_DigestInit_ex(mdCtx, digest, nullptr))
                throw "Failed to initialize MD context";
        }

        return true;
    } catch (const char *err) {
        errorString = Cryptography::errorString(am_ERR_get_error(), err);
        cleanup();
        return false;
    }
}

void DigestFilterPrivate::cleanup()
{
    if (mdCtx) {
        am_EVP_MD_CTX_destroy(mdCtx);
        mdCtx = nullptr;
    }
    if (hmacCtx) {
        am_HMAC_CTX_cleanup(hmacCtx);
        free(hmacCtx);
        hmacCtx = nullptr;
    }
}

qint64 DigestFilterPrivate::process(const char *data, unsigned int size)
{
    if (isHmac ? am_HMAC_Update(hmacCtx, (const unsigned char *) data, size)
               : am_EVP_DigestUpdate(mdCtx, data, size)) {
        return 0;
    } else {
        return am_ERR_get_error();
    }
}

qint64 DigestFilterPrivate::finish(char *result, unsigned int *size)
{
    if (isHmac ? am_HMAC_Final(hmacCtx, (unsigned char *) result, size)
            : am_EVP_DigestFinal(mdCtx, (unsigned char *) result, size)) {
        return 0;
    } else {
        return am_ERR_get_error();
    }
}

QT_END_NAMESPACE_AM
