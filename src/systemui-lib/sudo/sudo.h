// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SUDO_H
#define SUDO_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <qplatformdefs.h>

#include <memory>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_BEGIN_NAMESPACE_AM

class SudoClientPrivate;

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

private:
    explicit SudoClient(SudoClientPrivate *dd);
    ~SudoClient() override;

    std::unique_ptr<SudoClientPrivate> d;

    friend class Sudo;
    static SudoClient *s_instance;
};

QT_END_NAMESPACE_AM

#endif // SUDO_H
