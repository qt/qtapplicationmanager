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
