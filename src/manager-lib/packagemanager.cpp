/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QMetaMethod>
#include <QQmlEngine>
#include <QVersionNumber>
#include "packagemanager.h"
#include "packagedatabase.h"
#include "packagemanager_p.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "package.h"
#include "logging.h"
#include "installationreport.h"
#include "exception.h"
#include "sudo.h"
#include "utilities.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <errno.h>
#  if defined(Q_OS_ANDROID)
#    include <sys/vfs.h>
#    define statvfs statfs
#  else
#    include <sys/statvfs.h>
#  endif
#endif

#include <memory>

/*!
    \qmltype PackageManager
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The package installation/removal/update part of the application manager.

    The PackageManager singleton type handles the package installation
    part of the application manager. It provides both a DBus and QML APIs for
    all of its functionality.

    \note Unlike the deprecated ApplicationInstaller class, the PackageManager singleton and its
          corresponding DBus API are always available. Disabling the installer functionality via the
          application manager's \l{Configuration} will just lead to package (de-) installations
          failing instantly.

    Please be aware that setting the \c{applications/installationDirMountPoint} configuration
    option might delay the initialization of the package database. In this case, make sure to check
    that the \l ready property is \c true before interacting with the PackageManager.

    The type is derived from \c QAbstractListModel, so it can be used directly as a model from QML.

    \target PackageManager Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c packageId
        \li string
        \li The unique ID of a package, represented as a string (e.g. \c Browser or
            \c com.pelagicore.music)
    \row
        \li \c name
        \li string
        \li The name of the package. If possible, already translated to the current locale.
    \row
        \li \c icon
        \li string
        \li The URL of the package's icon.
    \row
        \li \c isBlocked
        \li bool
        \li A boolean value that gets set when the application manager needs to block any
            application within this package from running: this is normally only the case while an
            update is being applied.
    \row
        \li \c isUpdating
        \li bool
        \li A boolean value indicating whether the package is currently being installed or updated.
            If \c true, the \c updateProgress can be used to track the actual progress.
    \row
        \li \c isRemovable
        \li bool
        \li A boolean value indicating whether this package is user-removable; \c true for all
            dynamically installed third party packages and \c false for all system packages.
    \row
        \li \c updateProgress
        \li real
        \li While \c isUpdating is \c true, querying this role returns the actual progress as a
            floating-point value in the \c 0.0 to \c 1.0 range.
    \row
        \li \c categories
        \li list<string>
        \li The categories this package is registered for via its meta-data file.
    \row
        \li \c version
        \li string
        \li The currently installed version of this package.
    \row
        \li \c package
        \li Package
        \li The underlying package object for quick access to the properties outside of a
            model delegate.
    \endtable

    \target Task States

    The following table describes all possible states that a background task could be in:

    \table
    \header
        \li Task State
        \li Description
    \row
        \li \c Queued
        \li The task was created and is now queued up for execution.
    \row
        \li \c Executing
        \li The task is being executed.
    \row
        \li \c Finished
        \li The task was executed successfully.
    \row
        \li \c Failed
        \li The task failed to execute successfully.
    \row
        \li \c AwaitingAcknowledge
        \li \e{Installation tasks only!} The task is currently halted, waiting for either
            acknowledgePackageInstallation() or cancelTask() to continue. See startPackageInstallation()
            for more information on the installation workflow.
    \row
        \li \c Installing
        \li \e{Installation tasks only!} The installation was acknowledged via acknowledgePackageInstallation()
            and the final installation phase is now running.
    \row
        \li \c CleaningUp
        \li \e{Installation tasks only!} The installation has finished, and previous installations as
            well as temporary files are being cleaned up.
    \endtable

    The normal workflow for tasks is: \c Queued \unicode{0x2192} \c Executing \unicode{0x2192} \c
    Finished. The task can enter the \c Failed state at any point though - either by being canceled via
    cancelTask() or simply by failing due to an error.

    Installation tasks are a bit more complex due to the acknowledgment: \c Queued \unicode{0x2192}
    \c Executing \unicode{0x2192} \c AwaitingAcknowledge (this state may be skipped if
    acknowledgePackageInstallation() was called already) \unicode{0x2192} \c Installing
    \unicode{0x2192} \c Cleanup \unicode{0x2192} \c Finished. Again, the task can fail at any point.
*/

//TODO: THIS IS MISSING AN EXAMPLE!

/*!
    \qmlsignal PackageManager::taskStateChanged(string taskId, string newState)

    This signal is emitted when the state of the task identified by \a taskId changes. The
    new state is supplied in the parameter \a newState.

    \sa taskState()
*/

/*!
    \qmlsignal PackageManager::taskStarted(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Executing state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal PackageManager::taskFinished(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Finished state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal PackageManager::taskFailed(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Failed state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal PackageManager::taskRequestingInstallationAcknowledge(string taskId, PackageObject package, object packageExtraMetaData, object packageExtraSignedMetaData)

    This signal is emitted when the installation task identified by \a taskId has received enough
    meta-data to be able to emit this signal. The task may be in either \c Executing or \c
    AwaitingAcknowledge state.

    A temporary PackageObject is supplied via \a package. Please note, that this object is just
    constructed on the fly for this signal emission and is not part of the PackageManager model.
    The package object is destroyed again after the signal callback returns. Another permanent
    PackageObject that is also part of the model will be constructed later in the installation
    process.

    In addition, the package's extra meta-data (signed and unsinged) is also supplied via \a
    packageExtraMetaData and \a packageExtraSignedMetaData respectively as JavaScript objects.
    Both these objects are optional and need to be explicitly either populated during an
    application's packaging step or added by an intermediary app-store server.
    By default, both will just be empty.

    Following this signal, either cancelTask() or acknowledgePackageInstallation() has to be called
    for this \a taskId, to either cancel the installation or try to complete it.

    The PackageManager has two convenience functions to help the System UI with verifying the
    meta-data: compareVersions() and, in case you are using reverse-DNS notation for application-ids,
    validateDnsName().

    \sa taskStateChanged(), startPackageInstallation()
*/

