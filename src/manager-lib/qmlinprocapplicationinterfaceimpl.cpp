// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
#include "notificationmanager.h"
#include "intentclientrequest.h"

QT_BEGIN_NAMESPACE_AM

QmlInProcApplicationInterfaceImpl::QmlInProcApplicationInterfaceImpl(ApplicationInterface *ai,
                                                                     QmlInProcRuntime *runtime)
    : ApplicationInterfaceImpl(ai)
    , m_runtime(runtime)
{
    QObject::connect(ApplicationManager::instance(), &ApplicationManager::memoryLowWarning,
                     ai, &ApplicationInterface::memoryLowWarning);
    QObject::connect(ApplicationManager::instance(), &ApplicationManager::memoryCriticalWarning,
                     ai, &ApplicationInterface::memoryCriticalWarning);
    QObject::connect(runtime, &QmlInProcRuntime::aboutToStop,
                     ai, [this]() { handleQuit(); });

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
        return m_runtime->application()->packageInfo()->icon();
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

void QmlInProcApplicationInterfaceImpl::acknowledgeQuit()
{
    if (m_runtime)
        m_runtime->finish(0, Am::NormalExit);
}

QT_END_NAMESPACE_AM
