// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#ifndef SUDO_P_H
#define SUDO_P_H

#include "sudo.h"
#include "unix-utilities.h"

#include <QtCore/QPointer>
#include <QtCore/QString>
#if QT_CONFIG(am_multi_process)
#  include <QtDBus/QDBusContext>
#  include <QtDBus/QDBusUnixFileDescriptor>
#endif
#include <optional>

// generated D-Bus client proxy (global namespace), see io.qt.applicationmanager.sudo.xml
class IoQtApplicationManagerSudoInterface;

QT_BEGIN_NAMESPACE_AM

class SudoClientPrivate
{
public:
    bool isFallback = false;
#if QT_CONFIG(am_multi_process)
    QPointer<IoQtApplicationManagerSudoInterface> iface;
#endif
};

#if QT_CONFIG(am_multi_process)

class SudoServer : public QObject, public QDBusContext
{
    Q_OBJECT
public:
    SudoServer(QObject *parent = nullptr);

public Q_SLOTS:
    void removeRecursive(const QString &fileOrDir);
    void bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                             bool useNamespacePidFd, const QDBusUnixFileDescriptor &namespacePidFd);
    void setExtendedAttribute(const QString &file, const QByteArray &attrName,
                              const QByteArray &attrValue);
};

#endif // QT_CONFIG(am_multi_process)

QT_END_NAMESPACE_AM

// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // SUDO_P_H
