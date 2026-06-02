// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SUDO_H
#define SUDO_H

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <qplatformdefs.h>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_BEGIN_NAMESPACE_AM

class SocketIpc;
class SudoServer;

class Q_APPMANSYSTEMUI_EXPORT Sudo
{
public:
    enum DropPrivileges {
        DropPrivilegesPermanently,
        DropPrivilegesRegainable, // only use this for auto-tests
    };

    // Must be called before QCoreApplication is constructed.
    static void forkServer(DropPrivileges dropPrivileges) noexcept(false);
    // Must be called after QCoreApplication is constructed.
    static void startServer() noexcept(false);
    static void fallbackServer() noexcept(false);
};


class Q_APPMANSYSTEMUI_EXPORT SudoInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("SocketIpcClassName", "SudoInterface")

public:
    Q_INVOKABLE virtual void removeRecursive(const QString &fileOrDir) = 0;
    Q_INVOKABLE virtual void bindMountFileSystem(const QString &from, const QString &to,
                                                 bool readOnly, quint64 namespacePid,
                                                 quint64 namespacePidInode) = 0;
    Q_INVOKABLE virtual void setExtendedAttribute(const QString &file, const QByteArray &attrName,
                                                  const QByteArray &attrValue) = 0;

protected:
    explicit SudoInterface(QObject *parent = nullptr);

private:
    Q_DISABLE_COPY_MOVE(SudoInterface)
};


class Q_APPMANSYSTEMUI_EXPORT SudoClient : public SudoInterface
{
    Q_OBJECT
public:
    // Constructed by SocketIpc::bindSingleton<SudoClient>() - passes itself as the only arg.
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    explicit SudoClient(SocketIpc *ipc);
#endif
    static SudoClient *instance();

    bool isFallbackImplementation() const;

    void removeRecursive(const QString &fileOrDir) override;
    void bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                             quint64 namespacePid, quint64 namespacePidInode) override;
    void setExtendedAttribute(const QString &file, const QByteArray &attrName,
                              const QByteArray &attrValue) override;

private:
    // Constructed by Sudo::fallbackServer() - bypasses IPC and calls a local SudoServer directly.
    explicit SudoClient(SudoServer *fallback);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QPointer<SocketIpc> m_ipc;
#endif
    QPointer<SudoServer> m_fallback;

    friend class Sudo;
    static SudoClient *s_instance;
};


class Q_APPMANSYSTEMUI_EXPORT SudoServer : public SudoInterface
{
    Q_OBJECT
public:
    explicit SudoServer(QObject *parent = nullptr);

    void removeRecursive(const QString &fileOrDir) override;
    void bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                             quint64 namespacePid, quint64 namespacePidInode) override;
    void setExtendedAttribute(const QString &file, const QByteArray &attrName,
                              const QByteArray &attrValue) override;
};

QT_END_NAMESPACE_AM

#endif // SUDO_H
