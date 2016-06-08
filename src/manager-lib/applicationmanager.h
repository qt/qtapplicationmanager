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

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>
#include <QProcess>
#if defined(QT_DBUS_LIB)
#  include <QDBusContext>
#  include <QDBusConnectionInterface>
#endif

QT_FORWARD_DECLARE_CLASS(QDir)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

class Application;
class ApplicationDatabase;
class ApplicationManagerPrivate;
class AbstractRuntime;
class IpcProxyObject;


class ApplicationManager : public QAbstractListModel
#if defined(QT_DBUS_LIB)
        , protected QDBusContext
#endif
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool securityChecksEnabled READ securityChecksEnabled)
    Q_PROPERTY(bool dummy READ isDummy CONSTANT)  // set to false here and true in the dummydata imports
    Q_PROPERTY(QVariantMap additionalConfiguration READ additionalConfiguration CONSTANT)
    Q_ENUMS(RunState)

public:
    enum RunState {
        NotRunning,
        StartingUp,
        Running,
        ShutingDown
    };

    ~ApplicationManager();
    static ApplicationManager *createInstance(ApplicationDatabase *adb, QString *error);
    static ApplicationManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool isDummy() const { return false; }
    QVariantMap additionalConfiguration() const;
    void setAdditionalConfiguration(const QVariantMap &map);

    QVector<const Application *> applications() const;

    const Application *fromId(const QString &id) const;
    const Application *fromProcessId(qint64 pid) const;
    const Application *fromSecurityToken(const QByteArray &securityToken) const;
    const Application *schemeHandler(const QString &scheme) const;
    const Application *mimeTypeHandler(const QString &mimeType) const;

    bool startApplication(const Application *app, const QString &documentUrl = QString());
    void stopApplication(const Application *app, bool forceKill = false);

    // only use these two functions for development!
    bool securityChecksEnabled() const;
    void setSecurityChecksEnabled(bool enabled);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE const Application *application(int index) const;
    Q_INVOKABLE const Application *application(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(const QString &id) const;

    bool setDBusPolicy(const QVariantMap &yamlFragment);

    // DBus interface
    Q_SCRIPTABLE QStringList applicationIds() const;
    Q_SCRIPTABLE QVariantMap get(const QString &id) const;
    Q_SCRIPTABLE bool startApplication(const QString &id, const QString &documentUrl = QString());
    Q_SCRIPTABLE void stopApplication(const QString &id, bool forceKill = false);
    Q_SCRIPTABLE bool openUrl(const QString &url);
    Q_SCRIPTABLE QStringList capabilities(const QString &id) const;
    Q_SCRIPTABLE QString identifyApplication(qint64 pid) const;
    Q_SCRIPTABLE RunState applicationRunState(const QString &id) const;

signals:
    Q_SCRIPTABLE void applicationRunStateChanged(const QString &id, RunState runstate);
    Q_SCRIPTABLE void applicationWasActivated(const QString &id, const QString &aliasId);
    Q_SCRIPTABLE void countChanged();

    void inProcessRuntimeCreated(AbstractRuntime *runtime); // evil hook to support in-process runtimes

    void memoryLowWarning();

private slots:
    void preload();
    void openUrlRelay(const QUrl &url);

    // Interface for the installer
    //TODO: Find something nicer than private slots with 3 friend classes.
    //      This is hard though, since the senders live in different threads and
    //      need to use BlockingQueuedConnections
    bool lockApplication(const QString &id);
    bool unlockApplication(const QString &id);
    bool startingApplicationInstallation(Application *installApp);
    bool startingApplicationRemoval(const QString &id);
    void progressingApplicationInstall(const QString &id, qreal progress);
    bool finishedApplicationInstall(const QString &id);
    bool canceledApplicationInstall(const QString &id);

    friend class ApplicationInstaller;
    friend class InstallationTask;
    friend class DeinstallationTask;

private:
    void emitDataChanged(const Application *app);
    void registerMimeTypes();

    ApplicationManager(ApplicationDatabase *adb, QObject *parent);
    ApplicationManager(const ApplicationManager &);
    ApplicationManager &operator=(const ApplicationManager &);
    static ApplicationManager *s_instance;

    ApplicationManagerPrivate *d;
};
