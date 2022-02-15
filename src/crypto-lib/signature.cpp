/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include "signature.h"
#include "signature_p.h"
#include "cryptography.h"
#include "exception.h"

QT_BEGIN_NAMESPACE_AM

Signature::Signature(const QByteArray &hash)
    : d(new SignaturePrivate)
{
    // We have to use ASCII here since the default S/MIME content is text/plain.
    // This is what can be supported easily cross-platform without diving
    // deeply into low-level PKCS7 APIs.
    d->hash = hash.toBase64();
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
        // Although OpenSSL could, the macOS Security Framework (pre macOS 12) cannot
        // process empty detached data. So we better just not support it at all.
        if (d->hash.isEmpty())
            throw Exception("cannot sign an empty hash value");

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

QT_END_NAMESPACE_AM
