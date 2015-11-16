/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QVariant>
#include <QUrl>
#include <QStringList>
#include <QDir>
#if defined(QT_DBUS_LIB)
#  include <QDBusContext>
#  include <QDBusConnectionInterface>
#endif

#include "installationlocation.h"
#include "error.h"

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

class ApplicationManager;
class ApplicationInstallerPrivate;
class AsynchronousTask;
class SudoClient;


class ApplicationInstaller : public QObject
#if defined(QT_DBUS_LIB)
        , protected QDBusContext
#endif
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.pelagicore.ApplicationInstaller")

    // both are const on purpose - these should never change in a running system
    Q_PROPERTY(bool allowInstallationOfUnsignedPackages READ allowInstallationOfUnsignedPackages CONSTANT)
    Q_PROPERTY(bool developmentMode READ developmentMode CONSTANT)

    Q_PROPERTY(bool applicationUserIdSeparation READ isApplicationUserIdSeparationEnabled)
    Q_PROPERTY(uint commonApplicationGroupId READ commonApplicationGroupId)


public:
    ~ApplicationInstaller();
    static ApplicationInstaller *createInstance(const QList<InstallationLocation> &installationLocations,
                                                const QDir &manifestDir, const QDir &imageMountDir,
                                                QString *error);
    static ApplicationInstaller *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool developmentMode() const;
    void setDevelopmentMode(bool b);
    bool allowInstallationOfUnsignedPackages() const;
    void setAllowInstallationOfUnsignedPackages(bool b);

    bool isApplicationUserIdSeparationEnabled() const;
    uint commonApplicationGroupId() const;

    bool enableApplicationUserIdSeparation(uint minUserId, uint maxUserId, uint commonGroupId);

    QDir manifestDirectory() const;
    QDir applicationImageMountDirectory() const;

    bool setDBusPolicy(const QVariantMap &yamlFragment);
    void setCACertificates(const QList<QByteArray> &chainOfTrust);

    void cleanupBrokenInstallations() const throw(Exception);

    Q_SCRIPTABLE bool checkCleanup();

    // InstallationLocation handling
    QList<InstallationLocation> installationLocations() const;
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

    Q_SCRIPTABLE QString taskState(const QString &taskId);
    Q_SCRIPTABLE bool cancelTask(const QString &taskId);

    // convenience function for app-store implementations
    Q_SCRIPTABLE int compareVersions(const QString &version1, const QString &version2);

    Q_SCRIPTABLE qint64 installedApplicationSize(const QString &id) const;

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
    Q_SCRIPTABLE void taskStateChanged(const QString &taskId, const QString &newState);

    // installation only
    Q_SCRIPTABLE void taskRequestingInstallationAcknowledge(const QString &taskId, const QVariantMap &applicationAsVariantMap);
    Q_SCRIPTABLE void taskBlockingUntilInstallationAcknowledge(const QString &taskId);

    Q_SCRIPTABLE void packageActivated(const QString &id, bool successful);
    Q_SCRIPTABLE void packageDeactivated(const QString &id, bool successful);

private slots:
    void executeNextTask();

private:
    void triggerExecuteNextTask();
    QString enqueueTask(AsynchronousTask *task);
    void handleFailure(AsynchronousTask *task);

    QList<QByteArray> caCertificates() const;

    uint findUnusedUserId() const throw(Exception);

private:
    ApplicationInstaller(const QList<InstallationLocation> &installationLocations, const QDir &manifestDir,
                         const QDir &imageMountDir, QObject *parent);
    ApplicationInstaller(const ApplicationInstaller &);
    static ApplicationInstaller *s_instance;

    ApplicationInstallerPrivate *d;

    friend class InstallationTask;
    friend class DeinstallationTask;
};
