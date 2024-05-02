// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtQml/QJSValue>
#include <QtAppManCommon/global.h>
#include <QtAppManManager/application.h>


QT_FORWARD_DECLARE_CLASS(QDir)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerPrivate;
class AbstractRuntime;

// A place to collect signals used internally by appman without polluting
// ApplicationManager's public QML API.
class ApplicationManagerInternalSignals : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    // Emitted every time a new Runtime object is created
    void newRuntimeCreated(QtAM::AbstractRuntime *runtime);
    void shutDownFinished();
};

class ApplicationManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool singleProcess READ isSingleProcess CONSTANT FINAL)
    Q_PROPERTY(bool shuttingDown READ isShuttingDown NOTIFY shuttingDownChanged SCRIPTABLE false FINAL)
    Q_PROPERTY(bool securityChecksEnabled READ securityChecksEnabled CONSTANT FINAL)
    Q_PROPERTY(bool dummy READ isDummy CONSTANT FINAL)  // set to false here and true in the dummydata imports
    Q_PROPERTY(bool windowManagerCompositorReady READ isWindowManagerCompositorReady NOTIFY windowManagerCompositorReadyChanged FINAL)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT FINAL)
    Q_PROPERTY(QJSValue containerSelectionFunction READ containerSelectionFunction WRITE setContainerSelectionFunction NOTIFY containerSelectionFunctionChanged FINAL)
    Q_PROPERTY(QStringList availableRuntimeIds READ availableRuntimeIds CONSTANT FINAL REVISION(2, 2))
    Q_PROPERTY(QStringList availableContainerIds READ availableContainerIds CONSTANT FINAL REVISION(2, 2))

public:
    ~ApplicationManager() override;
    static ApplicationManager *createInstance(bool singleProcess);
    static ApplicationManager *instance();

    bool isSingleProcess() const;
    bool isDummy() const { return false; }
    bool isShuttingDown() const;
    QVariantMap systemProperties() const;
    void setSystemProperties(const QVariantMap &map);
    QStringList availableRuntimeIds() const;
    QStringList availableContainerIds() const;

    void addApplication(ApplicationInfo *appInfo, Package *package);
    void removeApplication(ApplicationInfo *appInfo, Package *package);
    QVector<Application *> applications() const;

    Application *fromId(const QString &id) const;
    QVector<Application *> fromProcessId(qint64 pid) const;
    Application *fromSecurityToken(const QByteArray &securityToken) const;
    QVector<Application *> schemeHandlers(const QString &scheme) const;
    QVector<Application *> mimeTypeHandlers(const QString &mimeType) const;
    QVariantMap get(Application *app) const;

    bool startApplicationInternal(const QString &appId, const QString &documentUrl = QString(),
                                  const QString &documentMimeType = QString(),
                                  const QString &debugWrapperSpecification = QString(),
                                  QVector<int> &&stdioRedirections = QVector<int>()) noexcept(false);
    void stopApplicationInternal(Application *app, bool forceKill = false);

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
    QVariant dataForRole(Application *app, int role) const;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QtAM::Application *application(int index) const;
    Q_INVOKABLE QtAM::Application *application(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(QtAM::Application *application) const;

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
    Q_SCRIPTABLE QStringList identifyAllApplications(qint64 pid) const;
    Q_SCRIPTABLE QtAM::Am::RunState applicationRunState(const QString &id) const;

    ApplicationManagerInternalSignals internalSignals;

public Q_SLOTS:
    void shutDown();

Q_SIGNALS:
    Q_SCRIPTABLE void applicationRunStateChanged(const QString &id, QtAM::Am::RunState runState);
    Q_SCRIPTABLE void applicationWasActivated(const QString &id, const QString &aliasId);
    Q_SCRIPTABLE void countChanged();

    Q_SCRIPTABLE void applicationAdded(const QString &id);
    Q_SCRIPTABLE void applicationAboutToBeRemoved(const QString &id);
    Q_SCRIPTABLE void applicationChanged(const QString &id, const QStringList &changedRoles);

    Q_SCRIPTABLE void windowManagerCompositorReadyChanged(bool ready);

    void openUrlRequested(const QString &requestId, const QString &url, const QString &mimeType, const QStringList &possibleAppIds);

    void memoryLowWarning();
    void memoryCriticalWarning();

    void containerSelectionFunctionChanged();
    void shuttingDownChanged();

private Q_SLOTS:
    void openUrlRelay(const QUrl &url);

private:
    void emitDataChanged(Application *app, const QVector<int> &roles = QVector<int>());
    void emitActivated(Application *app);
    void registerMimeTypes();

    ApplicationManager(bool singleProcess, QObject *parent = nullptr);
    ApplicationManager(const ApplicationManager &);
    ApplicationManager &operator=(const ApplicationManager &);
    static ApplicationManager *s_instance;

    ApplicationManagerPrivate *d;
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONMANAGER_H