/*!
    \qmlsignal PackageManager::taskBlockingUntilInstallationAcknowledge(string taskId)

    This signal is emitted when the installation task identified by \a taskId cannot continue
    due to a missing acknowledgePackageInstallation() call for the task.

    \sa taskStateChanged(), acknowledgePackageInstallation()
*/

/*!
    \qmlsignal PackageManager::taskProgressChanged(string taskId, qreal progress)

    This signal is emitted whenever the task identified by \a taskId makes progress towards its
    completion. The \a progress is reported as a floating-point number ranging from \c 0.0 to \c 1.0.

    \sa taskStateChanged()
*/

QT_BEGIN_NAMESPACE_AM

enum Roles
{
    Id = Qt::UserRole,
    Name,
    Description,
    Icon,

    IsBlocked,
    IsUpdating,
    IsRemovable,

    UpdateProgress,

    Version,
    PackageItem,
};

PackageManager *PackageManager::s_instance = nullptr;
QHash<int, QByteArray> PackageManager::s_roleNames;

PackageManager *PackageManager::createInstance(PackageDatabase *packageDatabase,
                                               const QString &documentPath)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("PackageManager::createInstance() was called a second time.");

    Q_ASSERT(packageDatabase);

    std::unique_ptr<PackageManager> pm(new PackageManager(packageDatabase, documentPath));
    registerQmlTypes();

    return s_instance = pm.release();
}

PackageManager *PackageManager::instance()
{
    if (!s_instance)
        qFatal("PackageManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *PackageManager::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void PackageManager::enableInstaller()
{
#if !defined(AM_DISABLE_INSTALLER)
    d->disableInstaller = false;
#endif
}

void PackageManager::registerPackages()
{
    qCDebug(LogSystem) << "Registering packages:";

    // collect all updates to builtin first, so we can avoid re-creating a lot of objects,
    // if we find an update to a builtin app later on
    QMap<QString, QPair<PackageInfo *, PackageInfo *>> pkgs;

    // map all the built-in packages first
    const auto builtinPackages = d->database->builtInPackages();
    for (auto packageInfo : builtinPackages) {
        auto existingPackageInfos = pkgs.value(packageInfo->id());
        if (existingPackageInfos.first) {
            throw Exception(Error::Package, "Found more than one built-in package with id '%1': here: %2 and there: %3")
                    .arg(packageInfo->id())
                    .arg(existingPackageInfos.first->manifestPath())
                    .arg(packageInfo->manifestPath());
        }
        pkgs.insert(packageInfo->id(), qMakePair(packageInfo, nullptr));
    }

    // next, map all the installed packages, making sure to detect updates to built-in ones
    const auto installedPackages = d->database->installedPackages();
    for (auto packageInfo : installedPackages) {
        auto existingPackageInfos = pkgs.value(packageInfo->id());
        if (existingPackageInfos.first) {
            if (existingPackageInfos.first->isBuiltIn()) { // update
                if (existingPackageInfos.second) { // but there already is an update applied!?
                    throw Exception(Error::Package, "Found more than one update for the built-in package with id '%1' here: %2 and there: %3")
                            .arg(packageInfo->id())
                            .arg(existingPackageInfos.second->manifestPath())
                            .arg(packageInfo->manifestPath());
                }
                pkgs[packageInfo->id()] = qMakePair(existingPackageInfos.first, packageInfo);

            } else {
                throw Exception(Error::Package, "Found more than one installed package with the same id '%1' here: %2 and there: %3")
                        .arg(packageInfo->id())
                        .arg(existingPackageInfos.first->manifestPath())
                        .arg(packageInfo->manifestPath());
            }
        } else {
            pkgs.insert(packageInfo->id(), qMakePair(packageInfo, nullptr));
        }
    }
    for (auto it = pkgs.constBegin(); it != pkgs.constEnd(); ++it)
        registerPackage(it.value().first, it.value().second);

    // now that we have a consistent pkg db, we can clean up the installed packages
    cleanupBrokenInstallations();

    emit readyChanged(d->cleanupBrokenInstallationsDone);

#if !defined(AM_DISABLE_INSTALLER)
    // something might have been queued already before the cleanup had finished
    triggerExecuteNextTask();
#endif
}

Package *PackageManager::registerPackage(PackageInfo *packageInfo, PackageInfo *updatedPackageInfo,
                                         bool currentlyBeingInstalled)
{
    auto *package = new Package(packageInfo, currentlyBeingInstalled ? Package::BeingInstalled
                                                                     : Package::Installed);
    if (updatedPackageInfo)
        package->setUpdatedInfo(updatedPackageInfo);

    QQmlEngine::setObjectOwnership(package, QQmlEngine::CppOwnership);

    if (currentlyBeingInstalled) {
        Q_ASSERT(package->isBlocked());

        beginInsertRows(QModelIndex(), d->packages.count(), d->packages.count());
        qCDebug(LogSystem) << "Installing package:";
    }

    d->packages << package;

    qCDebug(LogSystem).nospace().noquote() << " + package: " << package->id() << " [at: "
                                           << QDir().relativeFilePath(package->info()->baseDir().path()) << "]";

    if (currentlyBeingInstalled) {
        endInsertRows();
        emitDataChanged(package);
    }

    emit packageAdded(package->id());

    if (!currentlyBeingInstalled)
        registerApplicationsAndIntentsOfPackage(package);

    return package;
}

void PackageManager::registerApplicationsAndIntentsOfPackage(Package *package)
{
    const auto appInfos = package->info()->applications();
    for (auto appInfo : appInfos) {
        try {
            emit internalSignals.registerApplication(appInfo, package);
        } catch (const Exception &e) {
            qCWarning(LogSystem) << "Cannot register application" << appInfo->id() << ":"
                                 << e.errorString();
        }
    }

    const auto intentInfos = package->info()->intents();
    for (auto intentInfo : intentInfos) {
        try {
            emit internalSignals.registerIntent(intentInfo, package);
        } catch (const Exception &e) {
            qCWarning(LogSystem) << "Cannot register intent" << intentInfo->id() << ":"
                                 << e.errorString();
        }
    }
}

void PackageManager::unregisterApplicationsAndIntentsOfPackage(Package *package)
{
    const auto intentInfos = package->info()->intents();
    for (auto intentInfo : intentInfos) {
        try {
            emit internalSignals.unregisterIntent(intentInfo, package); // not throwing ATM
        } catch (const Exception &e) {
            qCWarning(LogSystem) << "Cannot unregister intent" << intentInfo->id() << ":"
                                 << e.errorString();
        }
    }

    const auto appInfos = package->info()->applications();
    for (auto appInfo : appInfos) {
        try {
            emit internalSignals.unregisterApplication(appInfo, package); // not throwing ATM
        } catch (const Exception &e) {
            qCWarning(LogSystem) << "Cannot unregister application" << appInfo->id() << ":"
                                 << e.errorString();
        }
    }
}

QVector<Package *> PackageManager::packages() const
{
    return d->packages;
}

void PackageManager::registerQmlTypes()
{
    qmlRegisterSingletonType<PackageManager>("QtApplicationManager.SystemUI", 2, 0, "PackageManager",
                                             &PackageManager::instanceForQml);
    qmlRegisterUncreatableType<Package>("QtApplicationManager.SystemUI", 2, 0, "PackageObject",
                                        qSL("Cannot create objects of type PackageObject"));
    qRegisterMetaType<Package *>("Package*");

    s_roleNames.insert(Id, "packageId");
    s_roleNames.insert(Name, "name");
    s_roleNames.insert(Description, "description");
    s_roleNames.insert(Icon, "icon");
    s_roleNames.insert(IsBlocked, "isBlocked");
    s_roleNames.insert(IsUpdating, "isUpdating");
    s_roleNames.insert(IsRemovable, "isRemovable");
    s_roleNames.insert(UpdateProgress, "updateProgress");
    s_roleNames.insert(Version, "version");
    s_roleNames.insert(PackageItem, "package");
}

PackageManager::PackageManager(PackageDatabase *packageDatabase,
                               const QString &documentPath)
    : QAbstractListModel()
    , d(new PackageManagerPrivate())
{
    d->database = packageDatabase;
    d->installationPath = packageDatabase->installedPackagesDir();
    d->documentPath = documentPath;
}

PackageManager::~PackageManager()
{
    qDeleteAll(d->packages);
    delete d->database;
    delete d;
    s_instance = nullptr;
}

Package *PackageManager::fromId(const QString &id) const
{
    for (auto package : d->packages) {
        if (package->id() == id)
            return package;
    }
    return nullptr;
}

QVariantMap PackageManager::get(Package *package) const
{
    QVariantMap map;
    if (package) {
        QHash<int, QByteArray> roles = roleNames();
        for (auto it = roles.begin(); it != roles.end(); ++it)
            map.insert(qL1S(it.value()), dataForRole(package, it.key()));
    }
    return map;
}

void PackageManager::emitDataChanged(Package *package, const QVector<int> &roles)
{
    int row = d->packages.indexOf(package);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), roles);

        static const auto pkgChanged = QMetaMethod::fromSignal(&PackageManager::packageChanged);
        if (isSignalConnected(pkgChanged)) {
            QStringList stringRoles;
            for (auto role : roles)
                stringRoles << qL1S(s_roleNames[role]);
            emit packageChanged(package->id(), stringRoles);
        }
    }
}

