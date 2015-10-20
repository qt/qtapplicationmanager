/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include "cryptography.h"
#include "digestfilter.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
#  error "OpenSSL version >= 1.0.0 is needed due to a HMAC API change"
#endif

class DigestFilterPrivate
{
public:
    DigestFilterPrivate(DigestFilter::Type t)
        : type(t)
        , initialized(false)
        , digest(typeToDigest(t))
        , error(0)
    {
        Cryptography::initialize();
    }

    static const EVP_MD *typeToDigest(DigestFilter::Type t)
    {
        switch (t) {
        case DigestFilter::Sha512: return EVP_sha512();
        case DigestFilter::Sha256: return EVP_sha256();
        case DigestFilter::Sha1  : return EVP_sha1();
        default                  : return 0;
        }
    }

    virtual ~DigestFilterPrivate()
    { }

    DigestFilter::Type type;
    bool initialized;

    const EVP_MD *digest;

    unsigned long error;
    QString customError;

    virtual int size() = 0;
    virtual int init() = 0;
    virtual void cleanup() = 0;
    virtual int process(const char *data, unsigned int size) = 0;
    virtual int finish(char *result, unsigned int *size) = 0;

    void setError()
    {
        error = ERR_get_error();
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
            return QString::fromLocal8Bit(ERR_error_string(error, 0));
    }
};

class MDFilterPrivate : public DigestFilterPrivate
{
public:
    MDFilterPrivate(DigestFilter::Type t)
        : DigestFilterPrivate(t)
    { }

    EVP_MD_CTX ctx;

    int size() override { return EVP_MD_size(digest); }
    int init() override { return EVP_DigestInit(&ctx, digest); }
    void cleanup() override { EVP_MD_CTX_cleanup(&ctx); }
    int process(const char *data, unsigned int size) override { return EVP_DigestUpdate(&ctx, data, size); }
    int finish(char *result, unsigned int *size) override { return EVP_DigestFinal(&ctx, (unsigned char *) result, size); }
};

class HMACFilterPrivate : public DigestFilterPrivate
{
public:
    HMACFilterPrivate(DigestFilter::Type t, const QByteArray &hmacKey)
        : DigestFilterPrivate(t)
        , key(hmacKey)
    { }

    QByteArray key;
    HMAC_CTX ctx;

    int size() override { return EVP_MD_size(digest); }
    int init() override { return HMAC_Init(&ctx, key.constData(), key.size(), digest); }
    void cleanup() override { HMAC_CTX_cleanup(&ctx); }
    int process(const char *data, unsigned int size) override { return HMAC_Update(&ctx, (const unsigned char *) data, size); }
    int finish(char *result, unsigned int *size) override { return HMAC_Final(&ctx, (unsigned char *) result, size); }
};

HMACFilter::HMACFilter(Type t, const QByteArray &key)
    : DigestFilter(t, key)
{ }

DigestFilter::DigestFilter(Type t)
    : d(new MDFilterPrivate(t))
{
}

DigestFilter::DigestFilter(Type t, const QByteArray &hmacKey)
    : d(new HMACFilterPrivate(t, hmacKey))
{ }

DigestFilter::~DigestFilter()
{
    if (d->initialized)
        d->cleanup();
    delete d;
}

DigestFilter::Type DigestFilter::type() const
{
    return d->type;
}

QByteArray DigestFilter::digest(Type t, const QByteArray &data)
{
    DigestFilter df(t);
    if (df.start()) {
        if (df.processData(data)) {
            QByteArray md;
            if (df.finish(md))
                return md;
        }
    }
    return QByteArray();
}

QByteArray HMACFilter::hmac(Type t, const QByteArray &key, const QByteArray &data)
{
    HMACFilter hf(t, key);
    if (hf.start()) {
        if (hf.processData(data)) {
            QByteArray md;
            if (hf.finish(md))
                return md;
        }
    }
    return QByteArray();
}

bool DigestFilter::start()
{
    if (d->initialized) {
        d->setError(QLatin1String("Called start() on an already started DigestFilter"));
        return false;
    }
    if (!d->digest) {
        d->setError(QLatin1String("Called start() on an invalid DigestFilter"));
        return false;
    }

    int ret = d->init();
    if (ret == 0)
        d->setError();

    return d->initialized = (ret == 1);
}

bool DigestFilter::processData(const QByteArray &data)
{
    return processData(data.constData(), data.size());
}

bool DigestFilter::processData(const char *data, int size)
{
    if (!d->initialized) {
        d->setError(QLatin1String("Called processData() on a DigestFilter which was not start()ed yet"));
        return false;
    }
    if (!data || size <= 0)
        return true;

    int ret = d->process(data, size);

    if (ret == 1) {
        return true;
    } else {
        d->setError();
        d->cleanup();
        d->initialized = false;
        return false;
    }
}

bool DigestFilter::finish(QByteArray &digest)
{
    if (!d->initialized) {
        d->setError(QLatin1String("Called finish() on a DigestFilter which was not start()ed yet"));
        return false;
    }

    unsigned int outlen = 0;
    digest.resize(d->size());
    int ret = d->finish(digest.data(), &outlen);

    if (ret == 1) {
        digest.resize(outlen);
    } else {
        digest.clear();
        d->setError();
    }
    d->cleanup();
    d->initialized = false;
    return (ret == 1);
}

QString DigestFilter::errorString() const
{
    return QLatin1String("OpenSSL MD error: ") + d->errorString();
}

int DigestFilter::size() const
{
    if (d->digest)
        return d->size();
    return 0;
}

QString DigestFilter::nameFromType(Type t)
{
    const EVP_MD *md = DigestFilterPrivate::typeToDigest(t);
    if (md)
        return QString::fromLatin1(EVP_MD_name(md));
    return QString();
}

DigestFilter::Type DigestFilter::typeFromName(const QString &name, bool *ok)
{
    // const EVP_MD *EVP_get_digestbyname(const char *name);
    const EVP_MD *md = EVP_get_digestbyname(name.toLatin1().constData());
    if (ok)
        *ok = (md);
    if (md == EVP_sha512()) {
        return DigestFilter::Sha512;
    } else if (md == EVP_sha256()) {
        return DigestFilter::Sha256;
    } else if (md == EVP_sha1()) {
        return DigestFilter::Sha1;
    } else {
        if (ok)
            *ok = false;
        return DigestFilter::Sha1;
    }
}
