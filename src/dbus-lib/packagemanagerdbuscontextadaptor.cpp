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

#include "packagemanagerdbuscontextadaptor.h"
#include "package.h"
#include "packagemanager.h"
#include "packagemanager_adaptor.h"
#include "dbuspolicy.h"
#include "exception.h"
#include "logging.h"


QT_BEGIN_NAMESPACE_AM

static QString taskStateToString(AsynchronousTask::TaskState state)
{
    const char *cstr = QMetaEnum::fromType<AsynchronousTask::TaskState>().valueToKey(state);
    return QString::fromUtf8(cstr);
}

PackageManagerDBusContextAdaptor::PackageManagerDBusContextAdaptor(PackageManager *pm)
    : AbstractDBusContextAdaptor(pm)
{
    m_adaptor = new PackageManagerAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

PackageManagerAdaptor::PackageManagerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto pm = PackageManager::instance();

    connect(pm, &PackageManager::taskBlockingUntilInstallationAcknowledge,
            this, &PackageManagerAdaptor::taskBlockingUntilInstallationAcknowledge);
    connect(pm, &PackageManager::taskFailed,
            this, &PackageManagerAdaptor::taskFailed);
    connect(pm, &PackageManager::taskFinished,
            this, &PackageManagerAdaptor::taskFinished);
    connect(pm, &PackageManager::taskProgressChanged,
            this, &PackageManagerAdaptor::taskProgressChanged);
    connect(pm, &PackageManager::taskRequestingInstallationAcknowledge,
            this, [this](const QString &taskId, Package *package,
                         const QVariantMap &packageExtraMetaData,
                         const QVariantMap &packageExtraSignedMetaData) {
        auto map = PackageManager::instance()->get(package);
        map.remove(qSL("package")); // cannot marshall QObject *
        emit taskRequestingInstallationAcknowledge(taskId, map, packageExtraMetaData,
                                                   packageExtraSignedMetaData);
    });
    connect(pm, &PackageManager::taskStarted,
            this, &PackageManagerAdaptor::taskStarted);
    connect(pm, &PackageManager::taskStateChanged,
            [this](const QString &taskId, AsynchronousTask::TaskState newState) {
                emit taskStateChanged(taskId, taskStateToString(newState));
            });
}

PackageManagerAdaptor::~PackageManagerAdaptor()
{ }

bool PackageManagerAdaptor::allowInstallationOfUnsignedPackages() const
{
    return PackageManager::instance()->allowInstallationOfUnsignedPackages();
}

bool PackageManagerAdaptor::applicationUserIdSeparation() const
{
    return PackageManager::instance()->isApplicationUserIdSeparationEnabled();
}

uint PackageManagerAdaptor::commonApplicationGroupId() const
{
    return PackageManager::instance()->commonApplicationGroupId();
}

bool PackageManagerAdaptor::developmentMode() const
{
    return PackageManager::instance()->developmentMode();
}

QDBusVariant PackageManagerAdaptor::installationLocation() const
{
    return QDBusVariant(PackageManager::instance()->installationLocation());
}

QDBusVariant PackageManagerAdaptor::documentLocation() const
{
    return QDBusVariant(PackageManager::instance()->documentLocation());
}

void PackageManagerAdaptor::acknowledgePackageInstallation(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(void)
    return PackageManager::instance()->acknowledgePackageInstallation(taskId);
}

bool PackageManagerAdaptor::cancelTask(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(bool)
    return PackageManager::instance()->cancelTask(taskId);
}

int PackageManagerAdaptor::compareVersions(const QString &version1, const QString &version2)
{
    AM_AUTHENTICATE_DBUS(int)
    return PackageManager::instance()->compareVersions(version1, version2);
}

QStringList PackageManagerAdaptor::packageIds()
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return PackageManager::instance()->packageIds();
}

QVariantMap PackageManagerAdaptor::get(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    auto map = PackageManager::instance()->get(id);
    map.remove(qSL("package")); // cannot marshall QObject *
    return map;
}

qlonglong PackageManagerAdaptor::installedPackageSize(const QString &packageId)
{
    AM_AUTHENTICATE_DBUS(qlonglong)
    return PackageManager::instance()->installedPackageSize(packageId);
}

QVariantMap PackageManagerAdaptor::installedPackageExtraMetaData(const QString &packageId)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    return PackageManager::instance()->installedPackageExtraMetaData(packageId);
}

QVariantMap PackageManagerAdaptor::installedPackageExtraSignedMetaData(const QString &packageId)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    return PackageManager::instance()->installedPackageExtraSignedMetaData(packageId);
}

QString PackageManagerAdaptor::removePackage(const QString &packageId, bool keepDocuments)
{
    return removePackage(packageId, keepDocuments, false);
}

QString PackageManagerAdaptor::removePackage(const QString &packageId, bool keepDocuments, bool force)
{
    AM_AUTHENTICATE_DBUS(QString)
    return PackageManager::instance()->removePackage(packageId, keepDocuments, force);
}

QString PackageManagerAdaptor::startPackageInstallation(const QString &sourceUrl)
{
    AM_AUTHENTICATE_DBUS(QString)
    return PackageManager::instance()->startPackageInstallation(sourceUrl);
}

QString PackageManagerAdaptor::taskState(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)
    return taskStateToString(PackageManager::instance()->taskState(taskId));
}

QString PackageManagerAdaptor::taskPackageId(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)
    return PackageManager::instance()->taskPackageId(taskId);
}

QStringList PackageManagerAdaptor::activeTaskIds()
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return PackageManager::instance()->activeTaskIds();
}
