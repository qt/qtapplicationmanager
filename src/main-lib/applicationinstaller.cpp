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

#include <QCoreApplication>
#include <qqml.h>
#include <QQmlEngine>

#include "applicationinstaller.h"


/*!
    \qmltype ApplicationInstaller
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The package installation/removal/update part of the application-manager.

    The ApplicationInstaller singleton type handles the package installation
    part of the application manager. It provides both a DBus and QML APIs for
    all of its functionality.

    \note The ApplicationInstaller singleton and its corresponding DBus API are only available if you
          specify a base directory for installed application manifests. See \l{Configuration} for details.

    \target TaskStates

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

// THIS IS MISSING AN EXAMPLE!

/*!
    \qmlsignal ApplicationInstaller::taskStateChanged(string taskId, string newState)

    This signal is emitted when the state of the task identified by \a taskId changes. The
    new state is supplied in the parameter \a newState.

    \sa taskState()
*/

/*!
    \qmlsignal ApplicationInstaller::taskStarted(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Executing state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskFinished(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Finished state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskFailed(string taskId)

    This signal is emitted when the task identified by \a taskId enters the \c Failed state.

    \sa taskStateChanged()
*/

/*!
    \qmlsignal ApplicationInstaller::taskRequestingInstallationAcknowledge(string taskId, object application, object packageExtraMetaData, object packageExtraSignedMetaData)

    This signal is emitted when the installation task identified by \a taskId has received enough
    meta-data to be able to emit this signal. The task may be in either \c Executing or \c
    AwaitingAcknowledge state.

    The contents of the package's manifest file are supplied via \a application as a JavaScript object.
    Please see the \l {ApplicationManager Roles}{role names} for the expected object fields.

    In addition, the package's extra meta-data (signed and unsinged) is also supplied via \a
    packageExtraMetaData and \a packageExtraSignedMetaData respectively as JavaScript objects.
    Both these objects are optional and need to be explicitly either populated during an
    application's packaging step or added by an intermediary app-store server.
    By default, both will just be empty.

    Following this signal, either cancelTask() or acknowledgePackageInstallation() has to be called
    for this \a taskId, to either cancel the installation or try to complete it.

    The ApplicationInstaller has two convenience functions to help the System-UI with verifying the
    meta-data: compareVersions() and, in case you are using reverse-DNS notation for application-ids,
    validateDnsName().

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

ApplicationInstaller::ApplicationInstaller(PackageManager *pm, QObject *parent)
    : QObject(parent)
    , m_pm(pm)
{
    // connect the APIs of the PackageManager and ApplicationInstaller
    QObject::connect(pm, &PackageManager::taskProgressChanged,
                     this, &ApplicationInstaller::taskProgressChanged);
    QObject::connect(pm, &PackageManager::taskStarted,
                     this, &ApplicationInstaller::taskStarted);
    QObject::connect(pm, &PackageManager::taskFinished,
                     this, &ApplicationInstaller::taskFinished);
    QObject::connect(pm, &PackageManager::taskFailed,
                     this, &ApplicationInstaller::taskFailed);
    QObject::connect(pm, &PackageManager::taskStateChanged,
                     this, &ApplicationInstaller::taskStateChanged);
    QObject::connect(pm, &PackageManager::taskRequestingInstallationAcknowledge,
                     this, &ApplicationInstaller::taskRequestingInstallationAcknowledge);
    QObject::connect(pm, &PackageManager::taskBlockingUntilInstallationAcknowledge,
                     this, &ApplicationInstaller::taskBlockingUntilInstallationAcknowledge);
}

ApplicationInstaller::~ApplicationInstaller()
{
    s_instance = nullptr;
}

ApplicationInstaller *ApplicationInstaller::createInstance(PackageManager *pm)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationInstaller::createInstance() was called a second time.");


    qmlRegisterSingletonType<ApplicationInstaller>("QtApplicationManager.SystemUI", 2, 0, "ApplicationInstaller",
                                                   &ApplicationInstaller::instanceForQml);

    return s_instance = new ApplicationInstaller(pm, QCoreApplication::instance());
}

ApplicationInstaller *ApplicationInstaller::instance()
{
    if (!s_instance)
        qFatal("ApplicationInstaller::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationInstaller::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

/*!
   \qmlmethod list<string> ApplicationInstaller::installationLocationIds()

   Retuns a list of all known installation location ids. Calling getInstallationLocation() on one of
   the returned identifiers will yield specific information about the individual installation locations.
*/