// item model part

int PackageManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->packages.count();
}

QVariant PackageManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    Package *package = d->packages.at(index.row());
    return dataForRole(package, role);
}

QVariant PackageManager::dataForRole(Package *package, int role) const
{
    switch (role) {
    case Id:
        return package->id();
    case Name:
        return package->name();
    case Description:
        return package->description();
    case Icon:
        return package->icon();
    case IsBlocked:
        return package->isBlocked();
    case IsUpdating:
        return package->state() != Package::Installed;
    case UpdateProgress:
        return package->progress();
    case IsRemovable:
        return !package->isBuiltIn();
    case Version:
        return package->version();
    case PackageItem:
        return QVariant::fromValue(package);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PackageManager::roleNames() const
{
    return s_roleNames;
}

int PackageManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object PackageManager::get(int index)

    Retrieves the model data at \a index as a JavaScript object. See the
    \l {PackageManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a index is invalid.

    \note This is very inefficient if you only want to access a single property from QML; use
          package() instead to access the PackageObject's properties directly.
*/
QVariantMap PackageManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "PackageManager::get(index): invalid index:" << index;
        return QVariantMap();
    }
    return get(d->packages.at(index));
}

/*!
    \qmlmethod PackageObject PackageManager::package(int index)

    Returns the PackageObject corresponding to the given \a index in the model, or \c null if the
    index is invalid.

    \note The object ownership of the returned PackageObject stays with the application manager.
          If you want to store this pointer, you can use the PackageManager's QAbstractListModel
          signals or the packageAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Package *PackageManager::package(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "PackageManager::package(index): invalid index:" << index;
        return nullptr;
    }
    return d->packages.at(index);
}

/*!
    \qmlmethod PackageObject PackageManager::package(string id)

    Returns the PackageObject corresponding to the given package \a id, or \c null if the id does
    not exist.

    \note The object ownership of the returned PackageObject stays with the application manager.
          If you want to store this pointer, you can use the PackageManager's QAbstractListModel
          signals or the packageAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Package *PackageManager::package(const QString &id) const
{
    auto index = indexOfPackage(id);
    return (index < 0) ? nullptr : package(index);
}

/*!
    \qmlmethod int PackageManager::indexOfPackage(string id)

    Maps the package \a id to its position within the model.

    Returns \c -1 if the specified \a id is invalid.
*/
int PackageManager::indexOfPackage(const QString &id) const
{
    for (int i = 0; i < d->packages.size(); ++i) {
        if (d->packages.at(i)->id() == id)
            return i;
    }
    return -1;
}

/*!
    \qmlproperty bool PackageManager::ready

    Loading the package database might be delayed at startup if the
    \c{applications/installationDirMountPoint} configuration option is set.

    If your system is relying on this behavior, you should always check if the \l ready property is
    \c true before accessing information about installed packages.
    \note Calls to startPackageInstallation() and removePackage() while ready is still \c false
          will be queued and executed once the package database is fully loaded.
*/
bool PackageManager::isReady() const
{
    return d->cleanupBrokenInstallationsDone;
}

bool PackageManager::developmentMode() const
{
    return d->developmentMode;
}

void PackageManager::setDevelopmentMode(bool enable)
{
    d->developmentMode = enable;
}

bool PackageManager::allowInstallationOfUnsignedPackages() const
{
    return d->allowInstallationOfUnsignedPackages;
}

void PackageManager::setAllowInstallationOfUnsignedPackages(bool enable)
{
    d->allowInstallationOfUnsignedPackages = enable;
}

QString PackageManager::hardwareId() const
{
    return d->hardwareId;
}

void PackageManager::setHardwareId(const QString &hwId)
{
    d->hardwareId = hwId;
}

bool PackageManager::isApplicationUserIdSeparationEnabled() const
{
    return d->userIdSeparation;
}

uint PackageManager::commonApplicationGroupId() const
{
    return d->commonGroupId;
}

bool PackageManager::enableApplicationUserIdSeparation(uint minUserId, uint maxUserId, uint commonGroupId)
{
    if (minUserId >= maxUserId || minUserId == uint(-1) || maxUserId == uint(-1) || commonGroupId == uint(-1))
        return false;
#if !defined(AM_DISABLE_INSTALLER)
    d->userIdSeparation = true;
    d->minUserId = minUserId;
    d->maxUserId = maxUserId;
    d->commonGroupId = commonGroupId;
    return true;
#else
    return false;
#endif
}

uint PackageManager::findUnusedUserId() const Q_DECL_NOEXCEPT_EXPR(false)
{
    if (!isApplicationUserIdSeparationEnabled())
        return uint(-1);

    for (uint uid = d->minUserId; uid <= d->maxUserId; ++uid) {
        bool match = false;
        for (Package *package : d->packages) {
            if (package->info()->uid() == uid) {
                match = true;
                break;
            }
        }
        if (!match)
            return uid;
    }
    throw Exception("could not find a free user-id for application separation in the range %1 to %2")
            .arg(d->minUserId).arg(d->maxUserId);
}

QList<QByteArray> PackageManager::caCertificates() const
{
    return d->chainOfTrust;
}

void PackageManager::setCACertificates(const QList<QByteArray> &chainOfTrust)
{
    d->chainOfTrust = chainOfTrust;
}

static QVariantMap locationMap(const QString &path)
{
    QString cpath = QFileInfo(path).canonicalPath();
    quint64 bytesTotal = 0;
    quint64 bytesFree = 0;

#if defined(Q_OS_WIN)
    GetDiskFreeSpaceExW((LPCWSTR) cpath.utf16(), (ULARGE_INTEGER *) &bytesFree,
                        (ULARGE_INTEGER *) &bytesTotal, nullptr);

#else // Q_OS_UNIX
    int result;
    struct ::statvfs svfs;

    do {
        result = ::statvfs(cpath.toLocal8Bit(), &svfs);
        if (result == -1 && errno == EINTR)
            continue;
    } while (false);

    if (result == 0) {
        bytesTotal = quint64(svfs.f_frsize) * svfs.f_blocks;
        bytesFree = quint64(svfs.f_frsize) * svfs.f_bavail;
    }
#endif // Q_OS_WIN


    return QVariantMap {
        { qSL("path"), path },
        { qSL("deviceSize"), bytesTotal },
        { qSL("deviceFree"), bytesFree }
    };
}

/*!
    \qmlproperty object PackageManager::installationLocation

    Returns an object describing the location under which packages are installed in detail.

    The returned object has the following members:

    \table
    \header
        \li \c Name
        \li \c Type
        \li Description
    \row
        \li \c path
        \li \c string
        \li The absolute file-system path to the base directory.
    \row
        \li \c deviceSize
        \li \c int
        \li The size of the device holding \c path in bytes.
    \row
        \li \c deviceFree
        \li \c int
        \li The amount of bytes available on the device holding \c path.
    \endtable

    Returns an empty object in case the installer component is disabled.
*/
QVariantMap PackageManager::installationLocation() const
{
    return locationMap(d->installationPath);
}

/*!
    \qmlproperty object PackageManager::documentLocation

    Returns an object describing the location under which per-user document
    directories are created in detail.

    The returned object has the same members as described in PackageManager::installationLocation.
*/
QVariantMap PackageManager::documentLocation() const
{
    return d->documentPath.isEmpty() ? QVariantMap { } : locationMap(d->documentPath);
}

bool PackageManager::isPackageInstallationActive(const QString &packageId) const
{
    for (const auto *t : qAsConst(d->installationTaskList)) {
        if (t->packageId() == packageId)
            return true;
    }
    return false;
}

void PackageManager::cleanupBrokenInstallations() Q_DECL_NOEXCEPT_EXPR(false)
{
    if (d->cleanupBrokenInstallationsDone)
        return;

#if !defined(AM_DISABLE_INSTALLER)
    // Check that everything in the app-db is available
    //    -> if not, remove from app-db

    // key: baseDirPath, value: subDirName/ or fileName
    QMultiMap<QString, QString> validPaths;
    if (!d->documentPath.isEmpty())
        validPaths.insert(d->documentPath, QString());
    if (!d->installationPath.isEmpty())
        validPaths.insert(d->installationPath, QString());

    for (Package *pkg : d->packages) { // we want to detach here!
        const InstallationReport *ir = pkg->info()->installationReport();
        if (ir) {
            bool valid = true;

            QString pkgDir = d->installationPath + QDir::separator() + pkg->id();
            QStringList checkDirs;
            QStringList checkFiles;

            checkFiles << pkgDir + qSL("/info.yaml");
            checkFiles << pkgDir + qSL("/.installation-report.yaml");
            checkDirs << pkgDir;

            for (const QString &checkFile : qAsConst(checkFiles)) {
                QFileInfo fi(checkFile);
                if (!fi.exists() || !fi.isFile() || !fi.isReadable()) {
                    valid = false;
                    qCDebug(LogInstaller) << "cleanup: uninstalling" << pkg->id() << "- file missing:" << checkFile;
                    break;
                }
            }
            for (const QString &checkDir : checkDirs) {
                QFileInfo fi(checkDir);
                if (!fi.exists() || !fi.isDir() || !fi.isReadable()) {
                    valid = false;
                    qCDebug(LogInstaller) << "cleanup: uninstalling" << pkg->id() << "- directory missing:" << checkDir;
                    break;
                }
            }

            if (valid) {
                validPaths.insert(d->installationPath, pkg->id() + QDir::separator());
                if (!d->documentPath.isEmpty())
                    validPaths.insert(d->documentPath, pkg->id() + QDir::separator());
            } else {
                if (startingPackageRemoval(pkg->id())) {
                    if (finishedPackageInstall(pkg->id()))
                        continue;
                }
                throw Exception(Error::Package, "could not remove broken installation of package %1 from database").arg(pkg->id());
            }
        }
    }

    // Remove everything that is not referenced from the app-db

    for (auto it = validPaths.cbegin(); it != validPaths.cend(); ) {
        const QString currentDir = it.key();

        // collect all values for the unique key currentDir
        QVector<QString> validNames;
        for ( ; it != validPaths.cend() && it.key() == currentDir; ++it)
            validNames << it.value();

        const QFileInfoList &dirEntries = QDir(currentDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

        // check if there is anything in the filesystem that is NOT listed in the validNames
        for (const QFileInfo &fi : dirEntries) {
            QString name = fi.fileName();
            if (fi.isDir())
                name.append(QDir::separator());

            if ((!fi.isDir() && !fi.isFile()) || !validNames.contains(name)) {
                qCDebug(LogInstaller) << "cleanup: removing unreferenced inode" << name;

                if (SudoClient::instance()) {
                    if (!SudoClient::instance()->removeRecursive(fi.absoluteFilePath())) {
                        throw Exception(Error::IO, "could not remove broken installation leftover %1: %2")
                            .arg(fi.absoluteFilePath()).arg(SudoClient::instance()->lastError());
                    }
                } else {
                    if (!recursiveOperation(fi.absoluteFilePath(), safeRemove)) {
                        throw Exception(Error::IO, "could not remove broken installation leftover %1 (maybe due to missing root privileges)")
                            .arg(fi.absoluteFilePath());
                    }
                }
            }
        }
    }
#endif // !defined(AM_DISABLE_INSTALLER)

    d->cleanupBrokenInstallationsDone = true;
}

/*!
    \qmlmethod list<string> PackageManager::packageIds()

    Returns a list of all available package ids. This can be used to further query for specific
    information via get().
*/
QStringList PackageManager::packageIds() const
{
    QStringList ids;
    ids.reserve(d->packages.size());
    for (int i = 0; i < d->packages.size(); ++i)
        ids << d->packages.at(i)->id();
    return ids;
}

/*!
    \qmlmethod object PackageManager::get(string packageId)

    Retrieves the model data for the package identified by \a packageId as a JavaScript object.
    See the \l {PackageManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a packageId is invalid.
*/
QVariantMap PackageManager::get(const QString &packageId) const
{
    return get(package(packageId));
}

/*!
   \qmlmethod int PackageManager::installedPackageSize(string packageId)

   Returns the size in bytes that the package identified by \a packageId is occupying on the storage
   device.

   Returns \c -1 in case the package \a packageId is not valid, or the package is not installed.
*/
qint64 PackageManager::installedPackageSize(const QString &packageId) const
{
    if (Package *package = fromId(packageId)) {
        if (const InstallationReport *report = package->info()->installationReport())
            return static_cast<qint64>(report->diskSpaceUsed());
    }
    return -1;
}

/*!
   \qmlmethod var PackageManager::installedPackageExtraMetaData(string packageId)

   Returns a map of all extra metadata in the package header of the package identified by \a packageId.

   Returns an empty map in case the package \a packageId is not valid, or the package is not installed.
*/
QVariantMap PackageManager::installedPackageExtraMetaData(const QString &packageId) const
{
    if (Package *package = fromId(packageId)) {
        if (const InstallationReport *report = package->info()->installationReport())
            return report->extraMetaData();
    }
    return QVariantMap();
}

/*!
   \qmlmethod var PackageManager::installedPackageExtraSignedMetaData(string packageId)

   Returns a map of all signed extra metadata in the package header of the package identified
   by \a packageId.

   Returns an empty map in case the package \a packageId is not valid, or the package is not installed.
*/
QVariantMap PackageManager::installedPackageExtraSignedMetaData(const QString &packageId) const
{
    if (Package *package = fromId(packageId)) {
        if (const InstallationReport *report = package->info()->installationReport())
            return report->extraSignedMetaData();
    }
    return QVariantMap();
}

/*! \internal
  Type safe convenience function, since DBus does not like QUrl
*/
QString PackageManager::startPackageInstallation(const QUrl &sourceUrl)
{
    AM_TRACE(LogInstaller, sourceUrl)

#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller)
        return enqueueTask(new InstallationTask(d->installationPath, d->documentPath, sourceUrl));
#endif
    return QString();
}

/*!
    \qmlmethod string PackageManager::startPackageInstallation(string sourceUrl)

    Downloads an application package from \a sourceUrl and installs it.

    The actual download and installation will happen asynchronously in the background. The
    PackageManager emits the signals \l taskStarted, \l taskProgressChanged, \l
    taskRequestingInstallationAcknowledge, \l taskFinished, \l taskFailed, and \l taskStateChanged
    for the returned taskId when applicable.

    \note Simply calling this function is not enough to complete a package installation: The
    taskRequestingInstallationAcknowledge() signal needs to be connected to a slot where the
    supplied package meta-data can be validated (either programmatically or by asking the user).
    If the validation is successful, the installation can be completed by calling
    acknowledgePackageInstallation() or, if the validation was unsuccessful, the installation should
    be canceled by calling cancelTask().
    Failing to do one or the other will leave an unfinished "zombie" installation.

    Returns a unique \c taskId. This can also be an empty string, if the task could not be
    created (in this case, no signals will be emitted).
*/
QString PackageManager::startPackageInstallation(const QString &sourceUrl)
{
    QUrl url(sourceUrl);
    if (url.scheme().isEmpty())
        url = QUrl::fromLocalFile(sourceUrl);
    return startPackageInstallation(url);
}

/*!
    \qmlmethod void PackageManager::acknowledgePackageInstallation(string taskId)

    Calling this function enables the installer to complete the installation task identified by \a
    taskId. Normally, this function is called after receiving the taskRequestingInstallationAcknowledge()
    signal, and the user and/or the program logic decided to proceed with the installation.

    \sa startPackageInstallation()
 */
void PackageManager::acknowledgePackageInstallation(const QString &taskId)
{
    AM_TRACE(LogInstaller, taskId)

#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        const auto allTasks = d->allTasks();

        for (AsynchronousTask *task : allTasks) {
            if (qobject_cast<InstallationTask *>(task) && (task->id() == taskId)) {
                static_cast<InstallationTask *>(task)->acknowledge();
                break;
            }
        }
    }
#endif
}

/*!
    \qmlmethod string PackageManager::removePackage(string packageId, bool keepDocuments, bool force)

    Uninstalls the package identified by \a packageId. Normally, the documents directory of the
    package is deleted on removal, but this can be prevented by setting \a keepDocuments to \c true.

    The actual removal will happen asynchronously in the background. The PackageManager will
    emit the signals \l taskStarted, \l taskProgressChanged, \l taskFinished, \l taskFailed and \l
    taskStateChanged for the returned \c taskId when applicable.

    Normally, \a force should only be set to \c true if a previous call to removePackage() failed.
    This may be necessary if the installation process was interrupted, or or has file-system issues.

    Returns a unique \c taskId. This can also be an empty string, if the task could not be created
    (in this case, no signals will be emitted).
*/
QString PackageManager::removePackage(const QString &packageId, bool keepDocuments, bool force)
{
    AM_TRACE(LogInstaller, packageId, keepDocuments, force)

#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        if (fromId(packageId)) {
            return enqueueTask(new DeinstallationTask(packageId, d->installationPath,
                                                      d->documentPath, force, keepDocuments));
        }
    }
