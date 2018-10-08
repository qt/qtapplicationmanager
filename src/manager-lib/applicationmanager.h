/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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
#include <QJSValue>
#include <QtAppManCommon/global.h>
#include <QtAppManManager/application.h>

QT_FORWARD_DECLARE_CLASS(QDir)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerPrivate;
class AbstractRuntime;
class IpcProxyObject;

// A place to collect signals used internally by appman without polluting
// ApplicationManager's public QML API.
class ApplicationManagerInternalSignals : public QObject
{
    Q_OBJECT
signals:
    // Emitted after an application is installed, updated, downgraded or removed
    void applicationsChanged();
};

class ApplicationManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/ApplicationManager 1.0")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool singleProcess READ isSingleProcess CONSTANT)
    Q_PROPERTY(bool shuttingDown READ isShuttingDown NOTIFY shuttingDownChanged)
    Q_PROPERTY(bool securityChecksEnabled READ securityChecksEnabled)
    Q_PROPERTY(bool dummy READ isDummy CONSTANT)  // set to false here and true in the dummydata imports
    Q_PROPERTY(bool windowManagerCompositorReady READ isWindowManagerCompositorReady NOTIFY windowManagerCompositorReadyChanged)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT)
    Q_PROPERTY(QJSValue containerSelectionFunction READ containerSelectionFunction WRITE setContainerSelectionFunction NOTIFY containerSelectionFunctionChanged)

public:
    ~ApplicationManager() override;
    static ApplicationManager *createInstance(bool singleProcess);
    static ApplicationManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool isSingleProcess() const;
    bool isDummy() const { return false; }
    bool isShuttingDown() const;
    QVariantMap systemProperties() const;
    void setSystemProperties(const QVariantMap &map);

    // Set the initial application list
    // To be used only during startup (ie, before exposing ApplicationManager to QML) as
    // no model update signals are emitted.
    void setApplications(const QVector<AbstractApplication *> &apps);

    QVector<AbstractApplication *> applications() const;

    AbstractApplication *fromId(const QString &id) const;
    AbstractApplication *fromProcessId(qint64 pid) const;
    AbstractApplication *fromSecurityToken(const QByteArray &securityToken) const;
    QVector<AbstractApplication *> schemeHandlers(const QString &scheme) const;
    QVector<AbstractApplication *> mimeTypeHandlers(const QString &mimeType) const;

    bool startApplicationInternal(AbstractApplication *app, const QString &documentUrl = QString(),
                                  const QString &documentMimeType = QString(),
                                  const QString &debugWrapperSpecification = QString(),
                                  const QVector<int> &stdioRedirections = QVector<int>()) Q_DECL_NOEXCEPT_EXPR(false);
    void stopApplicationInternal(AbstractApplication *app, bool forceKill = false);

    // only use these two functions for development!
    bool securityChecksEnabled() const;
    void setSecurityChecksEnabled(bool enabled);

    // container selection
    void setContainerSelectionConfiguration(const QList<QPair<QString, QString> > &containerSelectionConfig);
    QJSValue containerSelectionFunction() const;
    void setContainerSelectionFunction(const QJSValue &callback);

    // window manager interface
    bool isWindowManagerCompositorReady() const;
    void setWindowManagerCompositorReady(bool ready);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE AbstractApplication *application(int index) const;
    Q_INVOKABLE AbstractApplication *application(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(const QString &id) const;

    Q_INVOKABLE void acknowledgeOpenUrlRequest(const QString &requestId, const QString &appId);
    Q_INVOKABLE void rejectOpenUrlRequest(const QString &requestId);

    // DBus interface
    Q_SCRIPTABLE QStringList applicationIds() const;
    Q_SCRIPTABLE QVariantMap get(const QString &id) const;
    Q_SCRIPTABLE bool startApplication(const QString &id, const QString &documentUrl = QString());
    Q_SCRIPTABLE bool debugApplication(const QString &id, const QString &debugWrapper, const QString &documentUrl = QString());
    Q_SCRIPTABLE void stopApplication(const QString &id, bool forceKill = false);
    Q_SCRIPTABLE void stopAllApplications(bool forceKill = false);
    Q_SCRIPTABLE bool openUrl(const QString &url);
    Q_SCRIPTABLE QStringList capabilities(const QString &id) const;
    Q_SCRIPTABLE QString identifyApplication(qint64 pid) const;
    Q_SCRIPTABLE QT_PREPEND_NAMESPACE_AM(Am::RunState) applicationRunState(const QString &id) const;

    ApplicationManagerInternalSignals internalSignals;

    void enableSingleAppMode();

public slots:
    void shutDown();

signals:
    Q_SCRIPTABLE void applicationRunStateChanged(const QString &id, QT_PREPEND_NAMESPACE_AM(Am::RunState) runState);
    Q_SCRIPTABLE void applicationWasActivated(const QString &id, const QString &aliasId);
    Q_SCRIPTABLE void countChanged();

    Q_SCRIPTABLE void applicationAdded(const QString &id);
    Q_SCRIPTABLE void applicationAboutToBeRemoved(const QString &id);
    Q_SCRIPTABLE void applicationChanged(const QString &id, const QStringList &changedRoles);

    void openUrlRequested(const QString &requestId, const QString &url, const QString &mimeType, const QStringList &possibleAppIds);

    void inProcessRuntimeCreated(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime); // evil hook to support in-process runtimes

    void memoryLowWarning();
    void memoryCriticalWarning();

    void containerSelectionFunctionChanged();
    void shuttingDownChanged();
    void shutDownFinished();
    void windowManagerCompositorReadyChanged(bool ready);

private slots:
    void startSingleAppAndQuitWhenStopped();
    void openUrlRelay(const QUrl &url);
    void addApplication(AbstractApplication *app);

    // Interface for the installer
    //TODO: Find something nicer than private slots with 3 friend classes.
    //      This is hard though, since the senders live in different threads and
    //      need to use BlockingQueuedConnections
    bool blockApplication(const QString &id);
    bool unblockApplication(const QString &id);
    bool startingApplicationInstallation(QT_PREPEND_NAMESPACE_AM(ApplicationInfo*) installApp);
    bool startingApplicationRemoval(const QString &id);
    void progressingApplicationInstall(const QString &id, qreal progress);
    bool finishedApplicationInstall(const QString &id);
    bool canceledApplicationInstall(const QString &id);

    friend class ApplicationInstaller;
    friend class InstallationTask;
    friend class DeinstallationTask;

private:
    void emitDataChanged(AbstractApplication *app, const QVector<int> &roles = QVector<int>());
    void emitActivated(AbstractApplication *app);
    void registerMimeTypes();

    ApplicationManager(bool singleProcess, QObject *parent = nullptr);
    ApplicationManager(const ApplicationManager &);
    ApplicationManager &operator=(const ApplicationManager &);
    static ApplicationManager *s_instance;

    ApplicationManagerPrivate *d;
};

QT_END_NAMESPACE_AM
