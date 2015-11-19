/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
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

#include "cipherfilter.h"
#include "cryptography.h"
#include "libcryptofunction.h"

#include <openssl/evp.h>
#include <openssl/err.h>

static AM_LIBCRYPTO_FUNCTION(ERR_get_error, ERR_R_INTERNAL_ERROR);
static AM_LIBCRYPTO_FUNCTION(ERR_error_string, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_128_cbc, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_128_cfb128, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_128_ofb, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_256_cbc, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_256_cfb128, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_aes_256_ofb, nullptr);
static AM_LIBCRYPTO_FUNCTION(EVP_CIPHER_block_size, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CIPHER_key_length, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CIPHER_iv_length, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CIPHER_CTX_cleanup, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CipherInit, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CipherUpdate, 0);
static AM_LIBCRYPTO_FUNCTION(EVP_CipherFinal, 0);


class CipherFilterPrivate
{
public:
    CipherFilter::Type type;
    bool initialized;

    EVP_CIPHER_CTX ctx;
    const EVP_CIPHER *cipher;
    int cipherBlockSize;
    int cipherKeySize;
    int cipherIVSize;

    unsigned long error;
    QString customError;

    void setError()
    {
        error = am_ERR_get_error();
        customError.clear();
    }
    void setError(const QString &str)
    {
        error = (unsigned long) -1;
        customError = str;
    }
    QString errorString()
    {
        if (error == (unsigned long) -1)
            return customError;
        else
            return QString::fromLocal8Bit(am_ERR_error_string(error, nullptr));
    }
};


static inline const unsigned char *s2u(const char *s)
{
    return reinterpret_cast<const unsigned char *>(s);
}

static inline unsigned char *s2u(char *s)
{
    return reinterpret_cast<unsigned char *>(s);
}

CipherFilter::CipherFilter(Type t, Cipher cipher)
    : d(new CipherFilterPrivate())
{
    d->type = t;
    Cryptography::initialize();

    switch (cipher) {
    case AES_128_CBC: d->cipher = am_EVP_aes_128_cbc(); break;
    case AES_128_CFB: d->cipher = am_EVP_aes_128_cfb128(); break;
    case AES_128_OFB: d->cipher = am_EVP_aes_128_ofb(); break;
    case AES_256_CBC: d->cipher = am_EVP_aes_256_cbc(); break;
    case AES_256_CFB: d->cipher = am_EVP_aes_256_cfb128(); break;
    case AES_256_OFB: d->cipher = am_EVP_aes_256_ofb(); break;
    default         : d->cipher = 0; break;
    }

    d->cipherBlockSize = d->cipher ? am_EVP_CIPHER_block_size(d->cipher) : 0;
    d->cipherKeySize = d->cipher ? am_EVP_CIPHER_key_length(d->cipher) : 0;
    d->cipherIVSize = d->cipher ? am_EVP_CIPHER_iv_length(d->cipher) : 0;
    d->error = 0;

    d->initialized = false;
}

CipherFilter::~CipherFilter()
{
    if (d->initialized)
        am_EVP_CIPHER_CTX_cleanup(&d->ctx);
    delete d;
}

bool CipherFilter::start(const char *key, int keyLen, const char *iv, int ivLen)
{
    if (d->initialized) {
        d->setError(QLatin1String("Called start() on an already started CipherFilter"));
        return false;
    }
    if (!d->cipher) {
         d->setError(QLatin1String("Called start() on an invalid CipherFilter"));
         return false;
    }
    if (!key || keyLen != d->cipherKeySize) {
        d->setError(QLatin1String("CipherFilter::start() was called with an invalid key"));
        return false;
    }
    if ((d->cipherIVSize && !iv) || ivLen != d->cipherIVSize) {
        d->setError(QLatin1String("CipherFilter::start() was called with an invalid IV"));
        return false;
    }

    int ret = am_EVP_CipherInit(&d->ctx,
                                d->cipher,
                                s2u(key),
                                s2u(iv),
                                d->type == Encrypt ? 1 : 0);
    if (ret == 0)
        d->setError();

    return d->initialized = (ret == 1);
}

bool CipherFilter::processData(const QByteArray &src, QByteArray &dst)
{
    if (!d->initialized) {
        d->setError(QLatin1String("Called processData() on an CipherFilter which was not start()ed yet"));
        return false;
    }
    if (src.isEmpty()) {
        dst = src;
        return true;
    }
    return internalProcessData(src, dst, false);
}

bool CipherFilter::finish(QByteArray &dst)
{
    if (!d->initialized) {
        d->setError(QLatin1String("Called finish() on an CipherFilter which was not start()ed yet"));
        return false;
    }

    bool result = internalProcessData(QByteArray(), dst, true); // final chunk
    am_EVP_CIPHER_CTX_cleanup(&d->ctx);
    d->initialized = false;

    return result;
}

bool CipherFilter::internalProcessData(const QByteArray &src, QByteArray &dst, bool lastChunk)
{
    Q_ASSERT(d->initialized);
    Q_ASSERT(src.isEmpty() == lastChunk);

    int outlen = 0;
    int ret;
    if (!lastChunk) {
        dst.resize(src.size() + d->cipherBlockSize - (d->type == CipherFilter::Encrypt ? 1 : 0));
        ret = am_EVP_CipherUpdate(&d->ctx,
                                  s2u(dst.data()),
                                  &outlen,
                                  s2u(src.constData()),
                                  src.size());
    } else {
        dst.resize(d->cipherBlockSize);
        ret = am_EVP_CipherFinal(&d->ctx,
                                 s2u(dst.data()),
                                 &outlen);
    }
    if (ret == 1) {
        dst.resize(outlen);
    } else {
        d->setError();
        dst.clear();
    }
    return (ret == 1);
}

QString CipherFilter::errorString() const
{
    return QLatin1String("OpenSSL EVP error: ") + d->errorString();
}

int CipherFilter::blockSize() const
{
    return d->cipherBlockSize;
}

int CipherFilter::ivSize() const
{
    return d->cipherIVSize;
}

int CipherFilter::keySize() const
{
    return d->cipherKeySize;
}
