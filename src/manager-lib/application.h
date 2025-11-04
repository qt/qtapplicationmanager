// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QUrl>
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
    Q_PROPERTY(QString id READ id CONSTANT FINAL)
    Q_PROPERTY(QString name READ name NOTIFY bulkChange FINAL)
    Q_PROPERTY(QVariantMap names READ names NOTIFY bulkChange FINAL)
    Q_PROPERTY(QString description READ description NOTIFY bulkChange FINAL)
    Q_PROPERTY(QVariantMap descriptions READ descriptions NOTIFY bulkChange FINAL)
    Q_PROPERTY(QStringList categories READ categories NOTIFY bulkChange FINAL)
    Q_PROPERTY(QUrl icon READ icon NOTIFY bulkChange FINAL)
    Q_PROPERTY(QString runtimeName READ runtimeName CONSTANT FINAL)
    Q_PROPERTY(QVariantMap runtimeParameters READ runtimeParameters CONSTANT FINAL)
    Q_PROPERTY(QStringList capabilities READ capabilities CONSTANT FINAL)
    Q_PROPERTY(QString documentUrl READ documentUrl CONSTANT FINAL)
    Q_PROPERTY(QStringList supportedMimeTypes READ supportedMimeTypes CONSTANT FINAL)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT FINAL)
    Q_PROPERTY(QtAM::AbstractRuntime *runtime READ currentRuntime NOTIFY runtimeChanged FINAL)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged FINAL)
    Q_PROPERTY(QtAM::Am::ExitStatus lastExitStatus READ lastExitStatus NOTIFY lastExitStatusChanged FINAL)
    Q_PROPERTY(QString codeDir READ codeDir NOTIFY bulkChange FINAL)
    Q_PROPERTY(QtAM::Am::RunState runState READ runState NOTIFY runStateChanged FINAL)
    Q_PROPERTY(QtAM::Package *package READ package CONSTANT FINAL)

    // legacy, forwarded to Package
    Q_PROPERTY(bool builtIn READ isBuiltIn CONSTANT FINAL)
    Q_PROPERTY(bool alias READ isAlias CONSTANT FINAL)
    Q_PROPERTY(QtAM::Application *nonAliased READ nonAliased CONSTANT FINAL)
    Q_PROPERTY(QString version READ version CONSTANT FINAL)
    Q_PROPERTY(QtAM::Application::State state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool blocked READ isBlocked NOTIFY blockedChanged FINAL)

    // internal
    Q_PROPERTY(bool supportsApplicationInterface READ supportsApplicationInterface CONSTANT FINAL)

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
    QString documentUrl() const;
    QStringList supportedMimeTypes() const;
    bool isBlocked() const;

    // Properties that mainly forward content from ApplicationInfo
    QString id() const;
    QUrl icon() const;
    QStringList categories() const;
    QString name() const;
    QVariantMap names() const;
    QString description() const;
    QVariantMap descriptions() const;
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

Q_SIGNALS:
    void bulkChange();
    void runtimeChanged();
    void lastExitCodeChanged();
    void lastExitStatusChanged();
    void activated();
    void stateChanged(QtAM::Application::State state);
    void runStateChanged(QtAM::Am::RunState state);
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

QDebug operator<<(QDebug debug, const Application *app);

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(const QtAM::Application *)

#endif // APPLICATION_H
