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

#include <QtAppManCommon/global.h>
#include <QVariantMap>
#include <QStringList>
#include <QSurfaceFormat>

QT_FORWARD_DECLARE_STRUCT(QQmlDebuggingEnabler)

QT_BEGIN_NAMESPACE_AM

class SharedMain
{
public:
    SharedMain();
    ~SharedMain();

    static void initialize();
    static int &preConstructor(int &argc);
    void setupIconTheme(const QStringList &themeSearchPaths, const QString &themeName);
    void setupQmlDebugging(bool qmlDebugging);
    void setupLogging(bool verbose, const QStringList &loggingRules, const QString &messagePattern, const QVariant &useAMConsoleLogger);
    void setupOpenGL(const QVariantMap &openGLConfiguration);\
    void checkOpenGLFormat(const char *what, const QSurfaceFormat &format) const;

private:
    QQmlDebuggingEnabler *m_debuggingEnabler = nullptr;

    static bool s_initialized;
    QSurfaceFormat::OpenGLContextProfile m_requestedOpenGLProfile = QSurfaceFormat::NoProfile;
    int m_requestedOpenGLMajorVersion = -1;
    int m_requestedOpenGLMinorVersion = -1;
};

QT_END_NAMESPACE_AM
