// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <qqml.h>
#include <QQmlEngine>

#include "applicationinstaller.h"


/*!
    \qmltype ApplicationInstaller
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief (obsolete) The package installation/removal/update part of the application manager.

    \b{This class is obsolete since 5.14. It is provided to keep old source code working. We
    strongly advise against using it in new code. Use the new PackageManager instead.}

    Please be aware that the new PackageManager API expects package-IDs instead of application-IDs
    as is the case with this class. Since the new package format adds support for more than one
    application per package, this legacy API here has its limits in supporting the new features.
    It is strongly advised to only use the PackageManager API when dealing with new-style packages.

    The background tasks handling the (de)installation process are the same between the old and
    new API, so the documentation for the \l{Task States}{task states in PackageManager}
    holds true for this class as well.

    If possible, all function calls and signals are forwarded to and from the PackageManager API.
    Exceptions are documented below in the detailed function and signal descriptions.

    \note The ApplicationInstaller singleton and its corresponding DBus API are only available if
          you specify a base directory for installed application manifests. See \l{Configuration}
          for details.
*/

/*!
    \qmlsignal ApplicationInstaller::taskStateChanged(string taskId, string newState)
    \qmlobsolete

    Use PackageManager::taskStateChanged(\a taskId, \a newState).
*/

/*!
    \qmlsignal ApplicationInstaller::taskStarted(string taskId)
    \qmlobsolete

    Use PackageManager::taskStarted(\a taskId).
*/

/*!
    \qmlsignal ApplicationInstaller::taskFinished(string taskId)
    \qmlobsolete

    Use PackageManager::taskFinished(\a taskId).
*/

/*!
    \qmlsignal ApplicationInstaller::taskFailed(string taskId)
    \qmlobsolete

    Use PackageManager::taskFailed(\a taskId).
*/

/*!
    \qmlsignal ApplicationInstaller::taskRequestingInstallationAcknowledge(string taskId, object application, object packageExtraMetaData, object packageExtraSignedMetaData)
    \qmlobsolete

    Use PackageManager::taskRequestingInstallationAcknowledge(\a taskId, \c package, \a
    packageExtraMetaData, \a packageExtraSignedMetaData).

    A subset of the contents of the package's manifest file is supplied via \a application as a
    JavaScript object. This object is constructed from the replacement function's Package object
    parameter. Since the manifest files changed in 5.14, only the following legacy keys are reported
    through this object: \c id, \c version, \c icon, \c displayIcon, \c name, \c displayName,
    \c baseDir, \c codeDir, \c manifestDir and \c installationLocationId.
*/

/*!
    \qmlsignal ApplicationInstaller::taskBlockingUntilInstallationAcknowledge(string taskId)
    \qmlobsolete

    Use PackageManager::taskBlockingUntilInstallationAcknowledge(\a taskId).
*/

/*!
    \qmlsignal ApplicationInstaller::taskProgressChanged(string taskId, qreal progress)
    \qmlobsolete

    Use PackageManager::taskProgressChanged(\a taskId, \a progress).
*/

QT_BEGIN_NAMESPACE_AM

ApplicationInstaller *ApplicationInstaller::s_instance = nullptr;

ApplicationInstaller::ApplicationInstaller(PackageManager *pm, QObject *parent)
    : QObject(parent)
    , m_pm(pm)
{
#if !defined(AM_DISABLE_INSTALLER)
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
                     this, [this](const QString &taskId, Package *package,
                     const QVariantMap &packageExtraMetaData,
                     const QVariantMap &packageExtraSignedMetaData) {
        QVariantMap nameMap;
        auto names = package->names();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it)
            nameMap.insert(it.key(), it.value());

        QString baseDir = package->info()->baseDir().absolutePath();

        QVariantMap applicationData {
            { qSL("id"), package->id() },
            { qSL("version"), package->version() },
            { qSL("icon"), package->icon() },
            { qSL("displayIcon"), package->icon() },             // 1.0 compatibility
            { qSL("name"), nameMap },
            { qSL("displayName"), nameMap },                     // 1.0 compatibility
            { qSL("baseDir"), baseDir },
            { qSL("codeDir"), baseDir },                         // 5.12 compatibility
            { qSL("manifestDir"), baseDir },                     // 5.12 compatibility
            { qSL("installationLocationId"), qSL("internal-0") } // 5.13 compatibility
        };

        emit taskRequestingInstallationAcknowledge(taskId, applicationData, packageExtraMetaData,
                                                   packageExtraSignedMetaData);
    });
    QObject::connect(pm, &PackageManager::taskBlockingUntilInstallationAcknowledge,
                     this, &ApplicationInstaller::taskBlockingUntilInstallationAcknowledge);
