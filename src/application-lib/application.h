/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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
#include <QUrl>
#include <QMap>
#include <QVariant>
#include <QStringList>
#include <QDir>
#include <QObject>

#include <QtAppManCommon/global.h>
#include <QtAppManApplication/installationreport.h>

QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;
class ApplicationManager;
class JsonApplicationScanner;
class InstallationReport;

class Application : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/Application 1.0")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString runtimeName READ runtimeName NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap runtimeParameters READ runtimeParameters NOTIFY bulkChange)
    Q_PROPERTY(QUrl icon READ iconUrl NOTIFY bulkChange)
    Q_PROPERTY(QString documentUrl READ documentUrl NOTIFY bulkChange)
    Q_PROPERTY(qreal importance READ importance NOTIFY bulkChange)
    Q_PROPERTY(bool builtIn READ isBuiltIn CONSTANT)
    Q_PROPERTY(bool alias READ isAlias CONSTANT)
    Q_PROPERTY(bool preload READ isPreloaded NOTIFY bulkChange)
    Q_PROPERTY(const Application *nonAliased READ nonAliased CONSTANT)
    Q_PROPERTY(QStringList capabilities READ capabilities NOTIFY bulkChange)
    Q_PROPERTY(QStringList supportedMimeTypes READ supportedMimeTypes NOTIFY bulkChange)
    Q_PROPERTY(QStringList categories READ categories NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties NOTIFY bulkChange)
    Q_PROPERTY(AbstractRuntime *runtime READ currentRuntime NOTIFY runtimeChanged)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged)
    Q_PROPERTY(ExitStatus lastExitStatus READ lastExitStatus NOTIFY lastExitStatusChanged)
    Q_PROPERTY(QString version READ version NOTIFY bulkChange)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode NOTIFY bulkChange)
    Q_PROPERTY(bool supportsApplicationInterface READ supportsApplicationInterface NOTIFY bulkChange)
    Q_PROPERTY(QString codeDir READ codeDir NOTIFY bulkChange)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum ExitStatus { NormalExit, CrashExit, ForcedExit };
    Q_ENUM(ExitStatus)

    QString id() const;
    int uniqueNumber() const;
    QString absoluteCodeFilePath() const;
    QString codeFilePath() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QVariantMap environmentVariables() const;
    QMap<QString, QString> names() const;
    Q_INVOKABLE QString name(const QString &language) const;
    QString icon() const;
    QUrl iconUrl() const;
    QString documentUrl() const;

    bool supportsApplicationInterface() const;
    bool isPreloaded() const;
    qreal importance() const;
    bool isBuiltIn() const;
    bool isAlias() const;
    const Application *nonAliased() const;

    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QStringList categories() const;
    QVariantMap applicationProperties() const;
    QVariantMap allAppProperties() const;

    enum BackgroundMode
    {
        Auto,
        Never,
        ProvidesVoIP,
        PlaysAudio,
        TracksLocation
    };
    Q_ENUM(BackgroundMode)

    BackgroundMode backgroundMode() const;

    QString version() const;

    void validate() const Q_DECL_NOEXCEPT_EXPR(false);
    QVariantMap toVariantMap() const;
    static Application *fromVariantMap(const QVariantMap &map, QString *error = 0);
    void mergeInto(Application *app) const;

    const InstallationReport *installationReport() const;
    void setInstallationReport(InstallationReport *report);
    QString manifestDir() const;
    QString codeDir() const;
    uint uid() const;

    // dynamic part
    AbstractRuntime *currentRuntime() const;
    void setCurrentRuntime(AbstractRuntime *rt) const;
    bool isBlocked() const;
    bool block() const;
    bool unblock() const;

    enum State {
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingRemoved
    };
    State state() const;
    Q_ENUM(State)
    qreal progress() const;

    void setSupportsApplicationInterface(bool supportsAppInterface);
    void setCodeDir(const QString &path);
    void setManifestDir(const QString &path);
    void setBuiltIn(bool builtIn);

    int lastExitCode() const;
    ExitStatus lastExitStatus() const;

    static bool isValidApplicationId(const QString &appId, bool isAliasName = false, QString *errorString = nullptr);

signals:
    void bulkChange() const;
    void runtimeChanged() const;
    void lastExitCodeChanged() const;
    void lastExitStatusChanged() const;
    void activated() const;
    void stateChanged() const;

private:
    Application();

    // static part from info.json
    QString m_id;
    int m_uniqueNumber;

    QString m_codeFilePath; // relative to info.json location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    QVariantMap m_environmentVariables;
    QMap<QString, QString> m_name; // language -> name
    QString m_icon; // relative to info.json location
    QString m_documentUrl;
    QVariantMap m_allAppProperties;
    QVariantMap m_sysAppProperties;
    bool m_supportsApplicationInterface = false;

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
    QDir m_manifestDir;
    QDir m_codeDir;
    uint m_uid = uint(-1); // unix user id - move to installationReport

    // dynamic part
    mutable AbstractRuntime *m_runtime = 0;
    mutable QAtomicInt m_blocked;
    mutable QAtomicInt m_mounted;

    mutable State m_state = Installed;
    mutable qreal m_progress = 0;

    mutable int m_lastExitCode = 0;
    mutable ExitStatus m_lastExitStatus = NormalExit;

    friend class YamlApplicationScanner;
    friend class ApplicationManager; // needed to update installation status
    friend class ApplicationDatabase; // needed to create Application objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    static Application *readFromDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) Q_DECL_NOEXCEPT_EXPR(false);
    void writeToDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) const Q_DECL_NOEXCEPT_EXPR(false);

    Q_DISABLE_COPY(Application)
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(const QT_PREPEND_NAMESPACE_AM(Application *))

QDebug operator<<(QDebug debug, const QT_PREPEND_NAMESPACE_AM(Application) *app);
