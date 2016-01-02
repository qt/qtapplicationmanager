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

#include "digestfilter.h"
#include "digestfilter_p.h"
#include "cryptography.h"


HMACFilter::HMACFilter(Type t, const QByteArray &key)
    : DigestFilter(t, key)
{ }

DigestFilter::DigestFilter(Type t)
    : d(new DigestFilterPrivate)
{
    d->q = this;
    d->isHmac = false;
    d->type = t;

    Cryptography::initialize();
}

DigestFilter::DigestFilter(Type t, const QByteArray &hmacKey)
    : d(new DigestFilterPrivate)
{
    d->q = this;
    d->isHmac = true;
    d->type = t;
    d->key = hmacKey;
}

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
        d->errorString = QLatin1String("Called start() on an already started DigestFilter");
        return false;
    }
    if (d->type < DigestFilter::Sha1 || d->type > DigestFilter::Sha512) {
        d->errorString = QLatin1String("Called start() on an invalid DigestFilter");
        return false;
    }
    return d->initialized = d->init();
}

bool DigestFilter::processData(const QByteArray &data)
{
    return processData(data.constData(), data.size());
}

bool DigestFilter::processData(const char *data, int size)
{
    if (!d->initialized) {
        d->errorString = QLatin1String("Called processData() on a DigestFilter which was not start()ed yet");
        return false;
    }
    if (!data || size <= 0)
        return true;

    qint64 err = d->process(data, size);

    if (err) {
        d->errorString = Cryptography::errorString(err, "Failed to calculate digest");
        d->cleanup();
        d->initialized = false;
    }
    return !err;
}

bool DigestFilter::finish(QByteArray &digest)
{
    if (!d->initialized) {
        d->errorString = QLatin1String("Called finish() on a DigestFilter which was not start()ed yet");
        return false;
    }

    unsigned int outlen = 0;
    digest.resize(size());
    qint64 err = d->finish(digest.data(), &outlen);

    if (!err) {
        digest.resize(outlen);
    } else {
        d->errorString = Cryptography::errorString(err, "Failed to finalize digest");
        digest.clear();
    }

    d->cleanup();
    d->initialized = false;
    return !err;
}

QString DigestFilter::errorString() const
{
    return QLatin1String("Digest error: ") + d->errorString;
}

int DigestFilter::size() const
{
    switch (type()) {
        case DigestFilter::Sha512: return 64;
        case DigestFilter::Sha256: return 32;
        case DigestFilter::Sha1  : return 20;
        default                  : return 0;
    }
}
