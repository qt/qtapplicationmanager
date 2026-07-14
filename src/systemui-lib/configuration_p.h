// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CONFIGURATION_P_H
#define CONFIGURATION_P_H

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>
#include <QtCore/QCommandLineParser>

#include "configuration.h"


QT_BEGIN_NAMESPACE_AM

class ConfigurationPrivate
{
public:
    static void loadFromSource(const QByteArray &source, const QString &fileName, ConfigurationData &data);
    static QByteArray substituteVars(const QByteArray &sourceContent, const QString &fileName);
    static void merge(const ConfigurationData &from, ConfigurationData &into);

    QStringList defaultConfigFilePaths;
    QString buildConfigFilePath;
    QCommandLineParser clp;
    QList<QCommandLineOption> deprecatedOptions;
    ConfigurationData data;
    bool onlyOnePositionalArgument = false;
    bool forceVerbose = false;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // CONFIGURATION_P_H
