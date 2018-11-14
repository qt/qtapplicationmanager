/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
#include <QtAppManCommon/error.h>
#include <QtAppManInstaller/installationlocation.h>
#include <QtAppManInstaller/asynchronoustask.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class ApplicationManager;
class ApplicationInstallerPrivate;
class SudoClient;


class ApplicationInstaller : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationInstaller")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationInstaller 1.0")

    // both are const on purpose - these should never change in a running system
    Q_PROPERTY(bool allowInstallationOfUnsignedPackages READ allowInstallationOfUnsignedPackages CONSTANT)
    Q_PROPERTY(bool developmentMode READ developmentMode CONSTANT)

    Q_PROPERTY(bool applicationUserIdSeparation READ isApplicationUserIdSeparationEnabled)
    Q_PROPERTY(uint commonApplicationGroupId READ commonApplicationGroupId)


public:
    Q_ENUMS(QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState))

    ~ApplicationInstaller();
    static ApplicationInstaller *createInstance(const QVector<InstallationLocation> &installationLocations,
                                                const QString &manifestDirPath, const QString &imageMountDirPath,
                                                const QString &hardwareId, QString *error);
    static ApplicationInstaller *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool developmentMode() const;
    void setDevelopmentMode(bool b);
    bool allowInstallationOfUnsignedPackages() const;
    void setAllowInstallationOfUnsignedPackages(bool b);
    QString hardwareId() const;

    bool isApplicationUserIdSeparationEnabled() const;
    uint commonApplicationGroupId() const;

    bool enableApplicationUserIdSeparation(uint minUserId, uint maxUserId, uint commonGroupId);

    // Ownership of QDir* stays with ApplicationInstaller
    // Never returns null
    const QDir *manifestDirectory() const;

    // Ownership of QDir* stays with ApplicationInstaller
    // Will return null if app img mounting is disabled.
    const QDir *applicationImageMountDirectory() const;

    bool setDBusPolicy(const QVariantMap &yamlFragment);
    void setCACertificates(const QList<QByteArray> &chainOfTrust);

    void cleanupBrokenInstallations() const Q_DECL_NOEXCEPT_EXPR(false);

    // InstallationLocation handling
    QVector<InstallationLocation> installationLocations() const;
    const InstallationLocation &defaultInstallationLocation() const;
    const InstallationLocation &installationLocationFromId(const QString &installationLocationId) const;
    const InstallationLocation &installationLocationFromApplication(const QString &id) const;

    // Q_SCRIPTABLEs are available via both QML and D-Bus
    Q_SCRIPTABLE QStringList installationLocationIds() const;
    Q_SCRIPTABLE QString installationLocationIdFromApplication(const QString &id) const;
    Q_SCRIPTABLE QVariantMap getInstallationLocation(const QString &installationLocationId) const;

    // all QString return values are task-ids
    QString startPackageInstallation(const QString &installationLocationId, const QUrl &sourceUrl);
    Q_SCRIPTABLE QString startPackageInstallation(const QString &installationLocationId, const QString &sourceUrl);
    Q_SCRIPTABLE void acknowledgePackageInstallation(const QString &taskId);
    Q_SCRIPTABLE QString removePackage(const QString &id, bool keepDocuments, bool force = false);

    Q_SCRIPTABLE AsynchronousTask::TaskState taskState(const QString &taskId) const;
    Q_SCRIPTABLE QString taskApplicationId(const QString &taskId) const;
    Q_SCRIPTABLE QStringList activeTaskIds() const;
    Q_SCRIPTABLE bool cancelTask(const QString &taskId);

    // convenience function for app-store implementations
    Q_SCRIPTABLE int compareVersions(const QString &version1, const QString &version2);
    Q_SCRIPTABLE bool validateDnsName(const QString &name, int minimumParts = 1);

    Q_SCRIPTABLE qint64 installedApplicationSize(const QString &id) const;
    Q_SCRIPTABLE QVariantMap installedApplicationExtraMetaData(const QString &id) const;
    Q_SCRIPTABLE QVariantMap installedApplicationExtraSignedMetaData(const QString &id) const;

    // these 4 functions are not really for installation/deinstallation, but
    // for correctly (un)mounting a package that was installed on a removable medium.
    Q_SCRIPTABLE bool doesPackageNeedActivation(const QString &id);
    Q_SCRIPTABLE bool isPackageActivated(const QString &id);
    Q_SCRIPTABLE bool activatePackage(const QString &id);
    Q_SCRIPTABLE bool deactivatePackage(const QString &id);

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

    Q_SCRIPTABLE void packageActivated(const QString &id, bool successful);
    Q_SCRIPTABLE void packageDeactivated(const QString &id, bool successful);

private slots:
    void executeNextTask();

private:
    void cleanupMounts() const;
    void triggerExecuteNextTask();
    QString enqueueTask(AsynchronousTask *task);
    void handleFailure(AsynchronousTask *task);

    QList<QByteArray> caCertificates() const;

    uint findUnusedUserId() const Q_DECL_NOEXCEPT_EXPR(false);

private:
    // Ownership of manifestDir and iamgeMountDir is passed to ApplicationInstaller
    ApplicationInstaller(const QVector<InstallationLocation> &installationLocations, QDir *manifestDir,
                         QDir *imageMountDir, const QString &hardwareId, QObject *parent);
    ApplicationInstaller(const ApplicationInstaller &);
    static ApplicationInstaller *s_instance;

    ApplicationInstallerPrivate *d;

    friend class InstallationTask;
    friend class DeinstallationTask;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState))
