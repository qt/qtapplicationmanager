// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QMutex>
#include <QList>
#include <QSet>
#include <QThread>

#include <QtAppManManager/packagemanager.h>
#include <QtAppManApplication/packagedatabase.h>
#include <QtAppManManager/asynchronoustask.h>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/private/qtappman_common-config_p.h>

QT_BEGIN_NAMESPACE_AM

bool removeRecursiveHelper(const QString &path);

class PackageManagerPrivate
{
public:
    PackageDatabase *database = nullptr;
    QVector<Package *> packages;

    QMap<Package *, PackageInfo *> pendingPackageInfoUpdates;

    bool enableInstaller = false;
    bool developmentMode = false;
    bool allowInstallationOfUnsignedPackages = false;

    QString installationPath;
    QString documentPath;

    QString error;

    QString hardwareId;
    QByteArrayList chainOfTrust;
    bool cleanupBrokenInstallationsDone = false;

#if QT_CONFIG(am_installer)
    QList<AsynchronousTask *> incomingTaskList;     // incoming queue
    QList<AsynchronousTask *> installationTaskList; // installation jobs in state >= AwaitingAcknowledge
    AsynchronousTask *activeTask = nullptr;         // currently active

    QList<AsynchronousTask *> allTasks() const
    {
        QList<AsynchronousTask *> all = incomingTaskList;
        if (!installationTaskList.isEmpty())
            all += installationTaskList;
        if (activeTask)
            all += activeTask;
        return all;
    }
#endif
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
