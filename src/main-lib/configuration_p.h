// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CONFIGURATION_P_H
#define CONFIGURATION_P_H

#include <QtAppManCommon/global.h>
#include <QtCore/QCommandLineParser>

#include "configuration.h"
#include "configcache.h"


QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class ConfigurationPrivate
{
public:
    static quint32 dataStreamVersion();

    static void loadFromSource(QIODevice *source, const QString &fileName, ConfigurationData &data);
    static QByteArray substituteVars(const QByteArray &sourceContent, const QString &fileName);
    static void loadFromCache(QDataStream &ds, ConfigurationData &data);
    static void saveToCache(QDataStream &ds, const ConfigurationData &data);
    static void serialize(QDataStream &ds, ConfigurationData &data, bool write);
    static void merge(const ConfigurationData &from, ConfigurationData &into);

    friend class ConfigCacheAdaptor<ConfigurationData>;

    QStringList defaultConfigFilePaths;
    QString buildConfigFilePath;
    QCommandLineParser clp;
    ConfigurationData data;
    bool onlyOnePositionalArgument = false;
    bool forceVerbose = false;
    bool forceDisableWatchdog = false;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // CONFIGURATION_P_H