#endif
    return QString();
}


/*!
    \qmlmethod enumeration PackageManager::taskState(string taskId)

    Returns the current state of the installation task identified by \a taskId.
    \l {Task States}{See here} for a list of valid task states.

    Returns \c PackageManager.Invalid if the \a taskId is invalid.
*/
AsynchronousTask::TaskState PackageManager::taskState(const QString &taskId) const
{
#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        const auto allTasks = d->allTasks();

        for (const AsynchronousTask *task : allTasks) {
            if (task && (task->id() == taskId))
                return task->state();
        }
    }
#else
    Q_UNUSED(taskId)
#endif
    return AsynchronousTask::Invalid;
}

/*!
    \qmlmethod string PackageManager::taskPackageId(string taskId)

    Returns the package id associated with the task identified by \a taskId. The task may not
    have a valid package id at all times though and in this case the function will return an
    empty string (this will be the case for installations before the taskRequestingInstallationAcknowledge
    signal has been emitted).

    Returns an empty string if the \a taskId is invalid.
*/
QString PackageManager::taskPackageId(const QString &taskId) const
{
#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        const auto allTasks = d->allTasks();

        for (const AsynchronousTask *task : allTasks) {
            if (task && (task->id() == taskId))
                return task->packageId();
        }
    }
#else
    Q_UNUSED(taskId)
