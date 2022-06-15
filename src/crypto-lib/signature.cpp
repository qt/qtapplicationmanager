// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
