// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef ADAPTORCHECKS_DBUS_H
#define ADAPTORCHECKS_DBUS_H

#include <QtDBus/QDBusAbstractAdaptor>
#include <QtAppManManager/packagemanager.h>
#include <QtAppManMain/qtappmanmainglobal.h>
#include <QtAppManCommon/logging.h>


// The DBus adaptor headers are auto-generated, so we cannot use a common base class or additional
// member functions. Instead we use free functions and macros to implement the checks we need.
// The macros are not strictly necessary, but they make the actual checks in the adaptors more
// concise and easier to read.

QT_BEGIN_NAMESPACE_AM
namespace DBusAdaptorChecks {

// public API

#define checkDBusAccess()                     checkDBusAccessPrivate(this, __FUNCTION__)
#define checkDBusAccessNoCertificateNeeded()  checkDBusAccessPrivate(this, __FUNCTION__, false)
#define checkInstaller()                      checkInstallerPrivate(this)
#define checkDevelopmentModeSystem()          checkDevelopmentModeSystemPrivate(this)
#define checkPackageAccess(packageId)         checkPackageAccessPrivate(this, packageId)
#define checkApplicationAccess(applicationId) checkApplicationAccessPrivate(this, applicationId)
#define checkTaskAccess(taskId)               checkTaskAccessPrivate(this, taskId)

#define catchExceptionAsDBusError(...) catch(const Exception &e) { \
    DBusContextAdaptor::sendErrorReply(this, e.errorString()); \
    qCWarning(LogDBus).noquote() << "DBus call was denied:" << e.errorString(); \
    return __VA_ARGS__; \
}
#define catchExceptionAndIgnore(...)   catch(const Exception &) { return __VA_ARGS__; }

bool isDevelopmentModeBus(const QDBusAbstractAdaptor *a);

QStringList filterPackageListByAccess(const QDBusAbstractAdaptor *a, const QStringList &inList,
                                      std::function<QString(const QString &)> mapToPackageId = [](const QString &x) { return x; });

QStringList filterTaskListByAccess(const QDBusAbstractAdaptor *a, const QStringList &taskList);

// private API

void checkDBusAccessPrivate(const QDBusAbstractAdaptor *a, const char *function,
                            bool certificateNeeded = true);
void checkInstallerPrivate(const QDBusAbstractAdaptor *a);
void checkDevelopmentModeSystemPrivate(const QDBusAbstractAdaptor *a);
void checkPackageAccessPrivate(const QDBusAbstractAdaptor *a, const QString &packageId);
void checkApplicationAccessPrivate(const QDBusAbstractAdaptor *a, const QString &applicationId);
void checkTaskAccessPrivate(const QDBusAbstractAdaptor *a, const QString &taskId);

} // namespace DBusAdaptorChecks
QT_END_NAMESPACE_AM

#endif // ADAPTORCHECKS_DBUS_H
