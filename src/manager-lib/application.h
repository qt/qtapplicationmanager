/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QObject>
#include <QScopedPointer>
#include <QVariantMap>
#include <QUrl>
#include <QtAppManApplication/applicationinfo.h>
#include <QtAppManApplication/packageinfo.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;

// A place to collect functions used internally by appman without polluting
// Application's public QML API.
class ApplicationRequests
{
public:
    std::function<bool(const QString &documentUrl)> startRequested;
    std::function<bool(const QString &debugWrapper, const QString &documentUrl)> debugRequested;
    std::function<void(bool forceKill)> stopRequested;
};

class Package;

class Application : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationObject 2.0 UNCREATABLE")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString runtimeName READ runtimeName CONSTANT)
    Q_PROPERTY(QVariantMap runtimeParameters READ runtimeParameters CONSTANT)
    Q_PROPERTY(QStringList capabilities READ capabilities CONSTANT)
    Q_PROPERTY(QString documentUrl READ documentUrl CONSTANT)
    Q_PROPERTY(QStringList supportedMimeTypes READ supportedMimeTypes CONSTANT)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT)
    Q_PROPERTY(AbstractRuntime *runtime READ currentRuntime NOTIFY runtimeChanged)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged)
    Q_PROPERTY(QT_PREPEND_NAMESPACE_AM(Am::ExitStatus) lastExitStatus READ lastExitStatus NOTIFY lastExitStatusChanged)
    Q_PROPERTY(QString codeDir READ codeDir NOTIFY bulkChange)
    Q_PROPERTY(QT_PREPEND_NAMESPACE_AM(Am::RunState) runState READ runState NOTIFY runStateChanged)
    Q_PROPERTY(Package *package READ package CONSTANT)

    // legacy, forwarded to Package
    Q_PROPERTY(QUrl icon READ icon NOTIFY bulkChange)
    Q_PROPERTY(bool builtIn READ isBuiltIn CONSTANT)
    Q_PROPERTY(bool alias READ isAlias CONSTANT)
    Q_PROPERTY(Application *nonAliased READ nonAliased CONSTANT)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool blocked READ isBlocked NOTIFY blockedChanged)

    // internal
    Q_PROPERTY(bool supportsApplicationInterface READ supportsApplicationInterface CONSTANT)

public:
    enum State { // kept for compatibility ... in reality moved to class Package
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingDowngraded,
        BeingRemoved
    };
    Q_ENUM(State)

    Application(ApplicationInfo *info, Package *package);

    Q_INVOKABLE bool start(const QString &documentUrl = QString());
    Q_INVOKABLE bool debug(const QString &debugWrapper, const QString &documentUrl = QString());
    Q_INVOKABLE void stop(bool forceKill = false);

    ApplicationInfo *info() const;
    PackageInfo *packageInfo() const;
    Package *package() const;

    // legacy compatibility
    bool isAlias() const { return false; }
    Application *nonAliased() { return this; }
    QStringList categories() const;
    QUrl icon() const;
    QString documentUrl() const;
    QStringList supportedMimeTypes() const;
    QString name() const;
    Q_INVOKABLE QString name(const QString &language) const;
    bool isBlocked() const;

    // Properties that mainly forward content from ApplicationInfo
    QString id() const;
    bool isBuiltIn() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QStringList capabilities() const;
    QVariantMap applicationProperties() const;
    bool supportsApplicationInterface() const;
    QString codeDir() const;
    QString version() const;

    // Properties present only in Application (not coming from ApplicationInfo)

    AbstractRuntime *currentRuntime() const { return m_runtime; }
    State state() const;
    qreal progress() const;
    Am::RunState runState() const { return m_runState; }
    int lastExitCode() const { return m_lastExitCode; }
    Am::ExitStatus lastExitStatus() const { return m_lastExitStatus; }

    ApplicationRequests requests;

    void setRunState(Am::RunState runState);
    void setCurrentRuntime(AbstractRuntime *runtime);

signals:
    void bulkChange();
    void runtimeChanged();
    void lastExitCodeChanged();
    void lastExitStatusChanged();
    void activated();
    void stateChanged(State state);
    void runStateChanged(QT_PREPEND_NAMESPACE_AM(Am::RunState) state);
    void blockedChanged(bool blocked);

private:
    void setLastExitCodeAndStatus(int exitCode, Am::ExitStatus exitStatus);

    ApplicationInfo *m_info = nullptr;
    Package *m_package = nullptr;
    AbstractRuntime *m_runtime = nullptr;

    Am::RunState m_runState = Am::NotRunning;

    int m_lastExitCode = 0;
    Am::ExitStatus m_lastExitStatus = Am::NormalExit;

};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(const QT_PREPEND_NAMESPACE_AM(Application *))

QDebug operator<<(QDebug debug, const QT_PREPEND_NAMESPACE_AM(Application) *app);
