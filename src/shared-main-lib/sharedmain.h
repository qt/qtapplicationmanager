// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SHAREDMAIN_H
#define SHAREDMAIN_H

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/openglconfiguration.h>
#include <QtCore/QVariantMap>
#include <QtCore/QStringList>
#include <QtGui/QSurfaceFormat>

QT_FORWARD_DECLARE_STRUCT(QQmlDebuggingEnabler)

QT_BEGIN_NAMESPACE_AM

class Watchdog;


class SharedMain
{
public:
    SharedMain();
    ~SharedMain();

    static void initialize();
    static int &preConstructor(int &argc);
    void setupIconTheme(const QStringList &themeSearchPaths, const QString &themeName);
    void setupQmlDebugging(bool qmlDebugging);
    void setupWatchdog(std::chrono::milliseconds eventloopCheckInterval,
                       std::chrono::milliseconds eventloopWarn,
                       std::chrono::milliseconds eventloopKill,
                       std::chrono::milliseconds quickWindowCheckInterval,
                       std::chrono::milliseconds quickWindowSyncWarn,
                       std::chrono::milliseconds quickWindowSyncKill,
                       std::chrono::milliseconds quickWindowRenderWarn,
                       std::chrono::milliseconds quickWindowRenderKill,
                       std::chrono::milliseconds quickWindowSwapWarn,
                       std::chrono::milliseconds quickWindowSwapKill);
    void setupLogging(bool verbose, const QStringList &loggingRules, const QString &messagePattern,
                      const QVariant &useAMConsoleLogger);
    void setupOpenGL(const OpenGLConfiguration &openGLConfiguration);
    void checkOpenGLFormat(const char *what, const QSurfaceFormat &format) const;

private:
    static bool s_initialized;
    QSurfaceFormat::OpenGLContextProfile m_requestedOpenGLProfile = QSurfaceFormat::NoProfile;
    int m_requestedOpenGLMajorVersion = -1;
    int m_requestedOpenGLMinorVersion = -1;
    Watchdog *m_watchdog = nullptr;
};

QT_END_NAMESPACE_AM

#endif // SHAREDMAIN_H
