// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDir>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlInfo>

#include "logging.h"
#include "qmlinprocapplicationinterfaceimpl.h"
#include "qmlinprocnotificationimpl.h"
#include "qmlinprocruntime.h"
#include "application.h"
#include "applicationinterface.h"
#include "applicationmanager.h"
#include "notification.h"
#include "intentclientrequest.h"

QT_BEGIN_NAMESPACE_AM

QmlInProcApplicationInterfaceImpl::QmlInProcApplicationInterfaceImpl(QmlInProcRuntime *runtime)
    : ApplicationInterfaceImpl()
    , m_runtime(runtime)
{
    QObject::connect(runtime, &QmlInProcRuntime::aboutToStop,
                     runtime, [this]() { handleQuit(); });
}

void QmlInProcApplicationInterfaceImpl::attach(ApplicationInterface *iface)
{
    ApplicationInterfaceImpl::attach(iface);

    QmlInProcNotificationImpl::initialize();
}

QString QmlInProcApplicationInterfaceImpl::applicationId() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->id();
    return { };
}

QVariantMap QmlInProcApplicationInterfaceImpl::name() const
{
    QVariantMap names;
    if (m_runtime && m_runtime->application()) {
        const QMap<QString, QString> &sm = m_runtime->application()->packageInfo()->names();
        for (auto it = sm.cbegin(); it != sm.cend(); ++it)
            names.insert(it.key(), it.value());
    }
    return names;
}

QUrl QmlInProcApplicationInterfaceImpl::icon() const
{
    if (m_runtime && m_runtime->application())
        return QUrl(m_runtime->application()->packageInfo()->icon());
    return { };
}

QString QmlInProcApplicationInterfaceImpl::version() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->version();
    return { };
}

QVariantMap QmlInProcApplicationInterfaceImpl::systemProperties() const
{
    if (m_runtime)
        return m_runtime->systemProperties();
    return { };
}

QVariantMap QmlInProcApplicationInterfaceImpl::applicationProperties() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->info()->allAppProperties();
    return { };
}

QVariantMap QmlInProcApplicationInterfaceImpl::extraDirs() const
{
    if (!m_runtime || !m_runtime->application())
        return { };
    const QString appId = m_runtime->application()->id();
    QVariantMap result;
    for (const auto &[name, path] : m_runtime->m_applicationExtraDirs.asKeyValueRange())
        result.insert(name, QDir::cleanPath(path + QDir::separator() + appId));
    return result;
}

void QmlInProcApplicationInterfaceImpl::acknowledgeQuit()
{
    if (m_runtime)
        m_runtime->finish(0, Am::NormalExit);
}

QT_END_NAMESPACE_AM
