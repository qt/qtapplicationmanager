/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>
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
    static SudoClient *createInstance(int socketFd, SudoServer *shortCircuit = 0);

    static SudoClient *instance();

    bool isFallbackImplementation() const;

    bool removeRecursive(const QString &fileOrDir) override;
    bool setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions) override;

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
