// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SUDO_H
#define SUDO_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <qplatformdefs.h>

#include <memory>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_BEGIN_NAMESPACE_AM

class SudoClient;
class SudoClientPrivate;
class TrustedSaveFilePrivate;

class Q_APPMANSYSTEMUI_EXPORT TrustedFile : public QFile
{
public:
    explicit TrustedFile(QObject *parent = nullptr);
};

class Q_APPMANSYSTEMUI_EXPORT TrustedSaveFile : public QFile
{
public:
    explicit TrustedSaveFile(QObject *parent = nullptr);
    ~TrustedSaveFile() override;

    void commit();
    void cancel();

private:
    std::unique_ptr<TrustedSaveFilePrivate> d;

    friend class SudoClient;
    Q_DISABLE_COPY_MOVE(TrustedSaveFile)
};

class Q_APPMANSYSTEMUI_EXPORT Sudo
{
public:
    enum DropPrivileges {
        DropPrivilegesPermanently,
        DropPrivilegesRegainable, // only use this for auto-tests
    };

    static void forkServer(DropPrivileges dropPrivileges) noexcept(false);
    static void startServer() noexcept(false);
    static void fallbackServer() noexcept(false);
};


class Q_APPMANSYSTEMUI_EXPORT SudoClient : public QObject
{
    Q_OBJECT
public:
    static SudoClient *instance();

    bool isFallbackImplementation() const;

    void removeRecursive(const QString &fileOrDir);
    void bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                             int namespacePidFd);
    void setExtendedAttribute(const QString &file, const QByteArray &attrName,
                              const QByteArray &attrValue);

    void setInstanceId(const QString &instanceId);

    std::unique_ptr<TrustedFile> openTrustedFile(QStandardPaths::StandardLocation location, const QString &relPath);
    std::unique_ptr<TrustedSaveFile> openTrustedSaveFile(QStandardPaths::StandardLocation location, const QString &relPath);
    void removeTrustedFile(QStandardPaths::StandardLocation location, const QString &relPath);

#if defined(QT_BUILD_INTERNAL)
    void setTestRootPathPrefix(const QString &prefix);
    void commitRawFdForTest(int fd);
    void resetInstanceIdForTest();
#endif

private:
    explicit SudoClient(SudoClientPrivate *dd);
    ~SudoClient() override;

    std::unique_ptr<SudoClientPrivate> d;

    friend class Sudo;
    friend class TrustedSaveFile;
    static SudoClient *s_instance;
};

QT_END_NAMESPACE_AM

#endif // SUDO_H
