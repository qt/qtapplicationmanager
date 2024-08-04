// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SHAREDMAIN_H
#define SHAREDMAIN_H

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/openglconfiguration.h>
#include <QtAppManCommon/watchdogconfiguration.h>
#include <QtAppManSharedMain/watchdog.h>
#include <QtCore/QVariantMap>
#include <QtCore/QStringList>
#include <QtGui/QSurfaceFormat>

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
    void setupWatchdog(const WatchdogConfiguration &cfg);
    void setupLogging(bool verbose, const QStringList &loggingRules, const QString &messagePattern,
                      const QVariant &useAMConsoleLogger);
    void setupOpenGL(const OpenGLConfiguration &openGLConfiguration);
    void checkOpenGLFormat(const char *what, const QSurfaceFormat &format) const;

    class EventNotifyWatcher
    {
        // The implementation is inline below to reduce the function-call overhead.
        // The QThread pointer is cached because QObject::thread() is not cheap.
    public:
        EventNotifyWatcher(QObject *receiver, QEvent *event);
        ~EventNotifyWatcher();
    private:
        const QThread *m_thread = nullptr;
    };

private:
    static bool s_initialized;
    QSurfaceFormat::OpenGLContextProfile m_requestedOpenGLProfile = QSurfaceFormat::NoProfile;
    int m_requestedOpenGLMajorVersion = -1;
    int m_requestedOpenGLMinorVersion = -1;
};

inline SharedMain::EventNotifyWatcher::EventNotifyWatcher(QObject *receiver, QEvent *event)
{
    if (auto *wd = Watchdog::instance()) {
        if (wd->isActive()) {
            m_thread = receiver->thread();
            Q_ASSERT(m_thread);
            wd->eventCallback(m_thread, true, receiver, event);
        }
    }
}

inline SharedMain::EventNotifyWatcher::~EventNotifyWatcher()
{
    if (auto *wd = Watchdog::instance()) {
        if (wd->isActive() && m_thread)
            wd->eventCallback(m_thread, false, nullptr, nullptr);
    }
}

QT_END_NAMESPACE_AM

#endif // SHAREDMAIN_H
