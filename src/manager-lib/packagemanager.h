// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QAbstractListModel>
#include <QtAppManCommon/global.h>
#include <QtAppManApplication/packageinfo.h>
#include <QtAppManManager/asynchronoustask.h>


QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class PackageDatabase;
class Package;
class PackageManagerPrivate;
class InstallationTask;
class DeinstallationTask;

// A place to collect signals used internally by appman without polluting
// PackageManager's public QML API.
class PackageManagerInternalSignals : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    // the slots connected to these signals are allowed to throw Exception objects, if the
    // connection is direct!
    void registerApplication(QtAM::ApplicationInfo *applicationInfo,
                             QtAM::Package *package);
    void unregisterApplication(QtAM::ApplicationInfo *applicationInfo,
                               QtAM::Package *package);

    void registerIntent(QtAM::IntentInfo *intentInfo,
                        QtAM::Package *package);
    void unregisterIntent(QtAM::IntentInfo *intentInfo,
                          QtAM::Package *package);
};

class PackageManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.PackageManager")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    // these are const on purpose - these should never change in a running system
    Q_PROPERTY(bool allowInstallationOfUnsignedPackages READ allowInstallationOfUnsignedPackages CONSTANT FINAL)
    Q_PROPERTY(bool developmentMode READ developmentMode CONSTANT FINAL)
    Q_PROPERTY(QString hardwareId READ hardwareId CONSTANT FINAL)
    Q_PROPERTY(QString architecture READ architecture CONSTANT FINAL)

    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged FINAL)
    Q_PROPERTY(QVariantMap installationLocation READ installationLocation CONSTANT FINAL)
    Q_PROPERTY(QVariantMap documentLocation READ documentLocation CONSTANT FINAL)

public:
    Q_ENUMS(QtAM::AsynchronousTask::TaskState)

    enum CacheMode {
        NoCache,
        UseCache,
        RecreateCache
    };

    ~PackageManager() override;
    static PackageManager *createInstance(PackageDatabase *packageDatabase,
                                          const QString &documentPath);
    static PackageManager *instance();

    void enableInstaller();
    void registerPackages();

    QVector<Package *> packages() const;

    Package *fromId(const QString &id) const;
    QVariantMap get(Package *package) const;

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant dataForRole(Package *package, int role) const;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QtAM::Package *package(int index) const;
    Q_INVOKABLE QtAM::Package *package(const QString &id) const;
    Q_INVOKABLE int indexOfPackage(const QString &id) const;

    bool isReady() const;

    bool developmentMode() const;
    void setDevelopmentMode(bool enable);
    bool allowInstallationOfUnsignedPackages() const;
    void setAllowInstallationOfUnsignedPackages(bool enable);
    QString hardwareId() const;
    void setHardwareId(const QString &hwId);
    QString architecture() const;
    void setCACertificates(const QByteArrayList &chainOfTrust);

    void cleanupBrokenInstallations() noexcept(false);

    QVariantMap installationLocation() const;
    QVariantMap documentLocation() const;

    bool isPackageInstallationActive(const QString &packageId) const;

    // Q_SCRIPTABLEs are available via both QML and D-Bus
    Q_SCRIPTABLE QStringList packageIds() const;
    Q_SCRIPTABLE QVariantMap get(const QString &packageId) const;

    Q_SCRIPTABLE qint64 installedPackageSize(const QString &packageId) const;
    Q_SCRIPTABLE QVariantMap installedPackageExtraMetaData(const QString &packageId) const;
    Q_SCRIPTABLE QVariantMap installedPackageExtraSignedMetaData(const QString &packageId) const;

    // all QString return values are task-ids
    QString startPackageInstallation(const QUrl &sourceUrl);
    Q_SCRIPTABLE QString startPackageInstallation(const QString &sourceUrl);
    Q_SCRIPTABLE void acknowledgePackageInstallation(const QString &taskId);
    Q_SCRIPTABLE QString removePackage(const QString &id, bool keepDocuments, bool force = false);

    Q_SCRIPTABLE QtAM::AsynchronousTask::TaskState taskState(const QString &taskId) const;
    Q_SCRIPTABLE QString taskPackageId(const QString &taskId) const;
    Q_SCRIPTABLE QStringList activeTaskIds() const;
    Q_SCRIPTABLE bool cancelTask(const QString &taskId);

    // convenience function for app-store implementations
    Q_SCRIPTABLE int compareVersions(const QString &version1, const QString &version2);
    Q_SCRIPTABLE bool validateDnsName(const QString &name, int minimumParts = 1);

    PackageManagerInternalSignals internalSignals;

Q_SIGNALS:
    Q_SCRIPTABLE void readyChanged(bool b);
    Q_SCRIPTABLE void countChanged();

    Q_SCRIPTABLE void packageAdded(const QString &id);
    Q_SCRIPTABLE void packageAboutToBeRemoved(const QString &id);
    Q_SCRIPTABLE void packageChanged(const QString &id, const QStringList &changedRoles);

    Q_SCRIPTABLE void taskStarted(const QString &taskId);
    Q_SCRIPTABLE void taskProgressChanged(const QString &taskId, qreal progress);
    Q_SCRIPTABLE void taskFinished(const QString &taskId);
    Q_SCRIPTABLE void taskFailed(const QString &taskId, int errorCode, const QString &errorString);
    Q_SCRIPTABLE void taskStateChanged(const QString &taskId,
                                       QtAM::AsynchronousTask::TaskState newState);

    // installation only
    void taskRequestingInstallationAcknowledge(const QString &taskId,
                                               QtAM::Package *package,
                                               const QVariantMap &packageExtraMetaData,
                                               const QVariantMap &packageExtraSignedMetaData);
    Q_SCRIPTABLE void taskBlockingUntilInstallationAcknowledge(const QString &taskId);

protected:
    Package *startingPackageInstallation(PackageInfo *info);
    bool startingPackageRemoval(const QString &id);
    bool finishedPackageInstall(const QString &id);
    bool canceledPackageInstall(const QString &id);

private:
    void executeNextTask();
    void triggerExecuteNextTask();
    QString enqueueTask(AsynchronousTask *task);
    void handleFailure(AsynchronousTask *task);

private:
    void emitDataChanged(Package *package, const QVector<int> &roles = QVector<int>());
    Package *registerPackage(PackageInfo *packageInfo, PackageInfo *updatedPackageInfo,
                             bool currentlyBeingInstalled = false);
    void registerApplicationsAndIntentsOfPackage(Package *package);
    void unregisterApplicationsAndIntentsOfPackage(Package *package);
    static void registerQmlTypes();
    QByteArrayList caCertificates() const;

private:
    explicit PackageManager(PackageDatabase *packageDatabase,
                            const QString &documentPath);
    PackageManager(const PackageManager &);
    PackageManager &operator=(const PackageManager &);
    static PackageManager *s_instance;
    static QHash<int, QByteArray> s_roleNames;

    PackageManagerPrivate *d;

    friend class InstallationTask;
    friend class DeinstallationTask;
};

QT_END_NAMESPACE_AM

#endif // PACKAGEMANAGER_H
