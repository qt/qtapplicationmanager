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

#include <QMetaMethod>
#include <QQmlEngine>
#include <QVersionNumber>
#include "packagemanager.h"
#include "packagedatabase.h"
#include "packagemanager_p.h"
#include "package.h"
#include "logging.h"
#include "installationreport.h"
#include "exception.h"
#include "sudo.h"
#include "utilities.h"

#if defined(Q_OS_WIN)
#  include <Windows.h>
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

    QScopedPointer<PackageManager> pm(new PackageManager(packageDatabase, documentPath));
    registerQmlTypes();

    // map all the built-in packages first
    const auto builtinPackages = packageDatabase->builtInPackages();
    for (auto packageInfo : builtinPackages) {
        auto *package = new Package(packageInfo);
        QQmlEngine::setObjectOwnership(package, QQmlEngine::CppOwnership);
        pm->d->packages << package;
    }

    // next, map all the installed packages, making sure to detect updates to built-in ones
    const auto installedPackages = packageDatabase->installedPackages();
    for (auto packageInfo : installedPackages) {
        Package *builtInPackage = pm->fromId(packageInfo->id());

        if (builtInPackage) { // update
            if (builtInPackage->updatedInfo()) { // but there already is an update applied!?
                throw Exception(Error::Package, "Found more than one update for the built-in package '%1'")
                        .arg(builtInPackage->id());
                //TODO: can we get the paths to both info.yaml here?
            }
            builtInPackage->setUpdatedInfo(packageInfo);
        } else {
            auto *package = new Package(packageInfo);
            QQmlEngine::setObjectOwnership(package, QQmlEngine::CppOwnership);
            pm->d->packages << package;
        }
    }

    return s_instance = pm.take();
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
    }
    return QVariant();
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
          package() instead to access the Package object's properties directly.
*/
QVariantMap PackageManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "PackageManager::get(index): invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(qL1S(it.value()), data(this->index(index), it.key()));
    return map;
}

/*!
    \qmlmethod PackageObject PackageManager::package(int index)

    Returns the \l{PackageObject}{package} corresponding to the given \a index in the
    model, or \c null if the index is invalid.

    \note The object ownership of the returned Package object stays with the application-manager.
          If you want to store this pointer, you can use the PackageManager's QAbstractListModel
          signals or the packageAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Package *PackageManager::package(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "PackageManager::application(index): invalid index:" << index;
        return nullptr;
    }
    return d->packages.at(index);
}

/*!
    \qmlmethod PackageObject PackageManager::package(string id)

    Returns the \l{PackageObject}{package} corresponding to the given package \a id,
    or \c null if the id does not exist.

    \note The object ownership of the returned Package object stays with the application-manager.
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
    if (minUserId >= maxUserId || minUserId == uint(-1) || maxUserId == uint(-1))
        return false;
    d->userIdSeparation = true;
    d->minUserId = minUserId;
    d->maxUserId = maxUserId;
    d->commonGroupId = commonGroupId;
    return true;
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

    Returns an object describing the location under which applications are installed in detail.

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
    return locationMap(d->documentPath);
}

void PackageManager::cleanupBrokenInstallations() Q_DECL_NOEXCEPT_EXPR(false)
{
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

            QString pkgDir = d->installationPath + pkg->id();
            QStringList checkDirs;
            QStringList checkFiles;

            checkFiles << pkgDir + qSL("/info.yaml");
            checkFiles << pkgDir + qSL("/.installation-report.yaml");
            checkDirs << pkgDir;
            checkDirs << d->installationPath + pkg->id();

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
                validPaths.insertMulti(d->installationPath, pkg->id() + qL1C('/'));
                validPaths.insertMulti(d->documentPath, pkg->id() + qL1C('/'));
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
                name.append(qL1C('/'));

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
    \qmlmethod object PackageManager::get(string id)

    Retrieves the model data for the package identified by \a id as a JavaScript object.
    See the \l {PackageManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a id is invalid.
*/
QVariantMap PackageManager::get(const QString &id) const
{
    int index = indexOfPackage(id);
    return (index < 0) ? QVariantMap{} : get(index);
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
    AM_TRACE(LogInstaller, sourceUrl);

    return enqueueTask(new InstallationTask(d->installationPath, d->documentPath, sourceUrl));
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

    const auto allTasks = d->allTasks();

    for (AsynchronousTask *task : allTasks) {
        if (qobject_cast<InstallationTask *>(task) && (task->id() == taskId)) {
            static_cast<InstallationTask *>(task)->acknowledge();
            break;
        }
    }
}

/*!
    \qmlmethod string PackageManager::removePackage(string packageId, bool keepDocuments, bool force)

    Uninstalls the package identified by \a id. Normally, the documents directory of the
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
    AM_TRACE(LogInstaller, packageId, keepDocuments)

    if (Package *package = fromId(packageId)) {
        if (package->info()->installationReport()) {
            return enqueueTask(new DeinstallationTask(package, d->installationPath,
                                                      d->documentPath, force, keepDocuments));
        }
    }
    return QString();
}


/*!
    \qmlmethod enumeration PackageManager::taskState(string taskId)

    Returns the current state of the installation task identified by \a taskId.
    \l {TaskStates}{See here} for a list of valid task states.

    Returns \c PackageManager.Invalid if the \a taskId is invalid.
*/
AsynchronousTask::TaskState PackageManager::taskState(const QString &taskId) const
{
    const auto allTasks = d->allTasks();

    for (const AsynchronousTask *task : allTasks) {
        if (task && (task->id() == taskId))
            return task->state();
    }
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
    const auto allTasks = d->allTasks();

    for (const AsynchronousTask *task : allTasks) {
        if (task && (task->id() == taskId))
            return task->packageId();
    }
    return QString();
}

