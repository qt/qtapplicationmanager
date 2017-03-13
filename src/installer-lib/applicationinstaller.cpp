/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include <QCoreApplication>
#include <QDir>
#include <QUuid>

#include "logging.h"
#include "application.h"
#include "applicationinstaller.h"
#include "applicationinstaller_p.h"
#include "installationtask.h"
#include "deinstallationtask.h"
#include "sudo.h"
#include "utilities.h"
#include "exception.h"
#include "global.h"
#include "qml-utilities.h"
#include "applicationmanager.h"


#define AM_AUTHENTICATE_DBUS(RETURN_TYPE) \
    do { \
        if (!checkDBusPolicy(this, d->dbusPolicy, __FUNCTION__, [](qint64 pid) -> QStringList { return ApplicationManager::instance()->capabilities(ApplicationManager::instance()->identifyApplication(pid)); })) \
            return RETURN_TYPE(); \
    } while (false);

/*!
    \qmltype ApplicationInstaller
    \inqmlmodule QtApplicationManager
    \brief The ApplicationInstaller singleton.

    The ApplicationInstaller singleton type handles the package installation
    part of the application manager. It provides both a DBus and QML APIs for
    all of its functionality.

    \target TaskStates

    The following table describes all possible states that a background task could be in:

    \table
    \header
        \li Task State
        \li Description
    \row
        \li \c queued
        \li The task was created and is now queued up for execution.
    \row
        \li \c executing
        \li The task is being executed.
    \row
        \li \c finished
        \li The task was executed successfully.
    \row
        \li \c failed
        \li The task failed to execute successfully.
    \row
        \li \c awaitingAcknowledge
        \li \e{Installation tasks only!} The task is currently halted, waiting for either
            acknowledgePackageInstallation() or cancelTask() to continue. See startPackageInstallation()
            for more information on the installation workflow.
    \row
        \li \c installing
        \li \e{Installation tasks only!} The installation was acknowledged via acknowledgePackageInstallation()
            and the final installation phase is now running.
    \row
        \li \c cleaningUp
        \li \e{Installation tasks only!} The installation has finished, and previous installations as
            well as temporary files are being cleaned up.
    \endtable

    The normal workflow for tasks is: \c queued \unicode{0x2192} \c active \unicode{0x2192} \c
    finished. The task can enter the \c failed state at any point though - either by being canceled via
    cancelTask() or simply by failing due to an error.

    Installation tasks are a bit more complex due to the acknowledgment: \c queued \unicode{0x2192}
    \c executing \unicode{0x2192} \c awaitingAcknowledge (this state may be skipped if
    acknowledgePackageInstallation() was called already) \unicode{0x2192} \c installing
    \unicode{0x2192} \c cleanup \unicode{0x2192} \c finished. Again, the task can fail at any point.
*/

// THIS IS MISSING AN EXAMPLE!

/*!
    \qmlsignal ApplicationInstaller::taskStateChanged(string taskId, string newState)

    This signal is emitted when the state of the task identified by \a taskId changes. The
    new state is supplied in the parameter \a newState.

    \sa taskState()
*/

/*!
    \qmlsignal ApplicationInstaller::taskStarted(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c active state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskFinished(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c finished state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskFailed(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c failed state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskRequestingInstallationAcknowledge(string taskId, object application)

    This signal is emitted when the installation task identified by \a taskId has received enough
    meta-data to be able to emit this signal. The task may be in either \c executing or \c
    awaitingAcknowledge state.

    The contents of the package's manifest file are supplied via \a application as a JavaScript object.
    Please see the \l {ApplicationManager Roles}{role names} for the expected object fields.

    Following this signal, either cancelTask() or acknowledgePackageInstallation() has to be called
    for this \a taskId, to either cancel the installation or try to complete it.

    \sa taskStateChanged(), startPackageInstallation()
*/

/*!
    \qmlsignal ApplicationInstaller::taskBlockingUntilInstallationAcknowledge(string taskId)

    This signal is emitted when the installation task identified by \a taskId cannot continue
    due to a missing acknowledgePackageInstallation() call for the task.

    \sa taskStateChanged(), acknowledgePackageInstallation()
*/

/*!
    \qmlsignal ApplicationInstaller::taskProgressChanged(string taskId, qreal progress)

    This signal is emitted whenever the task identified by \a taskId makes progress towards its
    completion. The \a progress is reported as a floating-point number ranging from \c 0.0 to \c 1.0.

    \sa taskStateChanged()
*/

