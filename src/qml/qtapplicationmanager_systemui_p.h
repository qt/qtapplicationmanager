// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QTAPPLICATIONMANAGER_SYSTEMUI_P_H
#define QTAPPLICATIONMANAGER_SYSTEMUI_P_H
#include <QtQml/QQmlEngine>
#include <QtAppManIntentServer/intent.h>
#include <QtAppManIntentServer/intentmodel.h>
#include <QtAppManIntentServer/intentserver.h>
#include <QtAppManManager/intentaminterface.h>
#include <QtAppManManager/application.h>
#include <QtAppManManager/applicationmanager.h>
#include <QtAppManManager/applicationmodel.h>
#include <QtAppManManager/package.h>
#include <QtAppManManager/packagemanager.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManManager/notificationmodel.h>
#include <QtAppManManager/abstractruntime.h>
#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManManager/processstatus.h>
#include <QtAppManWindow/windowmanager.h>
#include <QtAppManWindow/window.h>
#include <QtAppManWindow/windowitem.h>


QT_BEGIN_NAMESPACE

class ForeignIntent
{
    Q_GADGET
    QML_FOREIGN(QtAM::Intent)
    QML_NAMED_ELEMENT(IntentObject)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignIntentModel
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentModel)
    QML_NAMED_ELEMENT(IntentModel)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignIntentServer
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentServer)
    QML_NAMED_ELEMENT(IntentServer)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::IntentServer *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::IntentServer::instance(), QQmlEngine::CppOwnership);
        return QtAM::IntentServer::instance();
    }
};

class ForeignApplicationManager
{
    Q_GADGET
    QML_FOREIGN(QtAM::ApplicationManager)
    QML_NAMED_ELEMENT(ApplicationManager)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::ApplicationManager *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::ApplicationManager::instance(), QQmlEngine::CppOwnership);
        return QtAM::ApplicationManager::instance();
    }
};

class ForeignNotificationManager
{
    Q_GADGET
    QML_FOREIGN(QtAM::NotificationManager)
    QML_NAMED_ELEMENT(NotificationManager)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::NotificationManager *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::NotificationManager::instance(), QQmlEngine::CppOwnership);
        return QtAM::NotificationManager::instance();
    }
};

class ForeignPackageManager
{
    Q_GADGET
    QML_FOREIGN(QtAM::PackageManager)
    QML_NAMED_ELEMENT(PackageManager)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::PackageManager *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::PackageManager::instance(), QQmlEngine::CppOwnership);
        return QtAM::PackageManager::instance();
    }
};

class ForeignWindowManager
{
    Q_GADGET
    QML_FOREIGN(QtAM::WindowManager)
    QML_NAMED_ELEMENT(WindowManager)
    QML_ADDED_IN_VERSION(2, 0)
    QML_SINGLETON
public:
    static QtAM::WindowManager *create(QQmlEngine *, QJSEngine *)
    {
        QQmlEngine::setObjectOwnership(QtAM::WindowManager::instance(), QQmlEngine::CppOwnership);
        return QtAM::WindowManager::instance();
    }
};

class ForeignApplicationModel
{
    Q_GADGET
    QML_FOREIGN(QtAM::ApplicationModel)
    QML_NAMED_ELEMENT(ApplicationModel)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignNotificationModel
{
    Q_GADGET
    QML_FOREIGN(QtAM::NotificationModel)
    QML_NAMED_ELEMENT(NotificationModel)
    QML_ADDED_IN_VERSION(2, 2)
};

class ForeignIntentServerHandler
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentServerHandler)
    QML_NAMED_ELEMENT(IntentServerHandler)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignProcessStatus
{
    Q_GADGET
    QML_FOREIGN(QtAM::ProcessStatus)
    QML_NAMED_ELEMENT(ProcessStatus)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignWindowItem
{
    Q_GADGET
    QML_FOREIGN(QtAM::WindowItem)
    QML_NAMED_ELEMENT(WindowItem)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignApplicationObject
{
    Q_GADGET
    QML_FOREIGN(QtAM::Application)
    QML_NAMED_ELEMENT(ApplicationObject)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignContainer
{
    Q_GADGET
    QML_FOREIGN(QtAM::AbstractContainer)
    QML_NAMED_ELEMENT(Container)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignPackageObject
{
    Q_GADGET
    QML_FOREIGN(QtAM::Package)
    QML_NAMED_ELEMENT(PackageObject)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignRuntime
{
    Q_GADGET
    QML_FOREIGN(QtAM::AbstractRuntime)
    QML_NAMED_ELEMENT(Runtime)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignWindowObject
{
    Q_GADGET
    QML_FOREIGN(QtAM::Window)
    QML_NAMED_ELEMENT(WindowObject)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

class ForeignNamespaceAm
{
    Q_GADGET
    QML_FOREIGN(QtAM::Am)
    QML_NAMED_ELEMENT(Am)
    QML_ADDED_IN_VERSION(2, 0)
    QML_UNCREATABLE("")
};

QT_END_NAMESPACE

// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // QTAPPLICATIONMANAGER_SYSTEMUI_P_H
