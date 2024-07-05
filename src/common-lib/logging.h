// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef LOGGING_H
#define LOGGING_H

#include <QtAppManCommon/global.h>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE_AM

#define QTAM_DUMMY_EXPORT // we're building static libraries, so no need to export symbols

QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogSystem, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogInstaller, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogGraphics, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogWaylandDebug, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogQml, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogNotifications, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogRuntime, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogQuickLaunch, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogQmlRuntime, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogDeployment, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogIntents, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogCache, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogDBus, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogWatchdogLive, QTAM_DUMMY_EXPORT)
QT_DECLARE_EXPORTED_QT_LOGGING_CATEGORY(LogWatchdogStat, QTAM_DUMMY_EXPORT)

#undef QTAM_DUMMY_EXPORT

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
    static bool isDltAvailable();
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

template <typename... Ts> void am_trace(QDebug dbg, Ts &&... ts)
{ (dbg << ... << ts); }

#define AM_TRACE(category, ...) \
    for (bool qt_category_enabled = category().isDebugEnabled(); qt_category_enabled; qt_category_enabled = false) { \
        QT_PREPEND_NAMESPACE_AM(am_trace(QMessageLogger(__FILE__, __LINE__, __FUNCTION__, category().categoryName()).debug(), "TRACE", __FUNCTION__, __VA_ARGS__)); \
    }

QT_END_NAMESPACE_AM

#endif // LOGGING_H
