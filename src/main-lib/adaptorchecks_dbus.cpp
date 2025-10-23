// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "dbuspolicy.h"
#include "dbuscontextadaptor.h"
#include "applicationmanager.h"
#include "packagemanager.h"
#include "adaptorchecks_dbus.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM
namespace DBusAdaptorChecks {

void checkDBusAccessPrivate(const QDBusAbstractAdaptor *a, const char *function)
{
    if (isDevelopmentModeBus(a)) {
        switch (PackageManager::instance()->developmentMode()) {
        default:
        case PackageManager::DevelopmentMode::Disabled:
            // there should not be a bus at all, if development mode is disabled
            Q_UNREACHABLE();
            throw Exception("Development mode is disabled");

        case PackageManager::DevelopmentMode::System:
            break;

        case PackageManager::DevelopmentMode::Application:
            if (!PackageManager::instance()->developerCertificate().isValid())
                throw Exception("Developer certificate is not set");
            break;
        }
    } else {
        try {
            DBusPolicy::instance()->check(a, function);
        } catch (const Exception &e) {
            throw Exception("Function %1 is not accessible: %2")
                .arg(QString::fromLatin1(function)).arg(e.errorString());
        }
    }
}

void checkInstallerPrivate(const QDBusAbstractAdaptor *)
{
    if (!PackageManager::instance()->installationEnabled())
        throw Exception("The application-manager was compiled without the installer part");
}

void checkDevelopmentModeSystemPrivate(const QDBusAbstractAdaptor *a)
{
    if (isDevelopmentModeBus(a) && (PackageManager::instance()->developmentMode()
                                    != PackageManager::DevelopmentMode::System)) {
        throw Exception("This function is only available in development mode 'system'");
    }
}

void checkTaskAccessPrivate(const QDBusAbstractAdaptor *a, const QString &taskId)
{
    const auto task = PackageManager::instance()->taskFromId(taskId);
    if (!task)
        throw Exception("Unknown task id '%1'").arg(taskId);

    if (!isDevelopmentModeBus(a))
        return;

    switch (PackageManager::instance()->developmentMode()) {
    default:
    case PackageManager::DevelopmentMode::Disabled:
        Q_UNREACHABLE();
        throw Exception("Development mode is disabled");

    case PackageManager::DevelopmentMode::System:
        return;

    case PackageManager::DevelopmentMode::Application:
        if (task->origin() == AsynchronousTask::Origin::System)
            throw Exception("Task id '%1' is not accessible").arg(taskId);
        break;
    }
}

void checkApplicationAccessPrivate(const QDBusAbstractAdaptor *a, const QString &applicationId)
{
    bool isSysUI = (applicationId == u":sysui:");
    QString packageId;

    if (!isSysUI) {
        if (auto *app = ApplicationManager::instance()->application(applicationId))
            packageId = app->packageInfo()->id();
        else
            throw Exception("Unknown application id '%1'").arg(applicationId);
    }

    if (!isDevelopmentModeBus(a))
        return;

    if (PackageManager::instance()->developmentMode() == PackageManager::DevelopmentMode::Application) {
        const auto cert = PackageManager::instance()->developerCertificate();
        Q_ASSERT(cert.isValid());

        if (isSysUI || !cert.matchPackageId(packageId)) {
            throw Exception("Application id '%1' is not accessible using the currently set developer certificate")
                .arg(applicationId);
        }
    }
}

void checkPackageAccessPrivate(const QDBusAbstractAdaptor *a, const QString &packageId)
{
    if (!PackageManager::instance()->package(packageId))
        throw Exception("Unknown package id '%1'").arg(packageId);

    if (!isDevelopmentModeBus(a))
        return;

    if (PackageManager::instance()->developmentMode() == PackageManager::DevelopmentMode::Application) {
        const auto cert = PackageManager::instance()->developerCertificate();
        Q_ASSERT(cert.isValid());

        if (!cert.matchPackageId(packageId)) {
            throw Exception("Package id '%1' is not accessible using the currently set developer certificate")
                .arg(packageId);
        }
    }
}

QStringList filterPackageListByAccess(const QDBusAbstractAdaptor *a, const QStringList &inList,
                                      std::function<QString (const QString &)> mapToPackageId)
{
    if (isDevelopmentModeBus(a)
        && (PackageManager::instance()->developmentMode() != PackageManager::DevelopmentMode::System)) {
        const auto cert = PackageManager::instance()->developerCertificate();
        Q_ASSERT(cert.isValid());

        QStringList outList;
        for (const auto &item : inList) {
            const QString packageId = mapToPackageId(item);
            if (!packageId.isEmpty() && cert.matchPackageId(packageId))
                outList << item;
        }
        return outList;
    } else {
        return inList;
    }
}

QStringList filterTaskListByAccess(const QDBusAbstractAdaptor *a, const QStringList &taskList)
{
    if (isDevelopmentModeBus(a)
        && (PackageManager::instance()->developmentMode() != PackageManager::DevelopmentMode::System)) {
        QStringList filteredTaskList;
        for (const auto &taskId : taskList) {
            if (auto *task = PackageManager::instance()->taskFromId(taskId)) {
                if (task->origin() == AsynchronousTask::Origin::ApplicationDeveloper)
                    filteredTaskList << taskId;
            }
        }
        return filteredTaskList;
    } else {
        return taskList;
    }
}

bool isDevelopmentModeBus(const QDBusAbstractAdaptor *a)
{
    return a->property("developmentModeChecksEnabled").toBool();
}

} // namespace DBusAdaptorChecks
QT_END_NAMESPACE_AM
