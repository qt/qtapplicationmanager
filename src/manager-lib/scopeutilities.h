// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class ScopedDirectoryCreator
{
public:
    ScopedDirectoryCreator();
    bool create(const QString &path, bool removeExisting = true);
    bool take();
    bool destroy();
    ~ScopedDirectoryCreator();

    QDir dir();

private:
    Q_DISABLE_COPY(ScopedDirectoryCreator)

    QString m_path;
    bool m_created = false;
    bool m_taken = false;
};

class ScopedRenamer
{
public:
    enum Mode {
        NameToNameMinus = 1,  // create backup   : foo -> foo-
        NamePlusToName  = 2   // replace with new: foo+ -> foo
    };

    Q_DECLARE_FLAGS(Modes, Mode)

    ScopedRenamer();
    bool rename(const QString &baseName, ScopedRenamer::Modes modes);
    bool rename(const QDir &baseName, ScopedRenamer::Modes modes);
    bool take();
    bool undoRename();
    ~ScopedRenamer();

    bool isRenamed() const;
    QString baseName() const;

private:
    bool interalUndoRename();
    static bool internalRename(const QDir &dir, const QString &from, const QString &to);
    Q_DISABLE_COPY(ScopedRenamer)
    QDir m_basePath;
    QString m_name;
    Modes m_requested;
    Modes m_done;
    bool m_taken = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ScopedRenamer::Modes)

QT_END_NAMESPACE_AM

