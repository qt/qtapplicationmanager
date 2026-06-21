// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef NOTIFYHELPER_H
#define NOTIFYHELPER_H

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

class NotifyHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit NotifyHelper(QObject *parent = nullptr);

    // posts a notification via the org.freedesktop.Notifications D-Bus service on the session bus,
    // using the given app_name, and returns the resulting notification id (0 on failure/rejection)
    Q_INVOKABLE uint notify(const QString &appName, const QString &summary);
};

#endif // NOTIFYHELPER_H
