// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QLocale>

#include "package.h"
#include "packageinfo.h"
#include "applicationinfo.h"
#include "application.h"
#include "utilities.h"

/*!
    \qmltype PackageObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a package known to the application manager.

    Each instance of this class represents a single package known to the application manager.

    Most of the read-only properties map directly to values read from the package's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.

    Items of this type are not creatable from QML code. Only functions and properties of
    PackageManager will return pointers to this class.

    Make sure to \b not save references to a PackageObject across function calls: packages can
    be deinstalled at any time, invalidating your reference. In case you do need a persistent
    handle, use the id string.
*/

/*!
    \qmlproperty string PackageObject::id
    \readonly

    This property returns the unique id of the package.
*/
/*!
    \qmlproperty bool PackageObject::builtIn
    \readonly

    This property describes, if this package is part of the built-in set of packages of the
    current System UI.
*/
/*!
    \qmlproperty bool PackageObject::builtInHasRemovableUpdate
    \readonly

    This property describes, if this package is part of the built-in set of packages of the
    current System UI \b and if there is currently an update installed that shadows the original
    built-in package contents.

    \sa builtIn
*/
/*!
    \qmlproperty url PackageObject::icon
    \readonly

    The URL of the package's icon - can be used as the source property of an \l Image.
*/
/*!
    \qmlproperty string PackageObject::version
    \readonly

    Holds the version of the package as a string.
*/
/*!
    \qmlproperty string PackageObject::name
    \readonly

    Returns the localized name of the package - as provided in the info.yaml file - in the currently
    active locale.

    This is a convenience property, that takes the mapping returned by the \l names property,
    and then tries to return the value for these keys if available: first the current locale's id,
    then \c en_US, then \c en and lastly the first available key.

    If no mapping is available, this will return the \l id.
*/
/*!
    \qmlproperty var PackageObject::names
    \readonly

    Returns an object with all the language code to localized name mappings as provided in the
    package's info.yaml file.
*/
/*!
    \qmlproperty string PackageObject::description
    \readonly

    Returns the localized description of the package - as provided in the info.yaml file - in the
    currently active locale.

    This property uses the same algorithm as the \l name property, but for the description.
*/
/*!
    \qmlproperty var PackageObject::descriptions
    \readonly

    Returns an object with all the language code to localized description mappings as provided in
    the package's info.yaml file.
*/
/*!
    \qmlproperty list<string> PackageObject::categories
    \readonly

    A list of category names the package should be associated with. This is mainly for the
    automated app-store uploads as well as displaying the package within a fixed set of
    categories in the System UI.
*/
/*!
    \qmlproperty enumeration PackageObject::state
    \readonly

    This property holds the current installation state of the package. It can be one of:

    \list
    \li PackageObject.Installed - The package is completely installed and ready to be used.
    \li PackageObject.BeingInstalled - The package is currently in the process of being installed.
    \li PackageObject.BeingUpdated - The package is currently in the process of being updated.
    \li PackageObject.BeingDowngraded - The package is currently in the process of being downgraded.
                                        That can only happen for a built-in package that was previously
                                        upgraded. It will then be brought back to its original, built-in,
                                        version and its state will go back to PackageObject.Installed.
    \li PackageObject.BeingRemoved - The package is currently in the process of being removed.
    \endlist
*/
/*!
    \qmlproperty bool PackageObject::blocked
    \readonly

    Describes if this package is currently blocked: being blocked means that all applications in
    the package are stopped and are prevented from being started while in this state.
    This is normally only the case while an update is being applied.
*/
/*!
    \qmlproperty list<ApplicationObject> PackageObject::applications
    \readonly

    Returns a list of ApplicationObjects that are part of this package.
*/

QT_BEGIN_NAMESPACE_AM

Package::Package(PackageInfo *packageInfo, State initialState)
    : m_info(packageInfo)
    , m_state(initialState)
{
    // calling block() would lead to the AM waiting for the not-yet installed apps to quit
    if (initialState == BeingInstalled)
        m_blocked = 1;
}

QString Package::id() const
{
    return info()->id();
}