/*!
   \qmlmethod string ApplicationInstaller::installationLocationIdFromApplication(string id)

   Returns the installation location id for the application identified by \a id. Returns
   an empty string in case the application is not installed.

   \sa installationLocationIds()
*/

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

/*!
   \qmlmethod int ApplicationInstaller::installedApplicationSize(string id)

   Returns the size in bytes that the application identified by \a id is occupying on the storage
   device.

   Returns \c -1 in case the application \a id is not valid, or the application is not installed.
*/

/*!
   \qmlmethod var ApplicationInstaller::installedApplicationExtraMetaData(string id)

   Returns a map of all extra metadata in the package header of the application identified by \a id.

   Returns an empty map in case the application \a id is not valid, or the application is not installed.
*/

/*!
   \qmlmethod var ApplicationInstaller::installedApplicationExtraSignedMetaData(string id)

   Returns a map of all signed extra metadata in the package header of the application identified
   by \a id.

   Returns an empty map in case the application \a id is not valid, or the application is not installed.
*/

/*! \internal
  Type safe convenience function, since DBus does not like QUrl
*/

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

/*!
    \qmlmethod void ApplicationInstaller::acknowledgePackageInstallation(string taskId)

    Calling this function enables the installer to complete the installation task identified by \a
    taskId. Normally, this function is called after receiving the taskRequestingInstallationAcknowledge()
    signal, and the user and/or the program logic decided to proceed with the installation.

    \sa startPackageInstallation()
 */

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

/*!
    \qmlmethod enumeration ApplicationInstaller::taskState(string taskId)

    Returns the current state of the installation task identified by \a taskId.
    \l {TaskStates}{See here} for a list of valid task states.

    Returns \c ApplicationInstaller.Invalid if the \a taskId is invalid.
*/

/*!
    \qmlmethod string ApplicationInstaller::taskApplicationId(string taskId)

    Returns the application id associated with the task identified by \a taskId. The task may not
    have a valid application id at all times though and in this case the function will return an
    empty string (this will be the case for installations before the taskRequestingInstallationAcknowledge
    signal has been emitted).

    Returns an empty string if the \a taskId is invalid.
*/


/*!
    \qmlmethod list<string> ApplicationInstaller::activeTaskIds()

    Retuns a list of all currently active (as in not yet finished or failed) installation task ids.
*/

/*!
    \qmlmethod bool ApplicationInstaller::cancelTask(string taskId)

    Tries to cancel the installation task identified by \a taskId.

    Returns \c true if the task was canceled, \c false otherwise.
*/

/*!
    \qmlmethod int ApplicationInstaller::compareVersions(string version1, string version2)

    Convenience method for app-store implementations or taskRequestingInstallationAcknowledge()
    callbacks for comparing version numbers, as the actual version comparison algorithm is not
    trivial.

    Returns \c -1, \c 0 or \c 1 if \a version1 is smaller than, equal to, or greater than \a
    version2 (similar to how \c strcmp() works).
*/

/*!
    \qmlmethod int ApplicationInstaller::validateDnsName(string name, int minimalPartCount)

    Convenience method for app-store implementations or taskRequestingInstallationAcknowledge()
    callbacks for checking if the given \a name is a valid DNS (or reverse-DNS) name according to
    RFC 1035/1123. If the optional parameter \a minimalPartCount is specified, this function will
    also check if \a name contains at least this amount of parts/sub-domains.

    Returns \c true if the name is a valid DNS name or \c false otherwise.
*/

QT_END_NAMESPACE_AM
