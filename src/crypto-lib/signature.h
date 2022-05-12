/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/
#pragma once

#include <QString>
#include <QByteArray>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate;

class Signature
{
public:
    explicit Signature(const QByteArray &hash);
    ~Signature();

    QByteArray create(const QByteArray &signingCertificatePkcs12, const QByteArray &signingCertificatePassword);
    bool verify(const QByteArray &signaturePkcs7, const QList<QByteArray> &chainOfTrust);

    QString errorString() const;

private:
    SignaturePrivate *d;
    Q_DISABLE_COPY(Signature)
};

QT_END_NAMESPACE_AM
