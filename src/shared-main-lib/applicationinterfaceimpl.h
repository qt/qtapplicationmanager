// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONINTERFACEIMPL_H
#define APPLICATIONINTERFACEIMPL_H

#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Notification;
class ApplicationInterface;

class ApplicationInterfaceImpl
{
public:
    ApplicationInterfaceImpl(ApplicationInterface *ai);
    virtual ~ApplicationInterfaceImpl() = default;

    ApplicationInterface *applicationInterface();

    virtual QString applicationId() const = 0;
    virtual QVariantMap name() const = 0;
    virtual QUrl icon() const = 0;
    virtual QString version() const = 0;
    virtual QVariantMap systemProperties() const = 0;
    virtual QVariantMap applicationProperties() const = 0;
    virtual void acknowledgeQuit() = 0;

    void handleQuit();

private:
    ApplicationInterface *m_interface = nullptr;
    Q_DISABLE_COPY_MOVE(ApplicationInterfaceImpl)
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONINTERFACEIMPL_H