#endif
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
    \qmlobsolete

    This function became obsolete, because the new architecture only supports one single
    installation location: it now always returns \c internal-0.
*/

/*!
    \qmlmethod string ApplicationInstaller::installationLocationIdFromApplication(string id)
    \qmlobsolete

    This function became obsolete, because the new architecture only supports one single
    installation location: it now always returns \c internal-0 if the application identified by
    \a id is installed or an empty string otherwise.
*/

/*!
    \qmlmethod object ApplicationInstaller::getInstallationLocation(string installationLocationId)
    \qmlobsolete

    This function became obsolete, because the new architecture only supports one single
    installation location (\c internal-0).

    Returns an empty object in case the \a installationLocationId is not valid.
*/

/*!
    \qmlmethod int ApplicationInstaller::installedApplicationSize(string id)
    \qmlobsolete

    Use PackageManager::installedPackageSize(), same return value.

    \note The replacement function PackageManager::installedPackageSize expects a
          \b package-ID as parameter, while this function wants an \b application-ID as \a id.
*/

/*!
    \qmlmethod var ApplicationInstaller::installedApplicationExtraMetaData(string id)
    \qmlobsolete

    Use PackageManager::installedPackageExtraMetaData(), same return value.

    \note The replacement function PackageManager::installedPackageExtraMetaData expects a
          \b package-ID as parameter, while this function wants an \b application-ID as \a id.
*/

/*!
    \qmlmethod var ApplicationInstaller::installedApplicationExtraSignedMetaData(string id)
    \qmlobsolete

    Use PackageManager::installedPackageExtraSignedMetaData(), same return value.

    \note The replacement function PackageManager::installedPackageExtraSignedMetaData expects a
          \b package-ID as parameter, while this function wants an \b application-ID as \a id.
*/

/*!
    \qmlmethod string ApplicationInstaller::startPackageInstallation(string installationLocationId, string sourceUrl)
    \qmlobsolete

    Use PackageManager::startPackageInstallation(\a sourceUrl), same return value.

    \note The replacement function PackageManager::startPackageInstallation is missing the first
          parameter (\a installationLocationId). This became obsolete, because the new architecture
          only supports one single installation location.
*/

/*!
    \qmlmethod void ApplicationInstaller::acknowledgePackageInstallation(string taskId)
    \qmlobsolete

    Use PackageManager::acknowledgePackageInstallation(\a taskId).
*/

/*!
    \qmlmethod string ApplicationInstaller::removePackage(string id, bool keepDocuments, bool force)
    \qmlobsolete

    Use PackageManager::removePackage(\c packageId, \a keepDocuments, \a force), same return value.

    \note The replacement function PackageManager::removePackage expects a \b package-ID as
          parameter, while this function wants an \b application-ID as \a id.
*/

/*!
    \qmlmethod enumeration ApplicationInstaller::taskState(string taskId)
    \qmlobsolete

    Use PackageManager::taskState(\a taskId), same return value.
*/

/*!
    \qmlmethod string ApplicationInstaller::taskApplicationId(string taskId)
    \qmlobsolete

    Use PackageManager::taskPackageId(\a taskId).

    \note The replacement function PackageManager::taskPackageId returns an \b package-ID,
          while this function returns an \b application-ID.
*/


/*!
    \qmlmethod list<string> ApplicationInstaller::activeTaskIds()
    \qmlobsolete

    Use PackageManager::activeTaskIds(), same return value.
*/

/*!
    \qmlmethod bool ApplicationInstaller::cancelTask(string taskId)
    \qmlobsolete

    Use PackageManager::cancelTask(\a taskId), same return value.
*/

/*!
    \qmlmethod int ApplicationInstaller::compareVersions(string version1, string version2)
    \qmlobsolete

    Use PackageManager::compareVersions(\a version1, \a version2), same return value.
*/

/*!
    \qmlmethod int ApplicationInstaller::validateDnsName(string name, int minimalPartCount)
    \qmlobsolete

    Use PackageManager::validateDnsName(\a name, \a minimalPartCount), same return value.
*/

QT_END_NAMESPACE_AM

#include "moc_applicationinstaller.cpp"
