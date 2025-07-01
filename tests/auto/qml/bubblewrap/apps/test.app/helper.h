// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef HELPER_H
#define HELPER_H

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

class Helper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit Helper(QObject *parent = nullptr);

    Q_INVOKABLE QByteArray getEnv(const QByteArray &envName);
};

#endif // HELPER_H
