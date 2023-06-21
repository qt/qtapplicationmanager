// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <qplatformdefs.h>

#ifdef Q_OS_UNIX
#  include <sys/types.h>
#else
typedef uint uid_t;
typedef uint gid_t;
//typedef uint mode_t; // already typedef'ed in qplatformdefs.h
#endif

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Sudo
{
public:
    enum DropPrivileges {
        DropPrivilegesPermanently,
        DropPrivilegesRegainable, // only use this for auto-tests
    };

    Q_DECL_DEPRECATED static void forkServer(DropPrivileges dropPrivileges, QStringList *warnings)
                                                                       Q_DECL_NOEXCEPT_EXPR(false);
    static void forkServer(DropPrivileges dropPrivileges) Q_DECL_NOEXCEPT_EXPR(false);
};

class SudoInterface
{
public:
    virtual ~SudoInterface() = default;

    virtual bool removeRecursive(const QString &fileOrDir) = 0;
    virtual bool setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions) = 0;
    virtual bool bindMountFileSystem(const QString &from, const QString &to, bool readOnly, quint64 namespacePid = 0) = 0;

protected:
    enum MessageType { Request, Reply };

#ifdef Q_OS_LINUX
    QByteArray receiveMessage(int socket, MessageType type, QString *errorString);
    bool sendMessage(int socket, const QByteArray &msg, MessageType type, const QString &errorString = QString());
#endif
    QByteArray receive(const QByteArray &packet);

protected:
    SudoInterface();
private:
    Q_DISABLE_COPY(SudoInterface)
};

class SudoServer;

class SudoClient : public SudoInterface
{
public:
    static SudoClient *createInstance(int socketFd, SudoServer *shortCircuit = nullptr);

    static SudoClient *instance();

    bool isFallbackImplementation() const;

    bool removeRecursive(const QString &fileOrDir) override;
    bool setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions) override;
    bool bindMountFileSystem(const QString &from, const QString &to, bool readOnly, quint64 namespacePid) override;

    void stopServer();

    QString lastError() const { return m_errorString; }

private:
    SudoClient(int socketFd);

    QByteArray call(const QByteArray &msg);

    int m_socket;
    QString m_errorString;
    QMutex m_mutex;
    SudoServer *m_shortCircuit;

    static SudoClient *s_instance;
};

class SudoServer : public SudoInterface
{
public:
    static SudoServer *createInstance(int socketFd);

    static SudoServer *instance();

    bool removeRecursive(const QString &fileOrDir) override;
    bool setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions) override;
    bool bindMountFileSystem(const QString &from, const QString &to, bool readOnly, quint64 namespacePid) override;

    QString lastError() const { return m_errorString; }

    Q_NORETURN void run();

private:
    SudoServer(int socketFd);

    QByteArray receive(const QByteArray &msg);
    friend class SudoClient;

    int m_socket;
    QString m_errorString;
    bool m_stop = false;

    static SudoServer *s_instance;
};

QT_END_NAMESPACE_AM
