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
    static void messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message);
    static void deferredMessageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message);
    static QStringList filterRules();
    static void setFilterRules(const QStringList &rules);
    static void setMessagePattern(const QString &pattern);
    static QVariant useAMConsoleLogger();
    static void useAMConsoleLogger(const QVariant &config);
    static void completeSetup();

    static QByteArray applicationId();
    static void setApplicationId(const QByteArray &appId);

    static bool deferredMessages();

    // DLT functionality
    static bool isDltEnabled();
    static void setDltEnabled(bool enabled);

    static void registerUnregisteredDltContexts();
    static void setSystemUiDltId(const QByteArray &dltAppId, const QByteArray &dltAppDescription);
    static void setDltApplicationId(const QByteArray &dltAppId, const QByteArray &dltAppDescription);

    static void logToDlt(QtMsgType msgType, const QMessageLogContext &context, const QString &message);

private:
    static bool s_dltEnabled;
    static bool s_messagePatternDefined;
    static bool s_useAMConsoleLogger;
    static QStringList s_rules;
    static QtMessageHandler s_defaultQtHandler;
    static QByteArray s_applicationId;
    static QVariant s_useAMConsoleLoggerConfig;
    static QMutex s_deferredMessagesMutex;
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
    static void init();

    static bool supportsAnsiColor;
    static bool isRunningInQtCreator;
    static bool hasConsoleWindow;
    static int width();

    enum Color { Off = 0, Black, Red, Green, Yellow, Blue, Magenta, Cyan, Gray, BrightFlag = 0x80 };
    static QByteArray &colorize(QByteArray &out, int color, bool forceNoColor = false);

private:
    static QAtomicInt consoleWidthCached;
};

QT_END_NAMESPACE_AM