/*!
    \qmlmethod list<string> PackageManager::activeTaskIds()

    Retuns a list of all currently active (as in not yet finished or failed) installation task ids.
*/
QStringList PackageManager::activeTaskIds() const
{
    const auto allTasks = d->allTasks();

    QStringList result;
    for (const AsynchronousTask *task : allTasks)
        result << task->id();
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

    // incoming tasks can be forcefully cancelled right away
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
    return false;
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
    int vn1Suffix = -1;
    int vn2Suffix = -1;
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
        QStringList parts = name.split('.');
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

QString PackageManager::enqueueTask(AsynchronousTask *task)
{
    d->incomingTaskList.append(task);
    triggerExecuteNextTask();
    return task->id();
}

void PackageManager::triggerExecuteNextTask()
{
    if (!QMetaObject::invokeMethod(this, "executeNextTask", Qt::QueuedConnection))
        qCCritical(LogSystem) << "ERROR: failed to invoke method checkQueue";
}

void PackageManager::executeNextTask()
{
    if (d->activeTask || d->incomingTaskList.isEmpty())
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

bool PackageManager::startingPackageInstallation(PackageInfo *info)
{
    // ownership of info is transferred to PackageManager
    QScopedPointer<PackageInfo> newInfo(info);

    if (!newInfo || newInfo->id().isEmpty())
        return false;
    Package *package = fromId(newInfo->id());
//    if (!RuntimeFactory::instance()->manager(newInfo->runtimeName()))
//        return false;

    if (package) { // update
        if (!package->block())
            return false;

        if (package->isBuiltIn()) {
            // overlay the existing base info
            // we will rollback to the base one if this update is removed.
            package->setUpdatedInfo(newInfo.take());
        } else {
            // overwrite the existing base info
            // we're not keeping track of the original. so removing the updated base version removes the
            // application entirely.
            package->setBaseInfo(newInfo.take());
        }
        package->setState(Package::BeingUpdated);
        package->setProgress(0);
        emitDataChanged(package);
    } else { // installation
        package = new Package(newInfo.take(), Package::BeingInstalled);

        Q_ASSERT(package->block());

        beginInsertRows(QModelIndex(), d->packages.count(), d->packages.count());

        QQmlEngine::setObjectOwnership(package, QQmlEngine::CppOwnership);
        d->packages << package;

        endInsertRows();

        emitDataChanged(package);

        emit packageAdded(package->id());
    }
    return true;
}

bool PackageManager::startingPackageRemoval(const QString &id)
{
    Package *package = fromId(id);
    if (!package)
        return false;

    if (package->isBlocked() || (package->state() != Package::Installed))
        return false;

    if (package->isBuiltIn() && !package->canBeRevertedToBuiltIn())
        return false;

    if (!package->block()) // this will implicitly stop all apps in this package (asynchronously)
        return false;

    package->setState(package->canBeRevertedToBuiltIn() ? Package::BeingDowngraded
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

    case Package::BeingInstalled:
    case Package::BeingUpdated: {
        // The Package object has been updated right at the start of the installation/update.
        // Now's the time to update the InstallationReport that was written by the installer.
        QFile irfile(QDir(package->info()->baseDir()).absoluteFilePath(qSL(".installation-report.yaml")));
        QScopedPointer<InstallationReport> ir(new InstallationReport(package->id()));
        if (!irfile.open(QFile::ReadOnly) || !ir->deserialize(&irfile)) {
            qCCritical(LogInstaller) << "Could not read the new installation-report for package"
                                     << package->id() << "at" << irfile.fileName();
            return false;
        }
        package->info()->setInstallationReport(ir.take());
        package->setState(Package::Installed);
        package->setProgress(0);

        emitDataChanged(package);

        package->unblock();
        emit package->bulkChange(); // not ideal, but icon and codeDir have changed
        break;
    }
    case Package::BeingDowngraded:
        package->setUpdatedInfo(nullptr);
        package->setState(Package::Installed);
        break;

    case Package::BeingRemoved: {
        int row = d->packages.indexOf(package);
        if (row >= 0) {
            emit packageAboutToBeRemoved(package->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->packages.removeAt(row);
            endRemoveRows();
        }
        delete package;
        break;
    }
    }

    //emit internalSignals.applicationsChanged();

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
        int row = d->packages.indexOf(package);
        if (row >= 0) {
            emit packageAboutToBeRemoved(package->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->packages.removeAt(row);
            endRemoveRows();
        }
        delete package;
        break;
    }
    case Package::BeingUpdated:
    case Package::BeingDowngraded:
    case Package::BeingRemoved:
        package->setState(Package::Installed);
        package->setProgress(0);
        emitDataChanged(package, QVector<int> { IsUpdating });

        package->unblock();
        break;
    }
    return true;
}

bool removeRecursiveHelper(const QString &path)
{
    if (PackageManager::instance()->isApplicationUserIdSeparationEnabled() && SudoClient::instance())
        return SudoClient::instance()->removeRecursive(path);
    else
        return recursiveOperation(path, safeRemove);
}

QT_END_NAMESPACE_AM
