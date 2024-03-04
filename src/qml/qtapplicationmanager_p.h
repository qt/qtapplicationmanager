// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QTAPPLICATIONMANAGER_P_H
#define QTAPPLICATIONMANAGER_P_H
#include <QtQml/QQmlEngine>
#include <QtAppManIntentClient/intentclient.h>
#include <QtAppManIntentClient/intenthandler.h>
#include <QtAppManIntentClient/intentclientrequest.h>
#include <QtAppManSharedMain/notification.h>
#include <QtAppManSharedMain/cpustatus.h>
#include <QtAppManSharedMain/frametimer.h>
#include <QtAppManSharedMain/gpustatus.h>
#include <QtAppManSharedMain/iostatus.h>
#include <QtAppManSharedMain/memorystatus.h>
#include <QtAppManSharedMain/monitormodel.h>
#include <QtAppManSharedMain/startuptimer.h>


QT_BEGIN_NAMESPACE

class ForeignIntentClient
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentClient)
    QML_NAMED_ELEMENT(IntentClient)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::IntentClient *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::IntentClient::instance(), QQmlEngine::CppOwnership);
        return QtAM::IntentClient::instance();
    }
};

class ForeignStartupTimer
{
    Q_GADGET
    QML_FOREIGN(QtAM::StartupTimer)
    QML_NAMED_ELEMENT(StartupTimer)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::StartupTimer *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::StartupTimer::instance(), QQmlEngine::CppOwnership);
        return QtAM::StartupTimer::instance();
    }
};

class ForeignIntentRequest
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentClientRequest)
    QML_NAMED_ELEMENT(IntentRequest)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignNotification
{
    Q_GADGET
    QML_FOREIGN(QtAM::Notification)
    QML_NAMED_ELEMENT(Notification)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignCpuStatus
{
    Q_GADGET
    QML_FOREIGN(QtAM::CpuStatus)
    QML_NAMED_ELEMENT(CpuStatus)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignFrameTimer
{
    Q_GADGET
    QML_FOREIGN(QtAM::FrameTimer)
    QML_NAMED_ELEMENT(FrameTimer)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignGpuStatus
{
    Q_GADGET
    QML_FOREIGN(QtAM::GpuStatus)
    QML_NAMED_ELEMENT(GpuStatus)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignIoStatus
{
    Q_GADGET
    QML_FOREIGN(QtAM::IoStatus)
    QML_NAMED_ELEMENT(IoStatus)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignMemoryStatus
{
    Q_GADGET
    QML_FOREIGN(QtAM::MemoryStatus)
    QML_NAMED_ELEMENT(MemoryStatus)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignMonitorModel
{
    Q_GADGET
    QML_FOREIGN(QtAM::MonitorModel)
    QML_NAMED_ELEMENT(MonitorModel)
    QML_ADDED_IN_VERSION(2, 0)
};

QT_END_NAMESPACE

// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // QTAPPLICATIONMANAGER_P_H
