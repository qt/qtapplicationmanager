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
    std::optional<QString> instanceId;  // set via setInstanceId(), forwarded to the helper
    std::optional<QString> testPrefix;

    void commitTrusted(int writtenFd);
    void cancelTrusted(int stagingFd) noexcept;
};

class TrustedSaveFilePrivate
{
public:
    QPointer<SudoClient> client;
    QString relPath;
    bool committed = false;
    bool cancelled = false;

    // Fallback (non-root) mode: the QFile is opened on a local temp file and commit() renames it
    // onto fallbackFinalPath.
    bool isFallback = false;
    QString fallbackFinalPath;
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

    void setInstanceId(const QString &instanceId);
    void setTestRootPathPrefix(const QString &prefix);

    QDBusUnixFileDescriptor openTrustedFile(int location, const QString &relPath);
    QDBusUnixFileDescriptor openTrustedSaveFile(int location, const QString &relPath);
    void commitTrustedSaveFile(const QDBusUnixFileDescriptor &saveFd);
    void cancelTrustedSaveFile(const QDBusUnixFileDescriptor &saveFd);
    void removeTrustedFile(int location, const QString &relPath);

private:
    // Helper keeps the O_TMPFILE open: pins the inode key, and lets commit linkat our own fd.
    struct SaveSession
    {
        Unix::Fd fd;
        QString absPath;
        std::chrono::steady_clock::time_point issuedAt;
    };

    // Keyed by the staging inode; the client passes the fd back on commit/cancel.
    std::map<std::pair<quint64, quint64>, SaveSession> m_saveSessions;
    static constexpr size_t MaxSaveSessions = 128;
    static constexpr auto MaxSaveSessionDuration = std::chrono::minutes(5);

    std::optional<QString> m_instanceId;
    std::optional<QString> m_testPrefix;

    static std::pair<quint64, quint64> saveSessionKey(int fd);
};

#endif // QT_CONFIG(am_multi_process)

QT_END_NAMESPACE_AM

// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // SUDO_P_H
