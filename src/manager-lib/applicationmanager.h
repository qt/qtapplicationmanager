// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtQml/QJSValue>
#include <QtAppManCommon/global.h>
#include <QtAppManManager/application.h>

#if defined(Q_MOC_RUN) && !defined(__attribute__) && !defined(__declspec)
#  define QT_PREPEND_NAMESPACE_AM(name) QtAM::name
#  error "This pre-processor scope is for qdbuscpp2xml only, but it seems something else triggered it"
#endif

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
signals:
    // Emitted every time a new Runtime object is created
    void newRuntimeCreated(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime);
};

class ApplicationManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationManager 2.0 SINGLETON")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool singleProcess READ isSingleProcess CONSTANT)
    Q_PROPERTY(bool shuttingDown READ isShuttingDown NOTIFY shuttingDownChanged SCRIPTABLE false)
    Q_PROPERTY(bool securityChecksEnabled READ securityChecksEnabled CONSTANT)
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

    void addApplication(ApplicationInfo *appInfo, Package *package);
    void removeApplication(ApplicationInfo *appInfo, Package *package);
    QVector<Application *> applications() const;

    Application *fromId(const QString &id) const;
    QVector<Application *> fromProcessId(qint64 pid) const;
    Application *fromSecurityToken(const QByteArray &securityToken) const;
    QVector<Application *> schemeHandlers(const QString &scheme) const;
    QVector<Application *> mimeTypeHandlers(const QString &mimeType) const;

    bool startApplicationInternal(const QString &appId, const QString &documentUrl = QString(),
                                  const QString &documentMimeType = QString(),
                                  const QString &debugWrapperSpecification = QString(),
                                  QVector<int> &&stdioRedirections = QVector<int>()) Q_DECL_NOEXCEPT_EXPR(false);
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
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Application) *application(int index) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Application) *application(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(QT_PREPEND_NAMESPACE_AM(Application) *application) const;

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
    Q_SCRIPTABLE QT_PREPEND_NAMESPACE_AM(Am::RunState) applicationRunState(const QString &id) const;

    ApplicationManagerInternalSignals internalSignals;

public slots:
    void shutDown();

signals:
    Q_SCRIPTABLE void applicationRunStateChanged(const QString &id, QT_PREPEND_NAMESPACE_AM(Am::RunState) runState);
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
    void shutDownFinished();

private slots:
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
