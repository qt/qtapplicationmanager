// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManApplication/packageinfo.h>
#include <QUrl>
#include <QString>
#include <QAtomicInt>
#include <QObject>

QT_BEGIN_NAMESPACE_AM

class Application;

class Package : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/PackageObject 2.0 UNCREATABLE")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(bool builtIn READ isBuiltIn NOTIFY bulkChange)
    Q_PROPERTY(bool builtInHasRemovableUpdate READ builtInHasRemovableUpdate NOTIFY bulkChange)
    Q_PROPERTY(QUrl icon READ icon NOTIFY bulkChange)
    Q_PROPERTY(QString version READ version NOTIFY bulkChange)
    Q_PROPERTY(QString name READ name NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap names READ names NOTIFY bulkChange)
    Q_PROPERTY(QString description READ description NOTIFY bulkChange)
    Q_PROPERTY(QVariantMap descriptions READ descriptions NOTIFY bulkChange)
    Q_PROPERTY(QStringList categories READ categories NOTIFY bulkChange)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool blocked READ isBlocked NOTIFY blockedChanged)
    Q_PROPERTY(QList<QObject *> applications READ applications NOTIFY applicationsChanged)

public:
    enum State {
        Installed,
        BeingInstalled,
        BeingUpdated,
        BeingDowngraded,
        BeingRemoved
    };
    Q_ENUM(State)

    Package(PackageInfo *packageInfo, State initialState = Installed);

    QString id() const;
    bool isBuiltIn() const;
    bool builtInHasRemovableUpdate() const;
    QUrl icon() const;
    QString version() const;
    QString name() const;
    QVariantMap names() const;
    QString description() const;
    QVariantMap descriptions() const;
    QStringList categories() const;
    QList<QObject *> applications() const;

    State state() const { return m_state; }
    qreal progress() const { return m_progress; }

    void setState(State state);
    void setProgress(qreal progress);

    /*
        All packages have a base info.

        Built-in packages, when updated, also get an updated info.
        The updated info then overlays the base one. Subsequent updates
        just replace the updated info. When requested to be removed, a
        built-in packages only loses its updated info, returning to
        expose the base one.

        Regular packages (ie, non-built-in) only have a base info. When
        updated, their base info gets replaced and thus there's no way to go
        back to a previous version. Regular packages get completely
        removed when requested.
     */

    // Returns the updated info, if there's one. Otherwise returns the base info.
    PackageInfo *info() const;
    PackageInfo *baseInfo() const;
    PackageInfo *updatedInfo() const;
    PackageInfo *setUpdatedInfo(PackageInfo *info);
    PackageInfo *setBaseInfo(PackageInfo *info);

    bool isBlocked() const;
    bool block();
    bool unblock();

    // function for Application to report it has stopped after getting a block request
    void applicationStoppedDueToBlock(const QString &appId);
    // query function for the installer to verify that it is safe to manipulate binaries
    bool areAllApplicationsStoppedDueToBlock() const;

    // for the ApplicationManager to update the package -> application mapping
    void addApplication(Application *application);
    void removeApplication(Application *application);

signals:
    void bulkChange();
    void stateChanged(QT_PREPEND_NAMESPACE_AM(Package::State) state);
    void blockedChanged(bool blocked);
    void applicationsChanged();

private:
    PackageInfo *m_info = nullptr;
    PackageInfo *m_updatedInfo = nullptr;

    State m_state = Installed;
    qreal m_progress = 0;
    QAtomicInt m_blocked;
    QAtomicInt m_blockedAppsCount;
    QVector<ApplicationInfo *> m_blockedApps;
    QVector<Application *> m_applications;
};

QT_END_NAMESPACE_AM