#endif
    return QString();
}

/*!
    \qmlmethod list<string> PackageManager::activeTaskIds()

    Retuns a list of all currently active (as in not yet finished or failed) installation task ids.
*/
QStringList PackageManager::activeTaskIds() const
{
    QStringList result;
#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        const auto allTasks = d->allTasks();

        for (const AsynchronousTask *task : allTasks)
            result << task->id();
    }
#endif
    return result;
}

/*!
    \qmlmethod bool PackageManager::cancelTask(string taskId)

    Tries to cancel the installation task identified by \a taskId.

    Returns \c true if the task was canceled, \c false otherwise.
*/
bool PackageManager::cancelTask(const QString &taskId)
{
    AM_TRACE(LogInstaller, taskId)

#if !defined(AM_DISABLE_INSTALLER)
    if (!d->disableInstaller) {
        // incoming tasks can be forcefully canceled right away
        for (AsynchronousTask *task : qAsConst(d->incomingTaskList)) {
            if (task->id() == taskId) {
                task->forceCancel();
                task->deleteLater();

                handleFailure(task);

                d->incomingTaskList.removeOne(task);
                triggerExecuteNextTask();
                return true;
            }
        }

        // the active task and async tasks might be in a state where cancellation is not possible,
        // so we have to ask them nicely
        if (d->activeTask && d->activeTask->id() == taskId)
            return d->activeTask->cancel();

        for (AsynchronousTask *task : qAsConst(d->installationTaskList)) {
            if (task->id() == taskId)
                return task->cancel();
        }
    }
#endif
    return false;
}

