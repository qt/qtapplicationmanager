/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include "packagemanagerdbuscontextadaptor.h"
#include "package.h"
#include "packagemanager.h"
#include "io.qt.packagemanager_adaptor.h"
#include "applicationmanager.h"
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

        const auto apps = package->applications(); // these are QObject * (legacy API)
        QVariantList appList;
        appList.reserve(apps.size());
        for (const auto *obj : apps) {
            QVariantMap app = ApplicationManager::instance()->get(obj->property("id").toString());
            app.remove(qSL("application"));  // cannot marshall QObject *
            appList.append(app);
        }
        map.insert(qSL("applications"), appList);

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
