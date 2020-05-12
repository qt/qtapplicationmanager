/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#pragma once

#include <QObject>
#include <QVariant>
#include <QUrl>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/logging.h>
#include <QtAppManManager/applicationmanager.h>
#include <QtAppManManager/packagemanager.h>
#include <QtAppManManager/package.h>
#include <QtAppManManager/asynchronoustask.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM


class ApplicationInstaller : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationInstaller")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationInstaller 2.0 SINGLETON")

    // both are const on purpose - these should never change in a running system
    Q_PROPERTY(bool allowInstallationOfUnsignedPackages READ allowInstallationOfUnsignedPackages CONSTANT)
    Q_PROPERTY(bool developmentMode READ developmentMode CONSTANT)

    Q_PROPERTY(bool applicationUserIdSeparation READ isApplicationUserIdSeparationEnabled)
    Q_PROPERTY(uint commonApplicationGroupId READ commonApplicationGroupId)


public:
    Q_ENUMS(QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState))

    ~ApplicationInstaller();
    static ApplicationInstaller *createInstance(PackageManager *pm);
    static ApplicationInstaller *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool developmentMode() const { return m_pm->developmentMode(); }
    bool allowInstallationOfUnsignedPackages() const { return m_pm->allowInstallationOfUnsignedPackages(); }
    QString hardwareId() const { return m_pm->hardwareId(); }

    bool isApplicationUserIdSeparationEnabled() const { return m_pm->isApplicationUserIdSeparationEnabled(); }
    uint commonApplicationGroupId() const { return m_pm->commonApplicationGroupId(); }

    // Q_SCRIPTABLEs are available via both QML and D-Bus
    Q_SCRIPTABLE QStringList installationLocationIds() const { return { qL1S("internal-0") }; }
    Q_SCRIPTABLE QString installationLocationIdFromApplication(const QString &applicationId) const
    {
        auto app = ApplicationManager::instance()->fromId(applicationId);
        if (app && ((!app->package()->isBuiltIn() || app->package()->builtInHasRemovableUpdate())))
            return qL1S("internal-0");
        return QString();
    }
    Q_SCRIPTABLE QVariantMap getInstallationLocation(const QString &installationLocationId) const
    {
        if (installationLocationId != qL1S("internal-0"))
            return QVariantMap { };

        auto iloc = m_pm->installationLocation();
        auto dloc = m_pm->documentLocation();

        return QVariantMap {
            { qSL("id"), qSL("internal-0") },
            { qSL("type"), qSL("internal") },
            { qSL("index"), 0 },
            { qSL("isDefault"), true },
            { qSL("installationPath"), iloc.value(qL1S("path")) },
            { qSL("installationDeviceSize"), iloc.value(qL1S("size")) },
            { qSL("installationDeviceFree"), iloc.value(qL1S("free")) },
            { qSL("documentPath"), dloc.value(qL1S("path")) },
            { qSL("documentDeviceSize"), dloc.value(qL1S("size")) },
            { qSL("documentDeviceFree"), dloc.value(qL1S("free")) }
        };
    }

#if !defined(AM_DISABLE_INSTALLER)
    // all QString return values are task-ids
    Q_SCRIPTABLE QString startPackageInstallation(const QString &installationLocationId, const QString &sourceUrl)
    {
        if (installationLocationId == qL1S("internal-0")) {
            return m_pm->startPackageInstallation(sourceUrl);
        } else {
            qCWarning(LogInstaller) << "The only supported legacy installation location is 'internal-0', "
                                       "but an installation was requested to:" << installationLocationId;
            return QString();
        }
    }

    Q_SCRIPTABLE void acknowledgePackageInstallation(const QString &taskId)
    { return m_pm->acknowledgePackageInstallation(taskId); }
    Q_SCRIPTABLE QString removePackage(const QString &applicationId, bool keepDocuments, bool force = false)
    {
        auto app = ApplicationManager::instance()->fromId(applicationId);
        return m_pm->removePackage(app ? app->packageInfo()->id() : QString(), keepDocuments, force);
    }
    Q_SCRIPTABLE AsynchronousTask::TaskState taskState(const QString &taskId) const
    { return m_pm->taskState(taskId); }
    Q_SCRIPTABLE QString taskApplicationId(const QString &taskId) const
    {
        auto package = m_pm->fromId(m_pm->taskPackageId(taskId));
        if (package && !package->info()->applications().isEmpty())
            return package->info()->applications().constFirst()->id();
        return QString();
    }
    Q_SCRIPTABLE QStringList activeTaskIds() const
    { return m_pm->activeTaskIds(); }
    Q_SCRIPTABLE bool cancelTask(const QString &taskId)
    { return m_pm->cancelTask(taskId); }
#endif

    // convenience function for app-store implementations
    Q_SCRIPTABLE int compareVersions(const QString &version1, const QString &version2)
    { return m_pm->compareVersions(version1, version2); }
    Q_SCRIPTABLE bool validateDnsName(const QString &name, int minimumParts = 1)
    { return m_pm->validateDnsName(name, minimumParts); }

    Q_SCRIPTABLE qint64 installedApplicationSize(const QString &applicationId) const
    {
        auto app = ApplicationManager::instance()->fromId(applicationId);
        return m_pm->installedPackageSize(app ? app->package()->id() : QString());
    }
    Q_SCRIPTABLE QVariantMap installedApplicationExtraMetaData(const QString &applicationId) const
    {
        auto app = ApplicationManager::instance()->fromId(applicationId);
        return m_pm->installedPackageExtraMetaData(app ? app->package()->id() : QString());
    }
    Q_SCRIPTABLE QVariantMap installedApplicationExtraSignedMetaData(const QString &applicationId) const
    {
        auto app = ApplicationManager::instance()->fromId(applicationId);
        return m_pm->installedPackageExtraSignedMetaData(app ? app->package()->id() : QString());
    }

signals:
    Q_SCRIPTABLE void taskStarted(const QString &taskId);
    Q_SCRIPTABLE void taskProgressChanged(const QString &taskId, qreal progress);
    Q_SCRIPTABLE void taskFinished(const QString &taskId);
    Q_SCRIPTABLE void taskFailed(const QString &taskId, int errorCode, const QString &errorString);
    Q_SCRIPTABLE void taskStateChanged(const QString &taskId,
                                       QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState) newState);

    // installation only
    Q_SCRIPTABLE void taskRequestingInstallationAcknowledge(const QString &taskId,
                                                            const QVariantMap &applicationAsVariantMap,
                                                            const QVariantMap &packageExtraMetaData,
                                                            const QVariantMap &packageExtraSignedMetaData);
    Q_SCRIPTABLE void taskBlockingUntilInstallationAcknowledge(const QString &taskId);


private:
    PackageManager *m_pm;
    ApplicationInstaller(PackageManager *pm, QObject *parent = nullptr);
    ApplicationInstaller(const ApplicationInstaller &);
    static ApplicationInstaller *s_instance;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState))
