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
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QSet>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM


// IMPORTANT: if you add/remove/change anything in this struct, you also have to adjust the
//            loadFromCache(), saveToCache() and mergeFrom() functions in the cpp file!

struct ConfigurationData
{
    static const quint32 DataStreamVersion;

    static ConfigurationData *loadFromSource(QIODevice *source, const QString &fileName);
    static QByteArray substituteVars(const QByteArray &sourceContent, const QString &fileName);
    static ConfigurationData *loadFromCache(QDataStream &ds);
    void saveToCache(QDataStream &ds) const;
    void mergeFrom(const ConfigurationData *from);


    struct Runtimes {
        QVariantMap configurations;
    } runtimes;

    struct {
        QVariantMap configurations;
        QList<QPair<QString, QString>> selection;
    } containers;

    struct {
        bool disable = false;
        struct {
            int disambiguation = 10000;
            int startApplication = 3000;
            int replyFromApplication = 5000;
            int replyFromSystem = 20000;
        } timeouts;
    } intents;

    struct {
        QStringList startup;
        QStringList container;
    } plugins;

    struct {
        struct {
            QString id;
            QString description;
        } dlt;
        QStringList rules;
        QString messagePattern;
        QVariant useAMConsoleLogger; // true / false / invalid
    } logging;

    struct {
        bool disable = false;
        QStringList caCertificates;
        struct {
            int minUserId = -1;
            int maxUserId = -1;
            int commonGroupId = -1;
        } applicationUserIdSeparation;
    } installer;

    struct {
        QVariantMap policies;
        QVariantMap registrations;
    } dbus;

    struct {
        double idleLoad = 0.;
        int runtimesPerContainer = 0;
    } quicklaunch;

    struct {
        QVariantMap opengl;
        bool enableTouchEmulation = false;
        QStringList iconThemeSearchPaths;
        QString iconThemeName;
        QString style;
        bool loadDummyData = false;
        QStringList importPaths;
        QStringList pluginPaths;
        QString windowIcon;
        bool fullscreen = false;
        QString mainQml;
        QStringList resources;
    } ui;

    struct {
        QStringList builtinAppsManifestDir;
        QString installationDir;
        QString documentDir;
    } applications; // TODO: rename to package?

    QVariantList installationLocations;

    QVariantMap crashAction;
    QVariantMap systemProperties;

    struct {
        bool forceSingleProcess = false;
        bool forceMultiProcess = false;
        bool noSecurity = false;
        bool developmentMode = false;
        bool allowUnsignedPackages = false;
        bool noUiWatchdog = false;
    } flags;

    struct {
        QString socketName;
        QVariantList extraSockets;
    } wayland;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
