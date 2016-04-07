/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QtCore/private/qcore_mac_p.h>

#include "cryptography.h"
#include "digestfilter_p.h"

#include <CommonCrypto/CommonHMAC.h>


bool DigestFilterPrivate::init()
{
    if (isHmac) {
        // Although CommonCrypto could, WinCrypt cannot cope with empty keys
        if (key.isEmpty()) {
            errorString = QLatin1String("The HMAC key cannot be empty");
            return false;
        }

        CCHmacAlgorithm digest = 0;
        switch (type) {
        case DigestFilter::Sha512: digest = kCCHmacAlgSHA512; break;
        case DigestFilter::Sha256: digest = kCCHmacAlgSHA256; break;
        case DigestFilter::Sha1  : digest = kCCHmacAlgSHA1; break;
        default                  : Q_ASSERT(false); return false;
        }

        hmacCtx = (CCHmacContext *) malloc(sizeof(CCHmacContext));
        CCHmacInit(hmacCtx, digest, key.constData(), key.size());
    } else {
        switch (type) {
        case DigestFilter::Sha512:
            sha512Ctx = (CC_SHA512_CTX *) malloc(sizeof(CC_SHA512_CTX));
            CC_SHA512_Init(sha512Ctx);
            break;
        case DigestFilter::Sha256:
            sha256Ctx = (CC_SHA256_CTX *) malloc(sizeof(CC_SHA256_CTX));
            CC_SHA256_Init(sha256Ctx);
            break;
        case DigestFilter::Sha1:
            sha1Ctx = (CC_SHA1_CTX *) malloc(sizeof(CC_SHA1_CTX));
            CC_SHA1_Init(sha1Ctx);
            break;
        default:
            Q_ASSERT(false);
            return false;
        }
    }
    return true;
}

void DigestFilterPrivate::cleanup()
{
    if (freePtr) {
        free(freePtr);
        freePtr = nullptr;
    }
}

qint64 DigestFilterPrivate::process(const char *data, unsigned int size)
{
    if (isHmac) {
        CCHmacUpdate(hmacCtx, data, size);
    } else {
        switch (type) {
        case DigestFilter::Sha512:
            CC_SHA512_Update(sha512Ctx, data, size);
            break;
        case DigestFilter::Sha256:
            CC_SHA256_Update(sha256Ctx, data, size);
            break;
        case DigestFilter::Sha1:
            CC_SHA1_Update(sha1Ctx, data, size);
            break;
        default:
            Q_ASSERT(false);
            break;
        }
    }
    return 0;
}

qint64 DigestFilterPrivate::finish(char *result, unsigned int *size)
{
    *size = q->size();

    if (isHmac) {
        CCHmacFinal(hmacCtx, result);
    } else {
        switch (type) {
        case DigestFilter::Sha512:
            CC_SHA512_Final((unsigned char *) result, sha512Ctx);
            break;
        case DigestFilter::Sha256:
            CC_SHA256_Final((unsigned char *) result, sha256Ctx);
            break;
        case DigestFilter::Sha1:
            CC_SHA1_Final((unsigned char *) result, sha1Ctx);
            break;
        default:
            Q_ASSERT(false);
            break;
        }
    }
    return 0;
}
