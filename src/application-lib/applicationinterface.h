// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager.ApplicationInterface")
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true)
    Q_PROPERTY(QVariantMap name READ name CONSTANT)
    Q_PROPERTY(QUrl icon READ icon CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT SCRIPTABLE true)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT SCRIPTABLE true)

public:
    virtual QString applicationId() const = 0;
    virtual QVariantMap name() const = 0;
    virtual QUrl icon() const = 0;
    virtual QString version() const = 0;
    virtual QVariantMap systemProperties() const = 0;
    virtual QVariantMap applicationProperties() const = 0;

#ifdef Q_QDOC
    Q_INVOKABLE Notification *createNotification();
    Q_INVOKABLE virtual void acknowledgeQuit() const;
#endif
    Q_SCRIPTABLE virtual void finishedInitialization() = 0;

signals:
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void memoryLowWarning();
    Q_SCRIPTABLE void memoryCriticalWarning();

    Q_SCRIPTABLE void openDocument(const QString &documentUrl, const QString &mimeType);
    Q_SCRIPTABLE void interfaceCreated(const QString &interfaceName);

    Q_SCRIPTABLE void slowAnimationsChanged(bool isSlow);

protected:
    ApplicationInterface(QObject *parent)
        : QObject(parent)
    { }

private:
    Q_DISABLE_COPY(ApplicationInterface)
};

QT_END_NAMESPACE_AM
