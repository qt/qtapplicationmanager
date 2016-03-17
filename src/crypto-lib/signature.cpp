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

#include "signature.h"
#include "signature_p.h"
#include "cryptography.h"


Signature::Signature(const QByteArray &hash)
    : d(new SignaturePrivate)
{
    d->hash = hash;
    Cryptography::initialize();
}

Signature::~Signature()
{
    delete d;
}

QString Signature::errorString() const
{
    return d->error;
}

QByteArray Signature::create(const QByteArray &signingCertificatePkcs12, const QByteArray &signingCertificatePassword)
{
    d->error.clear();
    try {
        QByteArray sig = d->create(signingCertificatePkcs12, signingCertificatePassword);
//        // very useful while debugging
//        QFile f(QDir::home().absoluteFilePath("sig.der"));
//        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
//            f.write(sig);
        return sig;
    } catch (const Exception &e) {
        d->error = e.errorString();
        return QByteArray();
    }
}

bool Signature::verify(const QByteArray &signaturePkcs7, const QList<QByteArray> &chainOfTrust)
{
    d->error.clear();

    try {
        return d->verify(signaturePkcs7, chainOfTrust);
    } catch (const Exception &e) {
        d->error = e.errorString();
        return false;
    }
}