#if !defined(AM_DISABLE_INSTALLER)

QString PackageManager::enqueueTask(AsynchronousTask *task)
{
    d->incomingTaskList.append(task);
    triggerExecuteNextTask();
    return task->id();
}

void PackageManager::triggerExecuteNextTask()
{
    if (!QMetaObject::invokeMethod(this, &PackageManager::executeNextTask, Qt::QueuedConnection))
        qCCritical(LogSystem) << "ERROR: failed to invoke method checkQueue";
}

void PackageManager::executeNextTask()
{
    if (!d->cleanupBrokenInstallationsDone || d->activeTask || d->incomingTaskList.isEmpty())
        return;

    AsynchronousTask *task = d->incomingTaskList.takeFirst();

    if (task->hasFailed()) {
        task->setState(AsynchronousTask::Failed);

        handleFailure(task);

        task->deleteLater();
        triggerExecuteNextTask();
        return;
    }

    connect(task, &AsynchronousTask::started, this, [this, task]() {
        emit taskStarted(task->id());
    });

    connect(task, &AsynchronousTask::stateChanged, this, [this, task](AsynchronousTask::TaskState newState) {
        emit taskStateChanged(task->id(), newState);
    });

    connect(task, &AsynchronousTask::progress, this, [this, task](qreal p) {
        emit taskProgressChanged(task->id(), p);

        Package *package = fromId(task->packageId());
        if (package && (package->state() != Package::Installed)) {
            package->setProgress(p);
            // Icon will be in a "+" suffixed directory during installation. So notify about a change on its
            // location as well.
            emitDataChanged(package, QVector<int> { Icon, UpdateProgress });
        }
    });

    connect(task, &AsynchronousTask::finished, this, [this, task]() {
        task->setState(task->hasFailed() ? AsynchronousTask::Failed : AsynchronousTask::Finished);

        if (task->hasFailed()) {
            handleFailure(task);
        } else {
            qCDebug(LogInstaller) << "emit finished" << task->id();
            emit taskFinished(task->id());
        }

        if (d->activeTask == task)
            d->activeTask = nullptr;
        d->installationTaskList.removeOne(task);

        delete task;
        triggerExecuteNextTask();
    });

    if (qobject_cast<InstallationTask *>(task)) {
        connect(static_cast<InstallationTask *>(task), &InstallationTask::finishedPackageExtraction, this, [this, task]() {
            qCDebug(LogInstaller) << "emit blockingUntilInstallationAcknowledge" << task->id();
            emit taskBlockingUntilInstallationAcknowledge(task->id());

            // we can now start the next download in parallel - the InstallationTask will take care
            // of serializing the final installation steps on its own as soon as it gets the
            // required acknowledge (or cancel).
            if (d->activeTask == task)
                d->activeTask = nullptr;
            d->installationTaskList.append(task);
            triggerExecuteNextTask();
        });
    }


    d->activeTask = task;
    task->setState(AsynchronousTask::Executing);
    task->start();
}

