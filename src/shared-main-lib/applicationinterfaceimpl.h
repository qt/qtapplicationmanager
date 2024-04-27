// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONINTERFACEIMPL_H
#define APPLICATIONINTERFACEIMPL_H

#include <QtCore/QObject>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Notification;
class ApplicationInterface;


class ApplicationInterfaceImpl : public QObject
{
    Q_OBJECT

public:
    static void setFactory(const std::function<ApplicationInterfaceImpl *(ApplicationInterface *)> &factory);
    static ApplicationInterfaceImpl *create(ApplicationInterface *iface);

    virtual void attach(ApplicationInterface *iface);
    virtual void detach(ApplicationInterface *iface);

    virtual ~ApplicationInterfaceImpl() = default;

    QList<ApplicationInterface *> amInterfaces();

    virtual QString applicationId() const = 0;
    virtual QVariantMap name() const = 0;
    virtual QUrl icon() const = 0;
    virtual QString version() const = 0;
    virtual QVariantMap systemProperties() const = 0;
    virtual QVariantMap applicationProperties() const = 0;
    virtual void acknowledgeQuit() = 0;

    void handleQuit();

protected:
    ApplicationInterfaceImpl();

private:
    QList<ApplicationInterface *> m_aminterfaces;

    static std::function<ApplicationInterfaceImpl *(ApplicationInterface *)> s_factory;
    Q_DISABLE_COPY_MOVE(ApplicationInterfaceImpl)
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONINTERFACEIMPL_H
