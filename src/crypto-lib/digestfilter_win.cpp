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

#include <QVarLengthArray>

#include "cryptography.h"
#include "digestfilter_p.h"

#include <windows.h>
#include <wincrypt.h>

QT_BEGIN_NAMESPACE_AM

bool DigestFilterPrivate::init()
{
    ALG_ID digest = 0;
    switch (type) {
    case DigestFilter::Sha512: digest = CALG_SHA_512; break;
    case DigestFilter::Sha256: digest = CALG_SHA_256; break;
    case DigestFilter::Sha1  : digest = CALG_SHA1; break;
    default                  : Q_ASSERT(false); return false;
    }

    try {
        if (!CryptAcquireContextW(&cryptProvider, 0, 0, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            throw "Failed to acquire crypto context";

        if (!isHmac) {
            if (!CryptCreateHash(cryptProvider, digest, 0, 0, &cryptHash))
                throw "Failed to create hash";

            return true;
        } else { // isHmac
            struct KeyBlob {
                BLOBHEADER header;
                DWORD keySize;
            } *keyBlob;
            QVarLengthArray<BYTE> buffer(sizeof(KeyBlob) + key.size());
            keyBlob = (KeyBlob *) buffer.data();
            keyBlob->header.bType = PLAINTEXTKEYBLOB;
            keyBlob->header.bVersion = CUR_BLOB_VERSION;
            keyBlob->header.reserved = 0;
            keyBlob->header.aiKeyAlg = CALG_RC2;
            keyBlob->keySize = key.size();
            memcpy(keyBlob + 1, key.constData(), key.size());

            if (!CryptImportKey(cryptProvider, buffer.constData(), buffer.size(), 0, CRYPT_IPSEC_HMAC_KEY, &cryptKey))
                throw "Failed to create HMAC key";
            if (!CryptCreateHash(cryptProvider, CALG_HMAC, cryptKey, 0, &cryptHash))
                throw "Failed to create hash";
            HMAC_INFO hmacInfo;
            memset(&hmacInfo, 0, sizeof(HMAC_INFO));
            hmacInfo.HashAlgid = digest;

            if (!CryptSetHashParam(cryptHash, HP_HMAC_INFO, (const BYTE *) &hmacInfo, 0))
                throw "Failed to set HMAC info";

            return true;
        }
    } catch (const char *err) {
        errorString = Cryptography::errorString(GetLastError(), err);
        cleanup();
        return false;
    }
}

void DigestFilterPrivate::cleanup()
{
    if (cryptHash) {
        CryptDestroyHash(cryptHash);
        cryptHash = 0;
    }
    if (cryptKey) {
        CryptDestroyKey(cryptKey);
        cryptKey = 0;
    }
    if (cryptProvider) {
        CryptReleaseContext(cryptProvider, 0);
        cryptProvider = 0;
    }
}

qint64 DigestFilterPrivate::process(const char *data, unsigned int size)
{
    if (CryptHashData(cryptHash, (const BYTE *) data, size, 0))
        return 0;
    else
        return GetLastError();
}

qint64 DigestFilterPrivate::finish(char *result, unsigned int *size)
{
    DWORD hashSize = q->size();
    bool ok = CryptGetHashParam(cryptHash, HP_HASHVAL, (BYTE *) result, &hashSize, 0);
    if (size)
        *size = ok ? hashSize : 0;
    return ok ? 0 : GetLastError();
}

QT_END_NAMESPACE_AM
