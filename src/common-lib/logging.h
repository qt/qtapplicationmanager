// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QLoggingCategory>
#include <QMutex>

QT_BEGIN_NAMESPACE_AM

Q_DECLARE_LOGGING_CATEGORY(LogSystem)
Q_DECLARE_LOGGING_CATEGORY(LogInstaller)
Q_DECLARE_LOGGING_CATEGORY(LogGraphics)
Q_DECLARE_LOGGING_CATEGORY(LogWaylandDebug)
Q_DECLARE_LOGGING_CATEGORY(LogQml)
Q_DECLARE_LOGGING_CATEGORY(LogNotifications)
Q_DECLARE_LOGGING_CATEGORY(LogRuntime)
Q_DECLARE_LOGGING_CATEGORY(LogQmlRuntime)
Q_DECLARE_LOGGING_CATEGORY(LogDeployment)
Q_DECLARE_LOGGING_CATEGORY(LogIntents)
Q_DECLARE_LOGGING_CATEGORY(LogCache)

class Logging
{
public:
    static void initialize();
    static void initialize(int argc, const char * const *argv);

    static QStringList filterRules();
    static void setFilterRules(const QStringList &rules);

    static void setMessagePattern(const QString &pattern);

    static QVariant useAMConsoleLogger();
    static void setUseAMConsoleLogger(const QVariant &config);

    static bool hasDeferredMessages();
    static void completeSetup();

    static QByteArray applicationId();
    static void setApplicationId(const QByteArray &appId);

    // DLT functionality
    static bool isDltEnabled();
    static void setDltEnabled(bool enabled);

    static void setSystemUiDltId(const QByteArray &dltAppId, const QByteArray &dltAppDescription);
    static void setDltApplicationId(const QByteArray &dltAppId, const QByteArray &dltAppDescription);

    static void registerUnregisteredDltContexts();

    static QString dltLongMessageBehavior();
    static void setDltLongMessageBehavior(const QString &behaviorString);

    static void logToDlt(QtMsgType msgType, const QMessageLogContext &context, const QString &message);

private:
    static void messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message);
    static void deferredMessageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message);
};

void am_trace(QDebug);
template <typename T, typename... TRest> void am_trace(QDebug dbg, T t, TRest... trest)
{ dbg << t; am_trace(dbg, trest...); }

#define AM_TRACE(category, ...) \
    for (bool qt_category_enabled = category().isDebugEnabled(); qt_category_enabled; qt_category_enabled = false) { \
        QT_PREPEND_NAMESPACE_AM(am_trace(QMessageLogger(__FILE__, __LINE__, __FUNCTION__, category().categoryName()).debug(), "TRACE", __FUNCTION__, __VA_ARGS__)); \
    }

class Console
{
public:
    static bool ensureInitialized();

    static bool supportsAnsiColor();
    static bool isRunningInQtCreator();
    static bool hasConsoleWindow();
    static int width();

    enum Color { Off = 0, Black, Red, Green, Yellow, Blue, Magenta, Cyan, Gray, BrightFlag = 0x80 };
    static QByteArray &colorize(QByteArray &out, int color, bool forceNoColor = false);
};

QT_END_NAMESPACE_AM
