// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONINTERFACE_H
#define APPLICATIONINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>


QT_BEGIN_NAMESPACE_AM

class Notification;
class ApplicationInterfaceImpl;

class ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager.ApplicationInterface")
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true FINAL)
    Q_PROPERTY(QVariantMap name READ name CONSTANT FINAL)
    Q_PROPERTY(QUrl icon READ icon CONSTANT FINAL)
    Q_PROPERTY(QString version READ version CONSTANT FINAL)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT SCRIPTABLE true FINAL)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT SCRIPTABLE true FINAL)

public:
    ~ApplicationInterface() override = default;

    template <typename IMPL, typename ...Args> static ApplicationInterface *create(QObject *parent, Args... args)
    {
        auto iface = new ApplicationInterface(parent);
        iface->m_impl = std::make_unique<IMPL>(iface, args...);
        return iface;
    }

    ApplicationInterfaceImpl *implementation();

    QString applicationId() const;
    QVariantMap name() const;
    QUrl icon() const;
    QString version() const;
    QVariantMap systemProperties() const;
    QVariantMap applicationProperties() const;

    Q_INVOKABLE QtAM::Notification *createNotification();
    Q_INVOKABLE void acknowledgeQuit();

Q_SIGNALS:
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void memoryLowWarning();
    Q_SCRIPTABLE void memoryCriticalWarning();

    Q_SCRIPTABLE void openDocument(const QString &documentUrl, const QString &mimeType);

    Q_SCRIPTABLE void slowAnimationsChanged(bool isSlow);

private:
    ApplicationInterface(QObject *parent);
    Q_DISABLE_COPY_MOVE(ApplicationInterface)

    std::unique_ptr<ApplicationInterfaceImpl> m_impl;
    friend class ApplicationInterfaceImpl;
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONINTERFACE_H