QT_BEGIN_NAMESPACE_AM

ApplicationInstaller *ApplicationInstaller::s_instance = nullptr;

ApplicationInstaller::ApplicationInstaller(const QVector<InstallationLocation> &installationLocations,
                                           const QDir &manifestDir, const QDir &imageMountDir,
                                           QObject *parent)
    : QObject(parent)
    , d(new ApplicationInstallerPrivate())
{
    d->installationLocations = installationLocations;
    d->manifestDir = manifestDir;
    d->imageMountDir = imageMountDir;
}

ApplicationInstaller::~ApplicationInstaller()
{
    delete d;
}

ApplicationInstaller *ApplicationInstaller::createInstance(const QVector<InstallationLocation> &installationLocations,
                                                           const QDir &manifestDir, const QDir &imageMountDir, QString *error)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationInstaller::createInstance() was called a second time.");

    qRegisterMetaType<AsynchronousTask *>();

    if (Q_UNLIKELY(!manifestDir.exists())) {
        if (error)
            *error = qL1S("ApplicationInstaller::createInstance() could not access the manifest directory ") + manifestDir.absolutePath();
        return 0;
    }
    if (Q_UNLIKELY(!imageMountDir.exists())) {
        if (error)
            *error = qL1S("ApplicationInstaller::createInstance() could not access the image-mount directory ") + imageMountDir.absolutePath();
        return 0;
    }

    qmlRegisterSingletonType<ApplicationInstaller>("QtApplicationManager", 1, 0, "ApplicationInstaller",
                                                   &ApplicationInstaller::instanceForQml);

    return s_instance = new ApplicationInstaller(installationLocations, manifestDir, imageMountDir, QCoreApplication::instance());
}

