/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
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

class AbstractRuntime;
class ApplicationManager;
class JsonApplicationScanner;
class InstallationReport;

class AM_EXPORT Application
{
public:
    enum Type { Gui, Headless };

    QString id() const                           { return m_id; }
    QString absoluteCodeFilePath() const         { return m_codeFilePath.isEmpty() ? QString() : baseDir().absoluteFilePath(m_codeFilePath); }
    QString codeFilePath() const                 { return m_codeFilePath; }
    QString runtimeName() const                  { return m_runtimeName; }
    QVariantMap runtimeParameters() const        { return m_runtimeParameters; }
    QMap<QString, QString> displayNames() const  { return m_displayName; }
    QString displayName(const QString &language) const { return m_displayName.value(language); }
    QString displayIcon() const                  { return m_displayIcon.isEmpty() ? QString() : baseDir().absoluteFilePath(m_displayIcon); }

    bool isPreloaded() const                     { return m_preload; }
    qreal importance() const                     { return m_importance; }
    bool isBuiltIn() const                       { return m_builtIn; }

    QStringList capabilities() const             { return m_capabilities; }
    QStringList supportedMimeTypes() const       { return m_mimeTypes; }
    QStringList categories() const               { return m_categories; }
    Type type() const                            { return m_type; }

    enum BackgroundMode
    {
        Auto,
        Never,
        ProvidesVoIP,
        PlaysAudio,
        TracksLocation
    };
    BackgroundMode backgroundMode() const       { return m_backgroundMode; }

    QString version() const                     { return m_version; }

    bool validate(QString *error) const;
    QVariantMap toVariantMap() const;
    static Application *fromVariantMap(const QVariantMap &map, QString *error = 0);
    void mergeInto(Application *app) const;

    const InstallationReport *installationReport() const { return m_installationReport.data(); }
    void setInstallationReport(InstallationReport *report) { m_installationReport.reset(report); }
    QDir baseDir() const;
    uint uid() const                { return m_uid; }

    // dynamic part
    AbstractRuntime *currentRuntime() const { return m_runtime; }
    void setCurrentRuntime(AbstractRuntime *rt) const { m_runtime = rt; }
    bool isLocked() const          { return m_locked.load() == 1; }
    bool lock() const              { return m_locked.testAndSetOrdered(0, 1); }
    bool unlock() const            { return m_locked.testAndSetOrdered(1, 0); }

    enum State {
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingRemoved
    };
    State state() const             { return m_state; }
    qreal progress() const          { return m_progress; }

    void setBaseDir(const QString &path); //TODO: replace baseDir handling with something that works :)

private:
    Application();

    // static part from info.json
    QString m_id;

    QString m_codeFilePath; // relative to info.json location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    QMap<QString, QString> m_displayName; // language -> name
    QString m_displayIcon; // relative to info.json location


    bool m_preload = false;
    qreal m_importance = 0; // relative to all others, with 0 being "normal"
    bool m_builtIn = false; // system app - not removable

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

    friend QDataStream &operator<<(QDataStream &ds, const Application &app);
    friend QDataStream &operator>>(QDataStream &ds, Application &app);
    friend class ApplicationManager; // needed to update installation status
    friend class ApplicationDatabase; // needed to create Application objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    Q_DISABLE_COPY(Application)
};

QDataStream &operator<<(QDataStream &ds, const Application &app);
QDataStream &operator>>(QDataStream &ds, Application &app);

QDebug operator<<(QDebug debug, const Application *app);
