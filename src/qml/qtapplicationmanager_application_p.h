// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QTAPPLICATIONMANAGER_APPLICATION_P_H
#define QTAPPLICATIONMANAGER_APPLICATION_P_H
#include <QtQml/QQmlEngine>
#include <QtAppManIntentClient/intenthandler.h>
#include <QtAppManSharedMain/applicationmanagerwindow.h>
#include <QtAppManSharedMain/applicationinterface.h>


QT_BEGIN_NAMESPACE

class ForeignIntentHandler
{
    Q_GADGET
    QML_FOREIGN(QtAM::IntentHandler)
    QML_NAMED_ELEMENT(IntentHandler)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignApplicationManagerWindow
{
    Q_GADGET
    QML_FOREIGN(QtAM::ApplicationManagerWindow)
    QML_NAMED_ELEMENT(ApplicationManagerWindow)
    QML_ADDED_IN_VERSION(2, 0)
};

class ForeignApplicationInterface
{
    Q_GADGET
    QML_FOREIGN(QtAM::ApplicationInterface)
    QML_ADDED_IN_VERSION(2, 0)
    QML_ANONYMOUS
};

QT_END_NAMESPACE

// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // QTAPPLICATIONMANAGER_APPLICATION_P_H
