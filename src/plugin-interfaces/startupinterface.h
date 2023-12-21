// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QtPlugin>
#include <QtAppManPluginInterfaces/qtappman_plugininterfaces-config.h>


QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QWindow)

class StartupInterface
{
    Q_DISABLE_COPY_MOVE(StartupInterface)

public:
    StartupInterface();
    virtual ~StartupInterface();

    virtual void initialize(const QVariantMap &systemProperties) noexcept(false) = 0;

    virtual void afterRuntimeRegistration() noexcept(false) = 0;
    virtual void beforeQmlEngineLoad(QQmlEngine *engine) noexcept(false) = 0;
    virtual void afterQmlEngineLoad(QQmlEngine *engine) noexcept(false) = 0;

    virtual void beforeWindowShow(QWindow *window) noexcept(false) = 0;
    virtual void afterWindowShow(QWindow *window) noexcept(false) = 0;
};

#define AM_StartupInterface_iid "io.qt.ApplicationManager.StartupInterface"

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(StartupInterface, AM_StartupInterface_iid)
QT_END_NAMESPACE
