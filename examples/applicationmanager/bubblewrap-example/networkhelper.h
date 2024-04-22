// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef NETWORKHELPER_H
#define NETWORKHELPER_H

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

class NetworkHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList ipAddresses READ ipAddresses CONSTANT)

public:
    explicit NetworkHelper(QObject *parent = nullptr);

    QStringList ipAddresses() const;

Q_SIGNALS:

private:
    QStringList m_ipAddresses;
};

#endif // NETWORKHELPER_H
