/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
#include <QUrl>
#include <QtAppManApplication/applicationinfo.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;
class Application;

// A place to collect signals used internally by appman without polluting
// AbstractApplication's public QML API.
class ApplicationRequests : public QObject
{
    Q_OBJECT
signals:
    void startRequested(const QString &documentUrl);
    void debugRequested(const QString &debugWrapper, const QString &documentUrl);
    void stopRequested(bool forceKill);
};

class AbstractApplication : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationObject 2.0 UNCREATABLE")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString runtimeName READ runtimeName NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap runtimeParameters READ runtimeParameters NOTIFY bulkChange)
    Q_PROPERTY(QUrl icon READ icon NOTIFY bulkChange)
    Q_PROPERTY(QString documentUrl READ documentUrl NOTIFY bulkChange)
    Q_PROPERTY(bool builtIn READ isBuiltIn CONSTANT)
    Q_PROPERTY(bool alias READ isAlias CONSTANT)
    Q_PROPERTY(Application* nonAliased READ nonAliased CONSTANT)
    Q_PROPERTY(QStringList capabilities READ capabilities NOTIFY bulkChange)
    Q_PROPERTY(QStringList supportedMimeTypes READ supportedMimeTypes NOTIFY bulkChange)
    Q_PROPERTY(QStringList categories READ categories NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties NOTIFY bulkChange)
    Q_PROPERTY(AbstractRuntime *runtime READ currentRuntime NOTIFY runtimeChanged)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged)
    Q_PROPERTY(QT_PREPEND_NAMESPACE_AM(Am::ExitStatus) lastExitStatus READ lastExitStatus NOTIFY lastExitStatusChanged)
    Q_PROPERTY(QString version READ version NOTIFY bulkChange)
    Q_PROPERTY(bool supportsApplicationInterface READ supportsApplicationInterface NOTIFY bulkChange)
    Q_PROPERTY(QString codeDir READ codeDir NOTIFY bulkChange)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QT_PREPEND_NAMESPACE_AM(Am::RunState) runState READ runState NOTIFY runStateChanged)

public:
    enum State {
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingDowngraded,
        BeingRemoved
    };
    Q_ENUM(State)

    AbstractApplication(AbstractApplicationInfo *info);

    Q_INVOKABLE void start(const QString &documentUrl = QString());
    Q_INVOKABLE void debug(const QString &debugWrapper, const QString &documentUrl = QString());
    Q_INVOKABLE void stop(bool forceKill = false);

    virtual Application *nonAliased() = 0;

    /*
        Returns information about this application, which can either
        be a full-fledged one (ApplicationInfo) or just an alias
        (ApplicationAliasInfo).

        Use info()->isAlias() to check and cast as appropriate.
     */
    virtual AbstractApplicationInfo *info() const = 0;

    /*
        Always returns information about the "concrete", non-aliased, application.
        This is the same as static_cast<ApplicationInfo*>(nonAliased()->info())
     */
    virtual ApplicationInfo *nonAliasedInfo() const = 0;

    // Properties that mainly forward content from ApplicationInfo
    QString id() const;
    virtual QString runtimeName() const = 0;
    virtual QVariantMap runtimeParameters() const = 0;
    QUrl icon() const;
    QString documentUrl() const;
    bool isBuiltIn() const;
    bool isAlias() const;
    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QStringList categories() const;
    QVariantMap applicationProperties() const;
    bool supportsApplicationInterface() const;
    QString version() const;
    Q_INVOKABLE QString name(const QString &language) const;

    // Properties present only in Application (not coming from ApplicationInfo)
    virtual AbstractRuntime *currentRuntime() const = 0;
    virtual State state() const = 0;
    QString codeDir() const;
    virtual bool isBlocked() const = 0;
    virtual bool block() = 0;
    virtual bool unblock() = 0;
    virtual qreal progress() const = 0;
    virtual Am::RunState runState() const = 0;
    virtual int lastExitCode() const = 0;
    virtual Am::ExitStatus lastExitStatus() const = 0;

    ApplicationRequests requests;

    // Creates a list of Applications from a list of ApplicationInfo objects.
    // Ownership of the given ApplicationInfo objects is passed to the returned Applications.
    static QVector<AbstractApplication *> fromApplicationInfoVector(QVector<AbstractApplicationInfo *> &);