ApplicationInstaller *ApplicationInstaller::instance()
{
    if (!s_instance)
        qFatal("ApplicationInstaller::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationInstaller::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
    return instance();
}

bool ApplicationInstaller::developmentMode() const
{
    return d->developmentMode;
}

void ApplicationInstaller::setDevelopmentMode(bool b)
{
    d->developmentMode = b;
}

bool ApplicationInstaller::allowInstallationOfUnsignedPackages() const
{
    return d->allowInstallationOfUnsignedPackages;
}

void ApplicationInstaller::setAllowInstallationOfUnsignedPackages(bool b)
{
    d->allowInstallationOfUnsignedPackages = b;
}

bool ApplicationInstaller::isApplicationUserIdSeparationEnabled() const
{
    return d->userIdSeparation;
}

uint ApplicationInstaller::commonApplicationGroupId() const
{
    return d->commonGroupId;
}

bool ApplicationInstaller::enableApplicationUserIdSeparation(uint minUserId, uint maxUserId, uint commonGroupId)
{
    if (minUserId >= maxUserId || minUserId == uint(-1) || maxUserId == uint(-1))
        return false;
    d->userIdSeparation = true;
    d->minUserId = minUserId;
    d->maxUserId = maxUserId;
    d->commonGroupId = commonGroupId;
    return true;
}

uint ApplicationInstaller::findUnusedUserId() const Q_DECL_NOEXCEPT_EXPR(false)
{
    if (!isApplicationUserIdSeparationEnabled())
        return uint(-1);

    QVector<const Application *> apps = ApplicationManager::instance()->applications();

    for (uint uid = d->minUserId; uid <= d->maxUserId; ++uid) {
        bool match = false;
        foreach (const Application *app, apps) {
            if (app->uid() == uid) {
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

QDir ApplicationInstaller::manifestDirectory() const
{
    return d->manifestDir;
}

QDir ApplicationInstaller::applicationImageMountDirectory() const
{
    return d->imageMountDir;
}

bool ApplicationInstaller::setDBusPolicy(const QVariantMap &yamlFragment)
{
    static const QVector<QByteArray> functions {
        QT_STRINGIFY(startPackageInstallation),
        QT_STRINGIFY(acknowledgePackageInstallation),
        QT_STRINGIFY(removePackage),
        QT_STRINGIFY(taskState),
        QT_STRINGIFY(cancelTask)
    };

    d->dbusPolicy = parseDBusPolicy(yamlFragment);

    for (auto it = d->dbusPolicy.cbegin(); it != d->dbusPolicy.cend(); ++it) {
       if (!functions.contains(it.key()))
           return false;
    }
    return true;
}

QList<QByteArray> ApplicationInstaller::caCertificates() const
{
    return d->chainOfTrust;
}

void ApplicationInstaller::setCACertificates(const QList<QByteArray> &chainOfTrust)
{
    d->chainOfTrust = chainOfTrust;
}

void ApplicationInstaller::cleanupBrokenInstallations() const Q_DECL_NOEXCEPT_EXPR(false)
{
    // 1. find mounts and loopbacks left-over from a previous instance and kill them

    QMultiMap<QString, QString> mountPoints = mountedDirectories();

    foreach (const QFileInfo &fi, applicationImageMountDirectory().entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        QString path = fi.canonicalFilePath();
        QString device = mountPoints.value(path);

        if (!device.isEmpty()) {
            if (!SudoClient::instance()->unmount(path)) {
                if (!SudoClient::instance()->unmount(path, true /*force*/))
                    throw Exception("failed to un-mount stale mount %1 on %2: %3")
                        .arg(device, path, SudoClient::instance()->lastError());
            }
            if (device.startsWith(qL1S("/dev/loop"))) {
                if (!SudoClient::instance()->detachLoopback(device)) {
                    qCWarning(LogInstaller) << "failed to detach stale loopback device" << device;
                    // we can still continue here
                }
            }
        }

        if (!SudoClient::instance()->removeRecursive(path))
            throw Exception(Error::IO, SudoClient::instance()->lastError());
    }

    // 2. Check that everything in the app-db is available
    //    -> if not, remove from app-db

    ApplicationManager *am = ApplicationManager::instance();

    // key: baseDirPath, value: subDirName/ or fileName
    QMultiMap<QString, QString> validPaths {
        { manifestDirectory().absolutePath(), QString() }
    };
    foreach (const InstallationLocation &il, installationLocations()) {
        if (!il.isRemovable() || il.isMounted()) {
            validPaths.insert(il.documentPath(), QString());
            validPaths.insert(il.installationPath(), QString());
        }
    }

    foreach (const Application *app, am->applications()) {
        const InstallationReport *ir = app->installationReport();
        if (ir) {
            const InstallationLocation &il = installationLocationFromId(ir->installationLocationId());

            bool valid = il.isValid();

            if (valid && (!il.isRemovable() || il.isMounted())) {
                QStringList checkDirs;
                QStringList checkFiles;

                checkDirs << manifestDirectory().absoluteFilePath(app->id());
                checkFiles << manifestDirectory().absoluteFilePath(app->id()) + qSL("/info.yaml");
                checkFiles << manifestDirectory().absoluteFilePath(app->id()) + qSL("/installation-report.yaml");
                checkDirs << il.documentPath() + app->id();

                if (il.isRemovable())
                    checkFiles << il.installationPath() + app->id() + qSL(".appimg");
                else
                    checkDirs << il.installationPath() + app->id();

                foreach (const QString &checkFile, checkFiles) {
                    QFileInfo fi(checkFile);
                    if (!fi.exists() || !fi.isFile() || !fi.isReadable()) {
                        valid = false;
                        break;
                    }
                }
                foreach (const QString &checkDir, checkDirs) {
                    QFileInfo fi(checkDir);
                    if (!fi.exists() || !fi.isDir() || !fi.isReadable()) {
                        valid = false;
                        break;
                    }
                }

                if (valid) {
                    if (il.isRemovable())
                        validPaths.insertMulti(il.installationPath(), app->id() + qSL(".appimg"));
                    else
                        validPaths.insertMulti(il.installationPath(), app->id() + qL1C('/'));
                    validPaths.insertMulti(il.documentPath(), app->id() + qL1C('/'));
                    validPaths.insertMulti(manifestDirectory().absolutePath(), app->id() + qL1C('/'));
                }
            }
            if (!valid) {
                if (am->startingApplicationRemoval(app->id())) {
                    if (am->finishedApplicationInstall(app->id()))
                        continue;
                }
                throw Exception(Error::Package, "could not remove broken installation of app %1 from database").arg(app->id());
            }
        } else {
            // built-in, so make sure we do not kill the document directory
            validPaths.insertMulti(defaultInstallationLocation().documentPath(), app->id() + qL1C('/'));
        }
    }

    // 3. Remove everything that is not referenced from the app-db

    foreach (const QString &dir, validPaths.uniqueKeys()) {
        foreach (const QFileInfo &fi, QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            QString name = fi.fileName();
            QStringList validNames = validPaths.values(dir);

            if ((fi.isDir() && validNames.contains(name + qL1C('/')))
                    || (fi.isFile() && validNames.contains(name))) {
                continue;
            }
            if (!SudoClient::instance()->removeRecursive(fi.absoluteFilePath()))
                throw Exception(Error::IO, "could not remove broken installation leftover %1 : %2").arg(fi.absoluteFilePath()).arg(SudoClient::instance()->lastError());
        }
    }
}

QVector<InstallationLocation> ApplicationInstaller::installationLocations() const
{
    return d->installationLocations;
}


const InstallationLocation &ApplicationInstaller::defaultInstallationLocation() const
{
    static bool once = false;
    static int defaultIndex = -1;

    if (!once) {
        for (int i = 0; i < d->installationLocations.size(); ++i) {
            if (d->installationLocations.at(i).isDefault()) {
                defaultIndex = i;
                break;
            }
        }
        once = true;
    }
    return (defaultIndex < 0) ? d->invalidInstallationLocation : d->installationLocations.at(defaultIndex);
}

const InstallationLocation &ApplicationInstaller::installationLocationFromId(const QString &installationLocationId) const
{
    foreach (const InstallationLocation &il, d->installationLocations) {
        if (il.id() == installationLocationId)
            return il;
    }
    return d->invalidInstallationLocation;
}

const InstallationLocation &ApplicationInstaller::installationLocationFromApplication(const QString &id) const
{
    if (const Application *a = ApplicationManager::instance()->fromId(id)) {
        if (const InstallationReport *report = a->installationReport())
            return installationLocationFromId(report->installationLocationId());
    }
    return d->invalidInstallationLocation;
}


/*!
   \qmlmethod list<string> ApplicationInstaller::installationLocationIds()

   Retuns a list of all known installation location ids. Calling getInstallationLocation() on one of
   the returned identifiers will yield specific information about the individual installation locations.
*/
QStringList ApplicationInstaller::installationLocationIds() const
{
    QStringList ids;
    ids.reserve(d->installationLocations.size());
    foreach (const InstallationLocation &il, d->installationLocations)
        ids << il.id();
    return ids;
}

/*!
   \qmlmethod string ApplicationInstaller::installationLocationIdFromApplication(string id)

   Returns the installation location id for the application identified by \a id. Returns
   an empty string in case the application is not installed.

   \sa installationLocationIds()
 */
QString ApplicationInstaller::installationLocationIdFromApplication(const QString &id) const
{
    const InstallationLocation &il = installationLocationFromApplication(id);
    return il.isValid() ? il.id() : QString();
}

/*!
    \qmlmethod object ApplicationInstaller::getInstallationLocation(string installationLocationId)

    Returns an object describing the installation location identified by \a installationLocationId in detail.

    The returned object has the following members:

    \table
    \header
        \li \c Name
        \li \c Type
        \li Description
    \row
        \li \c id
        \li \c string
        \li The installation location id that is used as the handle for all other ApplicationInstaller
            function calls. The \c id consists of the \c type and \c index field, concatenated by
            a single dash (for example, \c internal-0).
    \row
        \li \c type
        \li \c string
        \li The type of device this installation location is connected to. Valid values are \c
            internal (for any kind of built-in storage, e.g. flash), \c removable (for any kind of
            storage that is removable by the user, e.g. an SD card) and \c invalid.
    \row
        \li \c index
        \li \c int
        \li In case there is more than one installation location for the same type of device, this
            \c zero-based index is used for disambiguation. For example, two SD card slots will
            result in the ids \c removable-0 and \c removable-1. Otherwise, the index is always \c 0.
    \row
        \li \c isDefault
        \li \c bool

        \li Exactly one installation location is the default location which must be mounted and
            accessible at all times. This can be used by an UI application to get a sensible
            default for the installation location that it needs to pass to startPackageInstallation().
    \row
        \li \c isRemovable
        \li \c bool
        \li Indicates whether this installation location is on a removable media (for example, an SD
            card).
    \row
        \li \c isMounted
        \li \c bool
        \li The current mount status of this installation location: should always be \c true for
            non-removable media.
    \row
        \li \c installationPath
        \li \c string
        \li The absolute file-system path to the base directory under which applications are installed.
    \row
        \li \c installationDeviceSize
        \li \c int
        \li The size of the device holding \c installationPath in bytes. This field is only present if
            \c isMounted is \c true.
    \row
        \li \c installationDeviceFree
        \li \c int
        \li The amount of bytes available on the device holding \c installationPath. This field is only
            present if \c isMounted is \c true.
    \row
        \li \c documentPath
        \li \c string
        \li The absolute file-system path to the base directory under which per-user document
            directories are created.
    \row
        \li \c documentDeviceSize
        \li \c int
        \li The size of the device holding \c documentPath in bytes. This field is only present if
            \c isMounted is \c true.
    \row
        \li \c documentDeviceFree
        \li \c int
        \li The amount of bytes available on the device holding \c documentPath. This field is only
            present if \c isMounted is \c true.
    \endtable

    Returns an empty object in case the \a installationLocationId is not valid.
*/
QVariantMap ApplicationInstaller::getInstallationLocation(const QString &installationLocationId) const
{
    const InstallationLocation &il = installationLocationFromId(installationLocationId);
    return il.isValid() ? il.toVariantMap() : QVariantMap();
}

/*!
   \qmlmethod int ApplicationInstaller::installedApplicationSize(string id)

   Returns the size in bytes that the application identified by \a id is occupying on the storage
   device.

   Returns \c -1 in case the application \a id is not valid, or the application is not installed.
*/
qint64 ApplicationInstaller::installedApplicationSize(const QString &id) const
{
    if (const Application *a = ApplicationManager::instance()->fromId(id)) {
        if (const InstallationReport *report = a->installationReport()) {
            return report->diskSpaceUsed();
        }
    }
    return -1;
}

/*! \internal
  Type safe convenience function, since DBus does not like QUrl
*/
QString ApplicationInstaller::startPackageInstallation(const QString &installationLocationId, const QUrl &sourceUrl)
{
    AM_TRACE(LogInstaller, installationLocationId, sourceUrl);
    AM_AUTHENTICATE_DBUS(QString)

    const InstallationLocation &il = installationLocationFromId(installationLocationId);

    return enqueueTask(new InstallationTask(il, sourceUrl));
}

/*!
    \qmlmethod string ApplicationInstaller::startPackageInstallation(string installationLocationId, string sourceUrl)

    Downloads an application package from \a sourceUrl and installs it to the installation location
    described by \a installationLocationId.

    The actual download and installation will happen asynchronously in the background. The
    ApplicationInstaller emits the signals \l taskStarted, \l taskProgressChanged, \l
    taskRequestingInstallationAcknowledge, \l taskFinished, \l taskFailed, and \l taskStateChanged
    for the returned taskId when applicable.

    \note Simply calling this function is not enough to complete a package installation: The
    taskRequestingInstallationAcknowledge() signal needs to be connected to a slot where the
    supplied application meta-data can be validated (either programmatically or by asking the user).
    If the validation is successful, the installation can be completed by calling
    acknowledgePackageInstallation() or, if the validation was unsuccessful, the installation should
    be canceled by calling cancelTask().
    Failing to do one or the other will leave an unfinished "zombie" installation.

    Returns a unique \c taskId. This can also be an empty string, if the task could not be
    created (in this case, no signals will be emitted).
*/
QString ApplicationInstaller::startPackageInstallation(const QString &installationLocationId, const QString &sourceUrl)
{
    QUrl url(sourceUrl);
    if (url.scheme().isEmpty())
        url = QUrl::fromLocalFile(sourceUrl);
    return startPackageInstallation(installationLocationId, url);
}

/*!
    \qmlmethod void ApplicationInstaller::acknowledgePackageInstallation(string taskId)

    Calling this function enables the installer to complete the installation task identified by \a
    taskId. Normally, this function is called after receiving the requestingInstallationAcknowledge()
    signal, and the user and/or the program logic decided to proceed with the installation.

    \sa startPackageInstallation()
 */
void ApplicationInstaller::acknowledgePackageInstallation(const QString &taskId)
{
    AM_TRACE(LogInstaller, taskId)
    AM_AUTHENTICATE_DBUS(void)

    auto allTasks = d->taskQueue;
    allTasks.append(d->activeTask);

    for (AsynchronousTask *task : qAsConst(allTasks)) {
        if (qobject_cast<InstallationTask *>(task) && (task->id() == taskId)) {
            static_cast<InstallationTask *>(task)->acknowledge();
            break;
        }
    }
}

/*!
    \qmlmethod string ApplicationInstaller::removePackage(string id, bool keepDocuments, bool force)

    Uninstalls the application identified by \a id. Normally, the documents directory of the
    application is deleted on removal, but this can be prevented by setting \a keepDocuments to \c true.

    The actual removal will happen asynchronously in the background. The ApplicationInstaller will
    emit the signals \l taskStarted, \l taskProgressChanged, \l taskFinished, \l taskFailed and \l
    taskStateChanged for the returned \c taskId when applicable.

    Normally, \a force should only be set to \c true if a previous call to removePackage() failed.
    This may be necessary if the installation process was interrupted, or if an SD card got lost
    or has file-system issues.

    Returns a unique \c taskId. This can also be an empty string, if the task could not be created
    (in this case, no signals will be emitted).
*/
QString ApplicationInstaller::removePackage(const QString &id, bool keepDocuments, bool force)
{
    AM_TRACE(LogInstaller, id, keepDocuments)
    AM_AUTHENTICATE_DBUS(QString)

    if (const Application *a = ApplicationManager::instance()->fromId(id)) {
        if (const InstallationReport *report = a->installationReport()) {
            const InstallationLocation &il = installationLocationFromId(report->installationLocationId());

            if (il.isValid() && (il.id() == report->installationLocationId()))
                return enqueueTask(new DeinstallationTask(a, il, force, keepDocuments));
        }
    }
    return QString();
}


/*!
    \qmlmethod string ApplicationInstaller::taskState(string taskId)

    Returns a string describing the current state of the installation task identified by \a taskId.
    \l {TaskStates}{See here} for a list of valid task states.

    Returns an empty string if the \a taskId is invalid.
*/
QString ApplicationInstaller::taskState(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)

    auto allTasks = d->taskQueue;
    allTasks.append(d->activeTask);

    for (const AsynchronousTask *task : qAsConst(allTasks)) {
        if (task && (task->id() == taskId))
            return AsynchronousTask::stateToString(task->state());
    }
    return QString();
}

/*!
    \qmlmethod string ApplicationInstaller::taskApplicationId(string taskId)

    Returns the application id associated with the task identified by \a taskId. The task may not
    have a valid application id at all times though and in this case the function will return an
    empty string (this will be the case for installations before the taskRequestingInstallationAcknowledge
    signal has been emitted).

    Returns an empty string if the \a taskId is invalid.
*/
QString ApplicationInstaller::taskApplicationId(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)

    auto allTasks = d->taskQueue;
    allTasks.append(d->activeTask);

    for (const AsynchronousTask *task : qAsConst(allTasks)) {
        if (task && (task->id() == taskId))
            return task->applicationId();
    }
    return QString();
}

/*!
    \qmlmethod bool ApplicationInstaller::cancelTask(string taskId)

    Tries to cancel the installation task identified by \a taskId.

    Returns \c true if the task was canceled, \c false otherwise.
*/
bool ApplicationInstaller::cancelTask(const QString &taskId)
{
    AM_TRACE(LogInstaller, taskId)
    AM_AUTHENTICATE_DBUS(bool)

    if (d->activeTask && d->activeTask->id() == taskId)
        return d->activeTask->cancel();

    foreach (AsynchronousTask *task, d->taskQueue) {
        if (task->id() == taskId) {
            task->forceCancel();
            task->deleteLater();

            handleFailure(task);

            d->taskQueue.removeOne(task);
            triggerExecuteNextTask();
            return true;
        }
    }
    return false;
}

/*!
  \qmlmethod int ApplicationInstaller::compareVersions(string version1, string version2)

  Convenience method for app-store implementation for comparing version numbers, as the actual
  version comparison algorithm is not trivial.

  Returns \c -1, \c 0 or \c 1 if \a version1 is smaller than, equal to, or greater than \a version2
  (similar to how \c strcmp() works).
*/
int ApplicationInstaller::compareVersions(const QString &version1, const QString &version2)
{
    return versionCompare(version1, version2);
}


// this is a simple helper class - we need this to be able to run the filesystem (un)mounting
// in a separate thread to avoid blocking the UI and D-Bus
class ActivationHelper : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    enum Mode { Activate, Deactivate, IsActivated };

    static bool run(Mode mode, const QString &id)
    {
        if (!SudoClient::instance())
            return false;

        QDir imageMountDir = ApplicationInstaller::instance()->applicationImageMountDirectory();

        if (id.isEmpty() || !imageMountDir.exists())
            return false;

        const InstallationLocation &il = ApplicationInstaller::instance()->installationLocationFromApplication(id);
        if (!il.isValid() || !il.isRemovable())
            return false;

        QString imageName = il.installationPath() + id + qSL(".appimg");
        if (!QFile::exists(imageName))
            return false;

        QString mountPoint = imageMountDir.absoluteFilePath(id);
        QString mountedDevice;
        auto currentMounts = mountedDirectories();
        bool isMounted = currentMounts.contains(mountPoint);

        switch (mode) {

        case Activate:
            if (isMounted)
                return false;
            mountPoint.clear(); // not mounted yet
            break;
        case Deactivate:
            if (!isMounted)
                return false;
            mountedDevice = currentMounts.value(mountPoint);
            break;
        case IsActivated:
            return isMounted;
        default:
            return false;
        }

        ActivationHelper *a = new ActivationHelper(id, imageName, imageMountDir, mountPoint, mountedDevice);
        QThread *t = new QThread();
        a->moveToThread(t);
        connect(t, &QThread::started, a, [t, a, mode]() {
            a->m_status = (mode == Activate) ? a->mount()
                                             : a->unmount();
            t->quit();
        });
        connect(t, &QThread::finished, ApplicationInstaller::instance(), [id, t, a, mode]() {
            if (mode == Activate) {
                emit ApplicationInstaller::instance()->packageActivated(id, a->m_status);
                if (!a->m_status)
                    qCCritical(LogSystem) << "ERROR: failed to activate package" << id << ":" << a->m_errorString;
            } else {
                emit ApplicationInstaller::instance()->packageDeactivated(id, a->m_status);
                if (!a->m_status)
                    qCCritical(LogSystem) << "ERROR: failed to de-activate package" << id << ":" << a->m_errorString;
            }
            delete a;
            t->deleteLater();
        });
        t->start();
        return true;
    }

    ActivationHelper(const QString &id, const QString &imageName, const QDir &imageMountDir,
                     const QString &mountPoint, const QString &mountedDevice)
        : m_applicationId(id)
        , m_imageName(imageName)
        , m_imageMountDir(imageMountDir)
        , m_mountPoint(mountPoint)
        , m_mountedDevice(mountedDevice)
    { }

    bool mount()
    {
        SudoClient *root = SudoClient::instance();

        // m_mountPoint is only set, if the image is/was actually mounted
        QString mountDir = m_imageMountDir.filePath(m_applicationId);

        try {
            QFileInfo fi(mountDir);
            if (!fi.isDir() && !QDir(fi.absolutePath()).mkpath(fi.fileName()))
                throw Exception("could not create mountpoint directory %1").arg(mountDir);

            m_mountedDevice = root->attachLoopback(m_imageName, true /*ro*/);
            if (m_mountedDevice.isEmpty())
                throw Exception("could not create a new loopback device: %1").arg(root->lastError());

            if (!root->mount(m_mountedDevice, mountDir, true /*ro*/))
                throw Exception("could not mount application image %1 to %2: %3").arg(mountDir, m_mountedDevice, root->lastError());
            m_mountPoint = mountDir;

            // better be safe than sorry - make sure this is the exact same version we installed
            // (we cannot check every single file, but we at least make sure that info.yaml matches)
            QFile manifest1(ApplicationInstaller::instance()->manifestDirectory().absoluteFilePath(m_applicationId + qSL("/info.yaml")));
            QFile manifest2(QDir(mountDir).absoluteFilePath(qSL("info.yaml")));

            if ((manifest1.size() != manifest2.size())
                    || (manifest1.size() > (16 * 1024))
                    || !manifest1.open(QFile::ReadOnly)
                    || !manifest2.open(QFile::ReadOnly)
                    || (manifest1.readAll() != manifest2.readAll())) {
                throw Exception("the info.yaml files in the manifest directory and within the application image do not match");
            }
            return true;
        } catch (const Exception &e) {
            unmount();
            m_errorCode = e.errorCode();
            m_errorString = e.errorString();
            return false;
        }
    }


    bool unmount()
    {
        SudoClient *root = SudoClient::instance();

        try {
            if (!m_mountPoint.isEmpty() && !root->unmount(m_mountPoint))
                throw Exception("could not unmount the application image at %1: %2").arg(m_mountPoint, root->lastError());

            if (!m_mountedDevice.isEmpty() && !root->detachLoopback(m_mountedDevice))
                throw Exception("could not remove loopback device %1: %2").arg(m_mountedDevice, root->lastError());

            // m_mountPoint is only set, if the image is/was actually mounted
            QString mountDir = m_imageMountDir.filePath(m_applicationId);
            if (QFileInfo(mountDir).isDir() && !m_imageMountDir.rmdir(m_applicationId))
                throw Exception("could not remove mount-point directory %1").arg(mountDir);

            return true;
        } catch (const Exception &e) {
            m_errorCode = e.errorCode();
            m_errorString = e.errorString();
            return false;
        }
    }

private:
    QString m_applicationId;
    QString m_imageName;
    QDir m_imageMountDir;
    QString m_mountPoint;
    QString m_mountedDevice;
    bool m_status = false;
    Error m_errorCode = Error::None;
    QString m_errorString;
};

bool ApplicationInstaller::doesPackageNeedActivation(const QString &id)
{
    const InstallationLocation &il = installationLocationFromApplication(id);
    return il.isValid() && il.isRemovable();
}

bool ApplicationInstaller::isPackageActivated(const QString &id)
{
    return ActivationHelper::run(ActivationHelper::IsActivated, id);
}

bool ApplicationInstaller::activatePackage(const QString &id)
{
    return ActivationHelper::run(ActivationHelper::Activate, id);
}

bool ApplicationInstaller::deactivatePackage(const QString &id)
{
    return ActivationHelper::run(ActivationHelper::Deactivate, id);
}


QString ApplicationInstaller::enqueueTask(AsynchronousTask *task)
{
    d->taskQueue.enqueue(task);
    triggerExecuteNextTask();
    return task->id();
}

void ApplicationInstaller::triggerExecuteNextTask()
{
    if (!QMetaObject::invokeMethod(this, "executeNextTask", Qt::QueuedConnection))
        qCCritical(LogSystem) << "ERROR: failed to invoke method checkQueue";
}

void ApplicationInstaller::executeNextTask()
{
    if (d->activeTask || d->taskQueue.isEmpty())
        return;

    AsynchronousTask *task = d->taskQueue.dequeue();

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

    connect(task, &AsynchronousTask::stateChanged, this, [this, task](AsynchronousTask::State newState) {
        emit taskStateChanged(task->id(), AsynchronousTask::stateToString(newState));
    });

    connect(task, &AsynchronousTask::progress, this, [this, task](qreal p) {
        emit taskProgressChanged(task->id(), p);
        QMetaObject::invokeMethod(ApplicationManager::instance(),
                                  "progressingApplicationInstall",
                                  Qt::DirectConnection,
                                  Q_ARG(QString, task->applicationId()),
                                  Q_ARG(double, p));
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

        //task->deleteLater();
        delete task;
        triggerExecuteNextTask();
    });

    if (qobject_cast<InstallationTask *>(task)) {
        connect(static_cast<InstallationTask *>(task), &InstallationTask::finishedPackageExtraction, this, [this, task]() {
            qCDebug(LogInstaller) << "emit blockingUntilInstallationAcknowledge" << task->id();
            emit taskBlockingUntilInstallationAcknowledge(task->id());
        });
    }


    d->activeTask = task;
    task->setState(AsynchronousTask::Executing);
    task->start();
}

void ApplicationInstaller::handleFailure(AsynchronousTask *task)
{
    qCDebug(LogInstaller) << "emit failed" << task->id() << task->errorCode() << task->errorString();
    emit taskFailed(task->id(), int(task->errorCode()), task->errorString());
}


bool removeRecursiveHelper(const QString &path)
{
    if (ApplicationInstaller::instance()->isApplicationUserIdSeparationEnabled() && SudoClient::instance())
        return SudoClient::instance()->removeRecursive(path);
    else
        return recursiveOperation(path, SafeRemove());
}

QT_END_NAMESPACE_AM
