// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QtPlugin>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QWindow)

class StartupInterface
{
public:
    virtual ~StartupInterface();

    virtual void initialize(const QVariantMap &systemProperties) Q_DECL_NOEXCEPT_EXPR(false) = 0;

    virtual void afterRuntimeRegistration() Q_DECL_NOEXCEPT_EXPR(false) = 0;
    virtual void beforeQmlEngineLoad(QQmlEngine *engine) Q_DECL_NOEXCEPT_EXPR(false) = 0;
    virtual void afterQmlEngineLoad(QQmlEngine *engine) Q_DECL_NOEXCEPT_EXPR(false) = 0;

    virtual void beforeWindowShow(QWindow *window) Q_DECL_NOEXCEPT_EXPR(false) = 0;
    virtual void afterWindowShow(QWindow *window) Q_DECL_NOEXCEPT_EXPR(false) = 0;
};

#define AM_StartupInterface_iid "io.qt.ApplicationManager.StartupInterface"

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(StartupInterface, AM_StartupInterface_iid)
QT_END_NAMESPACE