signals:
    void bulkChange();
    void runtimeChanged();
    void lastExitCodeChanged();
    void lastExitStatusChanged();
    void activated();
    void stateChanged(State state);
    void runStateChanged(Am::RunState state);

protected:
    QScopedPointer<AbstractApplicationInfo> m_info;
};

class Application : public AbstractApplication
{
    Q_OBJECT
public:
    Application(ApplicationInfo *info, State initialState = Installed);


    // Returns the updated info, if there's one. Otherwise
    // returns the base info.
    AbstractApplicationInfo *info() const override;

    // same as info()
    ApplicationInfo *nonAliasedInfo() const override;


    /*
        All applications have a base info.

        Built-in applications, when updated, also get an updated info.
        The updated info then overlays the base one. Subsequent updates
        just replace the updated info. When requested to be removed, a
        built-in application only loses its updated info, returning to
        expose the base one.

        Regular applications (ie, non-built-in) only have a base info. When
        updated their base info gets replaced and thus there's no way to go
        back to a previous version. Regular applications get completely
        removed when requested.
     */
    void setBaseInfo(ApplicationInfo*);
    void setUpdatedInfo(ApplicationInfo*);
    ApplicationInfo *updatedInfo() const { return m_updatedInfo.data(); }
    ApplicationInfo *takeBaseInfo();

    void setState(State);
    void setProgress(qreal);
    void setRunState(Am::RunState);
    void setCurrentRuntime(AbstractRuntime *rt);

    Application *nonAliased() override { return this; }
    QString runtimeName() const override;
    QVariantMap runtimeParameters() const override;
    AbstractRuntime *currentRuntime() const override { return m_runtime; }
    State state() const override { return m_state; }
    bool isBlocked() const override;
    bool block() override;
    bool unblock() override;
    qreal progress() const override { return m_progress; }
    Am::RunState runState() const override { return m_runState; }
    int lastExitCode() const override { return m_lastExitCode; }
    Am::ExitStatus lastExitStatus() const override { return m_lastExitStatus; }

private:
    void setLastExitCodeAndStatus(int exitCode, Am::ExitStatus exitStatus);

    AbstractRuntime *m_runtime = nullptr;
    QAtomicInt m_blocked;
    QAtomicInt m_mounted;

    State m_state = Installed;
    Am::RunState m_runState = Am::NotRunning;
    qreal m_progress = 0;

    int m_lastExitCode = 0;
    Am::ExitStatus m_lastExitStatus = Am::NormalExit;

    QScopedPointer<ApplicationInfo> m_updatedInfo;
};

class ApplicationAlias : public AbstractApplication
{
    Q_OBJECT
public:
    ApplicationAlias(Application*, ApplicationAliasInfo*);

    AbstractApplicationInfo *info() const override { return m_info.data(); }
    Application *nonAliased() override { return m_application; }
    QString runtimeName() const override;
    QVariantMap runtimeParameters() const override;
    AbstractRuntime *currentRuntime() const override { return m_application->currentRuntime(); }
    State state() const override { return m_application->state(); }
    bool isBlocked() const override { return m_application->isBlocked(); }
    bool block() override { return m_application->block(); }
    bool unblock() override { return m_application->unblock(); }
    qreal progress() const override { return m_application->progress(); }
    Am::RunState runState() const override { return m_application->runState(); }
    int lastExitCode() const override { return m_application->lastExitCode(); }
    Am::ExitStatus lastExitStatus() const override { return m_application->lastExitStatus(); }
protected:
    // Returns info() from the application being aliased
    ApplicationInfo *nonAliasedInfo() const override;
private:
    Application *m_application;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(const QT_PREPEND_NAMESPACE_AM(AbstractApplication *))

QDebug operator<<(QDebug debug, const QT_PREPEND_NAMESPACE_AM(AbstractApplication) *app);
