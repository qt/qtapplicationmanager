/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QString>
#include <QDir>
#include <QFile>

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

QT_END_NAMESPACE_AM

Q_DECLARE_OPERATORS_FOR_FLAGS(QT_PREPEND_NAMESPACE_AM(ScopedRenamer::Modes))