bool Package::isBuiltIn() const
{
    return m_info->isBuiltIn();
}

bool Package::builtInHasRemovableUpdate() const
{
    return isBuiltIn() && m_updatedInfo;
}

QString Package::version() const
{
    return info()->version();
}

QString Package::name() const
{
    return translateFromMap(info()->names(), id());
}

QVariantMap Package::names() const
{
    QVariantMap names;
    const auto nmap = info()->names();
    for (auto it = nmap.cbegin(); it != nmap.cend(); ++it)
        names.insert(it.key(), it.value());
    return names;
}

QString Package::description() const
{
    return translateFromMap(info()->descriptions());
}

QVariantMap Package::descriptions() const
{
    QVariantMap descriptions;
    const auto dmap = info()->descriptions();
    for (auto it = dmap.cbegin(); it != dmap.cend(); ++it)
        descriptions.insert(it.key(), it.value());
    return descriptions;
}

QStringList Package::categories() const
{
    return info()->categories();
}

QVector<Application *> Package::applications() const
{
    return m_applications;
}

QUrl Package::icon() const
{
    if (info()->icon().isEmpty())
        return QUrl();

    QDir dir;
    switch (state()) {
    default:
    case Installed:
        dir = info()->baseDir();
        break;
    case BeingInstalled:
    case BeingUpdated:
        dir = QDir(info()->baseDir().absolutePath() + QLatin1Char('+'));
        break;
    case BeingRemoved:
        dir = QDir(info()->baseDir().absolutePath() + QLatin1Char('-'));
        break;
    }
    return QUrl::fromLocalFile(dir.absoluteFilePath(info()->icon()));
}

void Package::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void Package::setProgress(qreal progress)
{
    m_progress = progress;
}

PackageInfo *Package::info() const
{
    return m_updatedInfo ? m_updatedInfo : m_info;
}

PackageInfo *Package::baseInfo() const
{
    return m_info;
}

PackageInfo *Package::updatedInfo() const
{
    return m_updatedInfo;
}

PackageInfo *Package::setUpdatedInfo(PackageInfo *info)
{
    Q_ASSERT(!info || (m_info && info->id() == m_info->id()));
    Q_ASSERT(info != m_updatedInfo);

    auto old = m_updatedInfo;
    m_updatedInfo = info;
    emit bulkChange();
    return old;
}

PackageInfo *Package::setBaseInfo(PackageInfo *info)
{
    Q_ASSERT(info != m_info);

    auto old = m_info;
    m_info = info;
    emit bulkChange();
    return old;
}

bool Package::isBlocked() const
{
    return m_blocked > 0;
}

bool Package::block()
{
    bool blockedNow = (m_blocked.fetchAndAddOrdered(1) == 0);
    if (blockedNow) {
        m_blockedApps = info()->applications();
        m_blockedAppsCount = m_blockedApps.count();
        emit blockedChanged(true);
    }
    return blockedNow;
}

bool Package::unblock()
{
    bool unblockedNow = (m_blocked.fetchAndSubOrdered(1) == 1);
    if (unblockedNow) {
        m_blockedApps.clear();
        m_blockedAppsCount = 0;
        emit blockedChanged(false);
    }
    return unblockedNow;

}

void Package::applicationStoppedDueToBlock(const QString &appId)
{
    if (!isBlocked())
        return;

    auto it = std::find_if(m_blockedApps.cbegin(), m_blockedApps.cend(), [appId](const ApplicationInfo *appInfo) {
        return appInfo->id() == appId;
    });
    if (it != m_blockedApps.cend())
        m_blockedApps.removeOne(*it);
    m_blockedAppsCount = m_blockedApps.count();
}

bool Package::areAllApplicationsStoppedDueToBlock() const
{
    return isBlocked() && !m_blockedAppsCount;
}

void Package::addApplication(Application *application)
{
    m_applications.append(application);
    emit applicationsChanged();
}

void Package::removeApplication(Application *application)
{
    m_applications.removeAll(application);
    emit applicationsChanged();
}

QT_END_NAMESPACE_AM

#include "moc_package.cpp"
