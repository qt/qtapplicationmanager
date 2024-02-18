// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QStringList>
#include <QVariantMap>
#include <QJSValue>
#include <QSet>
#include <QtAppManCommon/global.h>
#include <QtAppManManager/applicationmanager.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerPrivate
{
public:
    bool securityChecksEnabled = true;
    bool singleProcess = true;
    bool shuttingDown = false;
    bool windowManagerCompositorReady = false;
    QVariantMap systemProperties;

    QVector<Application *> apps;

    QString currentLocale;
    QHash<int, QByteArray> roleNames;

    QList<QPair<QString, QString>> containerSelectionConfig;
    QJSValue containerSelectionFunction;

    QSet<QString> registeredMimeSchemes;
    struct OpenUrlRequest
    {
        QString requestId;
        QString urlStr;
        QString mimeTypeName;
        QStringList possibleAppIds;
    };

    QVector<OpenUrlRequest> openUrlRequests;

    ApplicationManagerPrivate();
    ~ApplicationManagerPrivate();
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
