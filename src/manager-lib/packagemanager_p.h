// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEMANAGER_P_H
#define PACKAGEMANAGER_P_H

#include <QMutex>
#include <QList>
#include <QSet>
#include <QThread>

#include <QtAppManManager/packagemanager.h>
#include <QtAppManApplication/packagedatabase.h>
#include <QtAppManManager/asynchronoustask.h>
#include <QtAppManCommon/global.h>
#include <QtAppManCrypto/signature.h>
#include <QtAppManCommon/private/qtappman_common-config_p.h>

QT_BEGIN_NAMESPACE_AM

class PackageManagerPrivate
{
public:
    PackageDatabase *database = nullptr;
    QVector<Package *> packages;
    bool aboutToBeRemoved = false;

    QMap<Package *, PackageInfo *> pendingPackageInfoUpdates;  // AXIVION Line Qt-QMapWithPointerKey: package is locked

    bool enableInstaller = false;
    PackageManager::DevelopmentMode developmentMode = PackageManager::DevelopmentMode::Disabled;
    Certificate developerCertificate;
    QByteArray developerSignature;
    bool allowInstallationOfUnsignedPackages = false;
    bool useSudoForDirectoryRemoval = false;
    bool configurationIsLocked = false;

    QString installationPath;
    QString documentPath;

    QString error;

    QString hardwareId;
    QByteArrayList caCertificatesCommon;
    QByteArrayList caCertificatesDeveloper;
    QByteArrayList caCertificatesStore;
    QByteArrayList certificateRevocationLists;
    QStringList issuerCertificateFingerprintsDeveloper;
    QStringList issuerCertificateFingerprintsStore;
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

#endif // PACKAGEMANAGER_P_H
