// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QMutex>
#include <QList>
#include <QSet>
#include <QScopedPointer>
#include <QThread>

#include <QtAppManManager/packagemanager.h>
#include <QtAppManApplication/packagedatabase.h>
#include <QtAppManManager/asynchronoustask.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

bool removeRecursiveHelper(const QString &path);

class PackageManagerPrivate
{
public:
    PackageDatabase *database = nullptr;
    QVector<Package *> packages;

    QMap<Package *, PackageInfo *> pendingPackageInfoUpdates;

#if defined(AM_DISABLE_INSTALLER)
    static constexpr
#endif
    bool disableInstaller = true;

    bool developmentMode = false;
    bool allowInstallationOfUnsignedPackages = false;
    bool userIdSeparation = false;
    uint minUserId = uint(-1);
    uint maxUserId = uint(-1);
    uint commonGroupId = uint(-1);

    QString installationPath;
    QString documentPath;

    QString error;

    QString hardwareId;
    QList<QByteArray> chainOfTrust;
    bool cleanupBrokenInstallationsDone = false;

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
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