void PackageManager::handleFailure(AsynchronousTask *task)
{
    qCDebug(LogInstaller) << "emit failed" << task->id() << task->errorCode() << task->errorString();
    emit taskFailed(task->id(), int(task->errorCode()), task->errorString());
}

#endif // !defined(AM_DISABLE_INSTALLER)


Package *PackageManager::startingPackageInstallation(PackageInfo *info)
{
    // ownership of info is transferred to PackageManager
    std::unique_ptr<PackageInfo> newInfo(info);

    if (!newInfo || newInfo->id().isEmpty())
        return nullptr;

    Package *package = fromId(newInfo->id());

    if (package) { // update
        if (!package->block())
            return nullptr;

        // do not overwrite the base-info / update-info yet - only after a successful installation
        d->pendingPackageInfoUpdates.insert(package, newInfo.release());

        package->setState(Package::BeingUpdated);
        package->setProgress(0);
        emitDataChanged(package);
        return package;

    } else { // installation
        // add a new package to the model and block it
        return registerPackage(newInfo.release(), nullptr, true);
    }
}

bool PackageManager::startingPackageRemoval(const QString &id)
{
    Package *package = fromId(id);
    if (!package)
        return false;

    if (package->isBlocked() || (package->state() != Package::Installed))
        return false;

    if (package->isBuiltIn() && !package->builtInHasRemovableUpdate())
        return false;

    if (!package->block()) // this will implicitly stop all apps in this package (asynchronously)
        return false;

    package->setState(package->builtInHasRemovableUpdate() ? Package::BeingDowngraded
                                                           : Package::BeingRemoved);

    package->setProgress(0);
    emitDataChanged(package, QVector<int> { IsUpdating });
    return true;
}

