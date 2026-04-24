// Copyright (C) 2024 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONINTERFACE_H
#define APPLICATIONINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <QtAppManShared/qtappmansharedglobal.h>


QT_BEGIN_NAMESPACE_AM

class Notification;
class ApplicationInterfaceImpl;

class Q_APPMANSHARED_EXPORT ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager.ApplicationInterface")
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true FINAL)
    Q_PROPERTY(QVariantMap name READ name CONSTANT FINAL)
    Q_PROPERTY(QUrl icon READ icon CONSTANT FINAL)
    Q_PROPERTY(QString version READ version CONSTANT FINAL)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT SCRIPTABLE true FINAL)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT SCRIPTABLE true FINAL)
    Q_PROPERTY(QVariantMap extraDirs READ extraDirs CONSTANT FINAL)
    QML_ATTACHED(QtAM::ApplicationInterface)

public:
    ~ApplicationInterface() override;

    static ApplicationInterface *qmlAttachedProperties(QObject *object);

    ApplicationInterfaceImpl *implementation();

    QString applicationId() const;
    QVariantMap name() const;
    QUrl icon() const;
    QString version() const;
    QVariantMap systemProperties() const;
    QVariantMap applicationProperties() const;
    QVariantMap extraDirs() const;

    Q_INVOKABLE QtAM::Notification *createNotification();
    Q_INVOKABLE void acknowledgeQuit();

Q_SIGNALS:
    Q_SCRIPTABLE void quit();
#if QT_DEPRECATED_SINCE(6, 12)
    QT_DEPRECATED_VERSION_X_6_12("Use CGroupStatus instead.") Q_SCRIPTABLE void memoryLowWarning();
    QT_DEPRECATED_VERSION_X_6_12("Use CGroupStatus instead.") Q_SCRIPTABLE void memoryCriticalWarning();
#endif

    Q_SCRIPTABLE void openDocument(const QString &documentUrl, const QString &mimeType);

private:
    QPointer<ApplicationInterfaceImpl> m_impl;

    explicit ApplicationInterface(QObject *parent);
    Q_DISABLE_COPY_MOVE(ApplicationInterface)

    friend class ApplicationInterfaceImpl; // for isSignalConnected
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONINTERFACE_H
