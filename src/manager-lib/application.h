/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QString>
#include <QMap>
#include <QVariant>
#include <QStringList>
#include <QDir>

#include "global.h"
#include "installationreport.h"
#include "exception.h"

class AbstractRuntime;
class ApplicationManager;
class JsonApplicationScanner;
class InstallationReport;

class AM_EXPORT Application
{
public:
    enum Type { Gui, Headless };

    QString id() const;
    QString absoluteCodeFilePath() const;
    QString codeFilePath() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QMap<QString, QString> names() const;
    QString name(const QString &language) const;
    QString icon() const;
    QString documentUrl() const;

    bool isPreloaded() const;
    qreal importance() const;
    bool isBuiltIn() const;
    bool isAlias() const;
    const Application *nonAliased() const;

    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QStringList categories() const;
    Type type() const;

    enum BackgroundMode
    {
        Auto,
        Never,
        ProvidesVoIP,
        PlaysAudio,
        TracksLocation
    };
    BackgroundMode backgroundMode() const;

    QString version() const;

    void validate() const throw (Exception);
    QVariantMap toVariantMap() const;
    static Application *fromVariantMap(const QVariantMap &map, QString *error = 0);
    void mergeInto(Application *app) const;

    const InstallationReport *installationReport() const;
    void setInstallationReport(InstallationReport *report);
    QDir baseDir() const;
    uint uid() const;

    // dynamic part
    AbstractRuntime *currentRuntime() const;
    void setCurrentRuntime(AbstractRuntime *rt) const;
    bool isLocked() const;
    bool lock() const;
    bool unlock() const;

    enum State {
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingRemoved
    };
    State state() const;
    qreal progress() const;

    void setBaseDir(const QString &path); //TODO: replace baseDir handling with something that works :)

private:
    Application();

    // static part from info.json
    QString m_id;

    QString m_codeFilePath; // relative to info.json location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    QMap<QString, QString> m_name; // language -> name
    QString m_icon; // relative to info.json location
    QString m_documentUrl;

    bool m_preload = false;
    qreal m_importance = 0; // relative to all others, with 0 being "normal"
    bool m_builtIn = false; // system app - not removable
    const Application *m_nonAliased = nullptr; // builtin only - multiple icons for the same app

    QStringList m_capabilities;
    QStringList m_categories;
    QStringList m_mimeTypes;

    BackgroundMode m_backgroundMode = Auto;

    QString m_version;

    // added by installer
    QScopedPointer<InstallationReport> m_installationReport;
    QDir m_baseDir;
    uint m_uid = uint(-1); // unix user id - move to installationReport

    Type m_type = Gui;

    // dynamic part
    mutable AbstractRuntime *m_runtime = 0;
    mutable QAtomicInt m_locked;
    mutable QAtomicInt m_mounted;

    mutable State m_state = Installed;
    mutable qreal m_progress = 0;

    friend class YamlApplicationScanner;
    friend class ApplicationManager; // needed to update installation status
    friend class ApplicationDatabase; // needed to create Application objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    static Application *readFromDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) throw(Exception);
    void writeToDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) const throw(Exception);

    Q_DISABLE_COPY(Application)
};

QDebug operator<<(QDebug debug, const Application *app);