bool PackageManager::finishedPackageInstall(const QString &id)
{
    Package *package = fromId(id);
    if (!package)
        return false;

    switch (package->state()) {
    case Package::Installed:
        return false;

    case Package::BeingUpdated:
    case Package::BeingInstalled:
    case Package::BeingDowngraded: {
        bool isUpdate = (package->state() == Package::BeingUpdated);
        bool isDowngrade = (package->state() == Package::BeingDowngraded);

        // figure out what the new info is
        PackageInfo *newPackageInfo;
        if (isUpdate)
            newPackageInfo = d->pendingPackageInfoUpdates.take(package);
        else if (isDowngrade)
            newPackageInfo = nullptr;
        else
            newPackageInfo = package->baseInfo();

        // attach the installation report (unless we're just downgrading a built-in)
        if (!isDowngrade) {
            QFile irfile(newPackageInfo->baseDir().absoluteFilePath(qSL(".installation-report.yaml")));
            auto ir = std::make_unique<InstallationReport>(package->id());
            irfile.open(QFile::ReadOnly);
            try {
                ir->deserialize(&irfile);
            } catch (const Exception &e) {
                qCCritical(LogInstaller) << "Could not read the new installation-report for package"
                                         << package->id() << "at" << irfile.fileName() << ":"
                                         << e.errorString();
                return false;
            }
            newPackageInfo->setInstallationReport(ir.release());
        }

        if (isUpdate || isDowngrade) {
            // unregister all the old apps & intents
            unregisterApplicationsAndIntentsOfPackage(package);

            // update the correct base/updated info pointer
            PackageInfo *oldPackageInfo;
            if (package->isBuiltIn())
                oldPackageInfo = package->setUpdatedInfo(newPackageInfo);
            else
                oldPackageInfo = package->setBaseInfo(newPackageInfo);

            if (oldPackageInfo)
                d->database->removePackageInfo(oldPackageInfo);
        }

        // add the new info to the package db
        if (newPackageInfo)
            d->database->addPackageInfo(newPackageInfo);

        // register all the apps & intents
        registerApplicationsAndIntentsOfPackage(package);

        // boiler-plate cleanup code
        package->setState(Package::Installed);
        package->setProgress(0);
        emitDataChanged(package);
        package->unblock();
        emit package->bulkChange(); // not ideal, but icon and codeDir have changed
        break;
    }

    case Package::BeingRemoved: {
        // unregister all the apps & intents
        unregisterApplicationsAndIntentsOfPackage(package);

        // remove the package from the model
        int row = d->packages.indexOf(package);
        if (row >= 0) {
            emit packageAboutToBeRemoved(package->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->packages.removeAt(row);
            endRemoveRows();
        }

        // cleanup
        package->unblock();

        // remove the package from the package db
        d->database->removePackageInfo(package->info());

        delete package;
        break;
    }
    }

    return true;
}

bool PackageManager::canceledPackageInstall(const QString &id)
{
    Package *package = fromId(id);
    if (!package)
        return false;

    switch (package->state()) {
    case Package::Installed:
        return false;

    case Package::BeingInstalled: {
        // remove the package from the model
        int row = d->packages.indexOf(package);
        if (row >= 0) {
            emit packageAboutToBeRemoved(package->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->packages.removeAt(row);
            endRemoveRows();
        }

        // cleanup
        package->unblock();

        // it's not yet added to the package db, so we need to delete ourselves
        delete package->info();

        delete package;
        break;
    }
    case Package::BeingUpdated:
    case Package::BeingDowngraded:
    case Package::BeingRemoved:
        delete d->pendingPackageInfoUpdates.take(package);

        package->setState(Package::Installed);
        package->setProgress(0);
        emitDataChanged(package, QVector<int> { IsUpdating });

        package->unblock();
        break;
    }
    return true;
}


/*!
    \qmlmethod int PackageManager::compareVersions(string version1, string version2)

    Convenience method for app-store implementations or taskRequestingInstallationAcknowledge()
    callbacks for comparing version numbers, as the actual version comparison algorithm is not
    trivial.

    Returns \c -1, \c 0 or \c 1 if \a version1 is smaller than, equal to, or greater than \a
    version2 (similar to how \c strcmp() works).
*/
int PackageManager::compareVersions(const QString &version1, const QString &version2)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    int vn1Suffix = -1;
    int vn2Suffix = -1;
#else
    qsizetype vn1Suffix = -1;
    qsizetype vn2Suffix = -1;
#endif
    QVersionNumber vn1 = QVersionNumber::fromString(version1, &vn1Suffix);
    QVersionNumber vn2 = QVersionNumber::fromString(version2, &vn2Suffix);

    int d = QVersionNumber::compare(vn1, vn2);
    return d < 0 ? -1 : (d > 0 ? 1 : version1.mid(vn1Suffix).compare(version2.mid(vn2Suffix)));
}

/*!
    \qmlmethod int PackageManager::validateDnsName(string name, int minimalPartCount)

    Convenience method for app-store implementations or taskRequestingInstallationAcknowledge()
    callbacks for checking if the given \a name is a valid DNS (or reverse-DNS) name according to
    RFC 1035/1123. If the optional parameter \a minimalPartCount is specified, this function will
    also check if \a name contains at least this amount of parts/sub-domains.

    Returns \c true if the name is a valid DNS name or \c false otherwise.
*/
bool PackageManager::validateDnsName(const QString &name, int minimalPartCount)
{
    try {
        // check if we have enough parts: e.g. "tld.company.app" would have 3 parts
        QStringList parts = name.split(qL1C('.'));
        if (parts.size() < minimalPartCount) {
            throw Exception(Error::Parse, "the minimum amount of parts (subdomains) is %1 (found %2)")
                .arg(minimalPartCount).arg(parts.size());
        }

        // standard RFC compliance tests (RFC 1035/1123)

        auto partCheck = [](const QString &part) {
            int len = part.length();

            if (len < 1 || len > 63)
                throw Exception(Error::Parse, "domain parts must consist of at least 1 and at most 63 characters (found %2 characters)").arg(len);

            for (int pos = 0; pos < len; ++pos) {
                ushort ch = part.at(pos).unicode();
                bool isFirst = (pos == 0);
                bool isLast  = (pos == (len - 1));
                bool isDash  = (ch == '-');
                bool isDigit = (ch >= '0' && ch <= '9');
                bool isLower = (ch >= 'a' && ch <= 'z');

                if ((isFirst || isLast || !isDash) && !isDigit && !isLower)
                    throw Exception(Error::Parse, "domain parts must consist of only the characters '0-9', 'a-z', and '-' (which cannot be the first or last character)");
            }
        };

        for (const QString &part : parts)
            partCheck(part);

        return true;
    } catch (const Exception &e) {
        qCDebug(LogInstaller).noquote() << "validateDnsName failed:" << e.errorString();
        return false;
    }
}

bool removeRecursiveHelper(const QString &path)
{
#if !defined(AM_DISABLE_INSTALLER)
    if (PackageManager::instance()->isApplicationUserIdSeparationEnabled() && SudoClient::instance())
        return SudoClient::instance()->removeRecursive(path);
    else
#endif
        return recursiveOperation(path, safeRemove);
}

QT_END_NAMESPACE_AM

#include "moc_packagemanager.cpp"
