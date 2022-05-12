/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QObject>
#include <QtPlugin>

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
