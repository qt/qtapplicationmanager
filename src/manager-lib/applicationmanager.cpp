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

#include <QCoreApplication>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTimer>
#include <QMimeDatabase>
#include <QDesktopServices>

#include "global.h"

#include "applicationdatabase.h"
#include "applicationmanager.h"
#include "application.h"
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "dbus-policy.h"
#include "qml-utilities.h"
#include "qtyaml.h"


#define AM_AUTHENTICATE_DBUS(RETURN_TYPE) \
    do { \
        if (!checkDBusPolicy(this, d->dbusPolicy, __FUNCTION__)) \
            return RETURN_TYPE(); \
    } while (false);


/*!
    \class ApplicationManager
    \internal
*/

/*!
    \qmltype ApplicationManager
    \inqmlmodule QtApplicationManager 1.0
    \brief The ApplicationManager singleton

    This singleton class is the core of the application manager. It provides both
    a DBus and a QML API for all of its functionality.

    To make QML programmers lifes easier, the class is derived from \c QAbstractListModel,
    so you can directly use this singleton as a model in your app-grid views.

    \target ApplicationManager Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c applicationId
        \li string
        \li The unique Id of an application represented as a string in reverse-dns form (e.g.
            \c com.pelagicore.foo)
    \row
        \li \c name
        \li string
        \li The name of the application. If possible already translated to the current locale.
    \row
        \li \c icon
        \li string
        \li The URL of the application's icon.

    \row
        \li \c isRunning
        \li bool
        \li A boolean value representing the run-state of the application.
    \row
        \li \c isStartingUp
        \li bool
        \li A boolean value telling if the application was started, but is not fully operational yet.
    \row
        \li \c isShutingDown
        \li bool
        \li A boolean value describing if the application is currently shuting down.
    \row
        \li \c isBlocked
        \li bool
        \li A boolean value that only gets set if the application manager needs to block the application
        from running: this is normally only the case while an update is applied.
    \row
        \li \c isUpdating
        \li bool
        \li A boolean value telling if the application is currently being installed or updated. If this
            is the case, then \c updateProgress can be used to track the actual progress.
    \row
        \li \c isRemovable
        \li bool
        \li A boolean value telling if this application is user-removable. This will be \c true for all
            dynamically installed 3rd-party applications and \c false for all system applications.

    \row
        \li \c updateProgress
        \li real
        \li In case IsUpdating is \c true, querying this role will give the actual progress as a floating-point
            value in the \c 0.0 to \c 1.0 range.

    \row
        \li \c codeFilePath
        \li string
        \li The filesystem path to the main "executable". The format of this file is dependent on the
        actual runtime though: for QML applications the "executable" is the \c main.qml file.

    \row
        \li \c categories
        \li list<string>
        \li The categories this application is registered for via its meta-dat file (this currently work in progress).

    \row
        \li \c version
        \li string
        \li The currently installed version of this application.

    \row
        \li \c application
        \li Application
        \li The underlying application object for quick access to the properties outside of a
            model delegate.
    \endtable

    Please note, that the index-based API is currently not available via DBus. The same functionality
    is however provided by the id-based API.

    After importing, you can just use the ApplicationManager singleton like so:

    \qml
    import QtQuick 2.0
    import QtApplicationManager 1.0

    ListView {
        id: appList
        model: ApplicationManager

        delegate: Text {
            text: name + "(" + id + ")"

            MouseArea {
                anchors.fill: parent
                onClick: ApplicationManager.startApplication(id)
            }
        }
    }
    \endqml
*/

/*!
    \qmlproperty int ApplicationManager::count

    This property holds the number of applications available.
*/

/*!
    \qmlsignal ApplicationManager::applicationWasReactivated(string id)

    This signal is emitted when an application is already running (in the background) and is
    started via the ApplicationManager API again.
    The window manager should take care of raising the application's window in this case.
*/

enum Roles
{
    Id = Qt::UserRole,
    Name,
    Icon,

    IsRunning,
    IsStartingUp,
    IsShutingDown,
    IsBlocked,
    IsUpdating,
    IsRemovable,

    UpdateProgress,

    CodeFilePath,
    RuntimeName,
    RuntimeParameters,
    BackgroundMode,
    Capabilities,
    Categories,
    Importance,
    Preload,
    Version,
    ApplicationItem
};


class ApplicationManagerPrivate
{
public:
    bool securityChecksEnabled = true;
    QVariantMap additionalConfiguration;
    ApplicationDatabase *database = nullptr;

    QMap<QByteArray, DBusPolicy> dbusPolicy;

    QVector<const Application *> apps;

    QString currentLocale;
    QHash<int, QByteArray> roleNames;

    QVector<IpcProxyObject *> interfaceExtensions;

    struct DebugWrapper
    {
        QString name;
        QStringList command;
        QVariantMap parameters;
        QStringList supportedRuntimes;
        QStringList supportedContainers;
    };
    QVector<DebugWrapper> debugWrappers;

    bool parseDebugWrapperSpecification(const QString &spec, DebugWrapper *outdw);

    ApplicationManagerPrivate();
    ~ApplicationManagerPrivate();
};


ApplicationManagerPrivate::ApplicationManagerPrivate()
{
    currentLocale = QLocale::system().name(); //TODO: language changes

    roleNames.insert(Id, "applicationId");
    roleNames.insert(Name, "name");
    roleNames.insert(Icon, "icon");
    roleNames.insert(IsRunning, "isRunning");
    roleNames.insert(IsStartingUp, "isStartingUp");
    roleNames.insert(IsShutingDown, "isShutingDown");
    roleNames.insert(IsBlocked, "isLocked");
    roleNames.insert(IsUpdating, "isUpdating");
    roleNames.insert(IsRemovable, "isRemovable");
    roleNames.insert(UpdateProgress, "updateProgress");
    roleNames.insert(CodeFilePath, "codeFilePath");
    roleNames.insert(RuntimeName, "runtimeName");
    roleNames.insert(RuntimeParameters, "runtimeParameters");
    roleNames.insert(BackgroundMode, "backgroundMode");
    roleNames.insert(Capabilities, "capabilities");
    roleNames.insert(Categories, "categories");
    roleNames.insert(Importance, "importance");
    roleNames.insert(Preload, "preload");
    roleNames.insert(Version, "version");
    roleNames.insert(ApplicationItem, "application");
}

ApplicationManagerPrivate::~ApplicationManagerPrivate()
{
    delete database;
}

ApplicationManager *ApplicationManager::s_instance = 0;

ApplicationManager *ApplicationManager::createInstance(ApplicationDatabase *adb, QString *error)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationManager::createInstance() was called a second time.");

    QScopedPointer<ApplicationManager> am(new ApplicationManager(adb));

    try {
        if (adb)
            am->d->apps = adb->read();
        am->registerMimeTypes();
    } catch (const Exception &e) {
        if (error)
            *error = e.what();
        return 0;
    }

    qmlRegisterSingletonType<ApplicationManager>("QtApplicationManager", 1, 0, "ApplicationManager",
                                                 &ApplicationManager::instanceForQml);
    qmlRegisterUncreatableType<const Application>("QtApplicationManager", 1, 0, "Application",
                                                  qL1S("Cannot create objects of type Application"));
    qmlRegisterUncreatableType<AbstractRuntime>("QtApplicationManager", 1, 0, "Runtime",
                                                qL1S("Cannot create objects of type Runtime"));
    qmlRegisterUncreatableType<AbstractContainer>("QtApplicationManager", 1, 0, "Container",
                                                  qL1S("Cannot create objects of type Container"));
    return s_instance = am.take();
}

ApplicationManager *ApplicationManager::instance()
{
    if (!s_instance)
        qFatal("ApplicationManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationManager::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
    return instance();
}

ApplicationManager::ApplicationManager(ApplicationDatabase *adb, QObject *parent)
    : QAbstractListModel(parent)
    , d(new ApplicationManagerPrivate())
{
    d->database = adb;
    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationManager::countChanged);

    QTimer::singleShot(0, this, &ApplicationManager::preload);
}

ApplicationManager::~ApplicationManager()
{
    delete d;
    s_instance = nullptr;
}

bool ApplicationManager::securityChecksEnabled() const
{
    return d->securityChecksEnabled;
}

void ApplicationManager::setSecurityChecksEnabled(bool enabled)
{
    d->securityChecksEnabled = enabled;
}

QVariantMap ApplicationManager::additionalConfiguration() const
{
    return d->additionalConfiguration;
}

void ApplicationManager::setAdditionalConfiguration(const QVariantMap &map)
{
    d->additionalConfiguration = map;
}

void ApplicationManager::setDebugWrapperConfiguration(const QVariantList &debugWrappers)
{
    // Example:
    //    debugWrappers:
    //    - name: gdbserver
    //      # %program% and %arguments% are internal variables
    //      command: [ /usr/bin/gdbserver, :%port%, %program%, --, %arguments% ]
    //      parameters:  # <name>: <default value>
    //        port: 5555
    //      supportedRuntimes: [ native, qml ]
    //      supportedContainers: [ process ]

    foreach (const QVariant &v, debugWrappers) {
        const QVariantMap &map = v.toMap();

        ApplicationManagerPrivate::DebugWrapper dw;
        dw.name = map.value("name").toString();
        dw.command = map.value("command").toStringList();
        dw.parameters = map.value("parameters").toMap();
        dw.supportedContainers = map.value("supportedContainers").toStringList();
        dw.supportedRuntimes = map.value("supportedRuntimes").toStringList();

        if (dw.name.isEmpty() || dw.command.isEmpty())
            continue;
        d->debugWrappers.append(dw);
    }
}

bool ApplicationManagerPrivate::parseDebugWrapperSpecification(const QString &spec, DebugWrapper *outdw)
{
    // Example:
    //    "gdbserver"
    //    "gdbserver: {port: 5555}"

    if (spec.isEmpty() || !outdw)
        return false;

    auto docs = QtYaml::variantDocumentsFromYaml(spec.toUtf8());
    if (docs.size() != 1)
        return false;
    const QVariant v = docs.at(0);

    QString name;
    QVariantMap userParams;

    switch (v.type()) {
    case QVariant::String:
        name = v.toString();
        break;
    case QVariant::Map: {
        const QVariantMap map = v.toMap();
        name = map.firstKey();
        userParams = map.first().toMap();
        break;
    }
    default:
        return false;
    }
    const DebugWrapper *basedw = nullptr;
    for (const DebugWrapper &dw : debugWrappers) {
        if (dw.name == name) {
            basedw = &dw;
            break;
        }
    }

    if (!basedw)
        return false;
    *outdw = *basedw;

    for (auto it = userParams.cbegin(); it != userParams.cend(); ++it) {
        QString key = it.key();

        auto keyit = outdw->parameters.find(key);
        if (keyit == outdw->parameters.end())
            return false;

        QString value = it.value().toString();
        *keyit = value;

        //commandLine
        key.prepend(qL1C('%'));
        key.append(qL1C('%'));
        std::for_each(outdw->command.begin(), outdw->command.end(), [key, value](QString &str) {
            str.replace(key, value);
        });
    }
    return true;
}

bool ApplicationManager::setDBusPolicy(const QVariantMap &yamlFragment)
{
    static const QVector<QByteArray> functions {
        QT_STRINGIFY(applicationIds),
        QT_STRINGIFY(get),
        QT_STRINGIFY(startApplication),
        QT_STRINGIFY(debugApplication),
        QT_STRINGIFY(stopApplication),
        QT_STRINGIFY(openUrl),
        QT_STRINGIFY(capabilities),
        QT_STRINGIFY(identifyApplication),
        QT_STRINGIFY(applicationState)
    };

    d->dbusPolicy = parseDBusPolicy(yamlFragment);

    for (auto it = d->dbusPolicy.cbegin(); it != d->dbusPolicy.cend(); ++it) {
       if (!functions.contains(it.key()))
           return false;
    }
    return true;
}

QVector<const Application *> ApplicationManager::applications() const
{
    return d->apps;
}

const Application *ApplicationManager::fromId(const QString &id) const
{
    foreach (const Application *app, d->apps) {
        if (app->id() == id)
            return app;
    }
    return 0;
}

const Application *ApplicationManager::fromProcessId(qint64 pid) const
{
    if (!pid)
        return 0;

    foreach (const Application *app, d->apps) {
        if (app->currentRuntime() && (app->currentRuntime()->applicationProcessId() == pid))
            return app;
    }
    return 0;
}

const Application *ApplicationManager::fromSecurityToken(const QByteArray &securityToken) const
{
    if (securityToken.size() != AbstractRuntime::SecurityTokenSize)
        return 0;

    foreach (const Application *app, d->apps) {
        if (app->currentRuntime() && app->currentRuntime()->securityToken() == securityToken)
            return app;
    }
    return 0;
}

const Application *ApplicationManager::schemeHandler(const QString &scheme) const
{
    foreach (const Application *app, d->apps) {
        foreach (const QString &mime, app->supportedMimeTypes()) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0)
                    && (mime.left(pos) == qL1S("x-scheme-handler"))
                    && (mime.mid(pos + 1) == scheme)) {
                return app;
            }
        }
    }
    return 0;
}

const Application *ApplicationManager::mimeTypeHandler(const QString &mimeType) const
{
    foreach (const Application *app, d->apps) {
        if (app->supportedMimeTypes().contains(mimeType))
            return app;
    }
    return 0;
}

void ApplicationManager::registerMimeTypes()
{
    QVector<QString> schemes;
    schemes << qSL("file") << qSL("http") << qSL("https");

    foreach (const Application *a, d->apps) {
        if (a->isAlias())
            continue;

        foreach (const QString &mime, a->supportedMimeTypes()) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0) && (mime.left(pos) == qL1S("x-scheme-handler")))
                schemes << mime.mid(pos + 1);
        }
    }
    foreach (const QString &scheme, schemes)
        QDesktopServices::setUrlHandler(scheme, this, "openUrlRelay");
}

bool ApplicationManager::startApplication(const Application *app, const QString &documentUrl, const QString &debugWrapperSpecification)
{
    if (!app) {
        qCWarning(LogSystem) << "Cannot start an invalid application";
        return false;
    }
    if (app->isLocked()) {
        qCWarning(LogSystem) << "Application" << app->id() << "is blocked - cannot start";
        return false;
    }
    AbstractRuntime *runtime = app->currentRuntime();

    ApplicationManagerPrivate::DebugWrapper debugWrapper;
    bool useDebugWrapper = false;
    if (!debugWrapperSpecification.isEmpty()) {
        useDebugWrapper = d->parseDebugWrapperSpecification(debugWrapperSpecification, &debugWrapper);
        if (!useDebugWrapper) {
            qCWarning(LogSystem) << "Application" << app->id() << "cannot be started by this debug wrapper specification:" << debugWrapperSpecification;
            return false;
        }
    }

    if (runtime) {
        switch (runtime->state()) {
        case AbstractRuntime::Startup:
        case AbstractRuntime::Active:
            if (useDebugWrapper) {
                qCDebug(LogSystem) << "Application" << app->id() << "is already running - cannot start with debug-wrapper" << debugWrapper.name;
                return false;
            }

            if (!documentUrl.isNull())
                runtime->openDocument(documentUrl);
            else if (!app->documentUrl().isNull())
                runtime->openDocument(app->documentUrl());

            emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());
            return true;

        case AbstractRuntime::Shutdown:
            return false;

        case AbstractRuntime::Inactive:
            break;
        }
    }

    auto runtimeManager = RuntimeFactory::instance()->manager(app->runtimeName());
    if (!runtimeManager) {
        qCWarning(LogSystem) << "No RuntimeManager found for runtime:" << app->runtimeName();
        return false;
    }

    bool inProcess = runtimeManager->inProcess();
    AbstractContainer *container = nullptr;
    QString containerId = qSL("process"); //TODO: ask SystemUI or use config file
    bool attachRuntime = false;

    if (useDebugWrapper) {
        if (!debugWrapper.supportedRuntimes.contains(app->runtimeName())) {
            qCDebug(LogSystem) << "Application" << app->id() << "is using the" << app->runtimeName()
                               << "runtime, which is not compatible with the requested debug-wrapper"
                               << debugWrapper.name;
            return false;
        }
        if (!debugWrapper.supportedContainers.contains(containerId)) {
            qCDebug(LogSystem) << "Application" << app->id() << "is using the" << containerId
                               << "container, which is not compatible with the requested debug-wrapper"
                               << debugWrapper.name;
            return false;
        }
    }

    if (!runtime) {
        if (!inProcess) {
            if (!useDebugWrapper) {
                // check quicklaunch pool
                QPair<AbstractContainer *, AbstractRuntime *> quickLaunch =
                        QuickLauncher::instance()->take(containerId, app->m_runtimeName);
                container = quickLaunch.first;
                runtime = quickLaunch.second;

                qCDebug(LogSystem) << "Found a quick-launch entry for container" << containerId
                                   << "and runtime" << app->m_runtimeName << "->" << container << runtime;

                if (!container && runtime) {
                    runtime->deleteLater();
                    qCCritical(LogSystem) << "ERROR: QuickLauncher provided a runtime without a container.";
                    return false;
                }
            }

            if (!container) {
                if (useDebugWrapper)
                    container = ContainerFactory::instance()->create(containerId, debugWrapper.command);
                else
                    container = ContainerFactory::instance()->create(containerId);
            }
            if (!container) {
                qCCritical(LogSystem) << "ERROR: Couldn't create Container for Application (" << app->id() <<")!";
                return false;
            }
            if (runtime)
                attachRuntime = true;
        }
        if (!runtime)
            runtime = RuntimeFactory::instance()->create(container, app);

        if (runtime && inProcess)  {
            // evil hook to get in-process mode working transparently
            emit inProcessRuntimeCreated(runtime);
        }
    }

    if (!runtime) {
        qCCritical(LogSystem) << "ERROR: Couldn't create Runtime for Application (" << app->id() <<")!";
        return false;
    }

    connect(runtime, &AbstractRuntime::stateChanged, this, [this, app]() {
        emit applicationRunStateChanged(app->isAlias() ? app->nonAliased()->id() : app->id(),
                                        applicationRunState(app->id()));
    });

    if (!documentUrl.isNull())
        runtime->openDocument(documentUrl);
    else if (!app->documentUrl().isNull())
        runtime->openDocument(app->documentUrl());

    emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());

    qCDebug(LogSystem) << "app:" << app->id() << "; document:" << documentUrl << "; runtime: " << runtime;

    if (inProcess) {
        bool ok = runtime->start();
        if (!ok)
            runtime->deleteLater();
        return ok;
    } else {
        auto f = [=]() {
            qCDebug(LogSystem) << "Container ready for app (" << app->id() <<")!";
            bool successfullyStarted = attachRuntime ? runtime->attachApplicationToQuickLauncher(app)
                                                     : runtime->start();
            if (!successfullyStarted)
                runtime->deleteLater(); // ~Runtime() will clean app->m_runtime

            return successfullyStarted;
        };

        if (container->isReady()) {
            // Since the container is already ready, start the app immediately
            return f();
        }
        else {
            // We postpone the starting of the application to a later point in time since the container is not ready yet
            connect(container, &AbstractContainer::ready, f);
            return true;       // we return true for now, since we don't know at this point in time whether the container will be able to start the application. TODO : fix
        }
    }
}

void ApplicationManager::stopApplication(const Application *app, bool forceKill)
{
    if (!app)
        return;
    AbstractRuntime *rt = app->currentRuntime();
    if (rt) {
        rt->stop(forceKill);
        rt->deleteLater(); // ~Runtime() will clean app->m_runtime
    }
}

void ApplicationManager::killAll()
{
    for (const Application *app : d->apps) {
        AbstractRuntime *rt = app->currentRuntime();
        if (rt) {
            rt->stop(true);
            delete rt;
        }
    }
}

/*!
    \qmlmethod bool ApplicationManager::startApplication(string id, string document)

    Tells the application manager to start the application identified by its unique \a id. The
    optional argument \a documentUrl will be supplied to the application as is - most commonly this
    is used to refer to a document to display.
    Returns \c true if the application id is valid and the application manager was able to start
    the runtime plugin. Returns \c false otherwise. Please note, that even though this call may
    indicate success, the application may still later fail to start correctly, since the actual
    startup within the runtime plugin may be asynchronous.
*/
bool ApplicationManager::startApplication(const QString &id, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    return startApplication(fromId(id), documentUrl);
}

bool ApplicationManager::debugApplication(const QString &id, const QString &debugWrapper, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    return startApplication(fromId(id), documentUrl, debugWrapper);
}

/*!
    \qmlmethod ApplicationManager::stopApplication(string id, bool forceKill)

    Tells the application manager to stop an application identified by its unique \a id. The
    meaning of the \a forceKill parameter is runtime dependent, but in general you should always try
    to stop an application with \a forceKill set to \c false first in order to allow a clean
    shutdown. Use \a forceKill set to \c true only as last resort to kill hanging applications.
*/
void ApplicationManager::stopApplication(const QString &id, bool forceKill)
{
    AM_AUTHENTICATE_DBUS(void)

    return stopApplication(fromId(id), forceKill);
}

/*!
    \qmlmethod bool ApplicationManager::openUrl(string url)

    Tries to match the supplied \a url against the internal MIME database. If a match is found,
    the corresponding application is started via startApplication() and \a url is supplied as a
    document to open.

    The function will return the result of startApplication() or \c false if no application was
    registered for the type and/or scheme of \a url.
*/
bool ApplicationManager::openUrl(const QString &urlStr)
{
    AM_AUTHENTICATE_DBUS(bool)

    QUrl url(urlStr);
    const Application *app = 0;
    if (url.isValid()) {
        QString scheme = url.scheme();
        if (scheme != qL1S("file")) {
            app = schemeHandler(scheme);

            if (app) {
                if (app->isAlias())
                    app = app->nonAliased();

                // try to find a better matching alias, if available
                foreach (const Application *alias, d->apps) {
                    if (alias->isAlias() && alias->nonAliased() == app) {
                        if (url.toString(QUrl::PrettyDecoded | QUrl::RemoveScheme) == alias->documentUrl()) {
                            app = alias;
                            break;
                        }
                    }
                }
            }
        }

        if (!app) {
            QMimeDatabase mdb;
            QMimeType mt = mdb.mimeTypeForUrl(url);

            app = mimeTypeHandler(mt.name());
            if (app->isAlias())
                app = app->nonAliased();
        }
    }
    if (app)
        return startApplication(app, urlStr);
    return false;
}

/*!
    \qmlmethod list<string> ApplicationManager::capabilities(string id)

    Returns a list of all capabilities granted by the user to the application identified by \a id.
    Will return an empty list, if the application \a id is not valid.
*/
QStringList ApplicationManager::capabilities(const QString &id) const
{
    AM_AUTHENTICATE_DBUS(QStringList)

    const Application *app = fromId(id);

    if (!app)
        return QStringList();

    return app->capabilities();
}

/*!
    \qmlmethod string ApplicationManager::identifyApplication(int pid, string securityToken)

    Calling this function will validate that the process running with process-identifier \a pid
    was indeed started by the application manager.
    Will return the application's \c id on success or an empty string otherwise.
*/
QString ApplicationManager::identifyApplication(qint64 pid) const
{
    AM_AUTHENTICATE_DBUS(QString)

    const Application *app1 = fromProcessId(pid);

    if (app1)
        return app1->id();
    else
        return QString();
}

bool ApplicationManager::lockApplication(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;
    if (!app->lock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    stopApplication(app, true);
    emitDataChanged(app, QVector<int> { IsRunning });
    return true;
}

bool ApplicationManager::unlockApplication(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;
    if (!app->unlock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    return true;
}

bool ApplicationManager::startingApplicationInstallation(Application *installApp)
{
    if (!installApp || installApp->id().isEmpty())
        return false;
    const Application *app = fromId(installApp->id());

    if (!RuntimeFactory::instance()->manager(installApp->runtimeName()))
        return false;

    if (app) { // update
        if (!lockApplication(app->id()))
            return false;
        installApp->mergeInto(const_cast<Application *>(app));
        app->m_state = Application::BeingUpdated;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });
    } else { // installation
        installApp->m_locked.ref();
        installApp->m_state = Application::BeingInstalled;
        installApp->m_progress = 0;
        beginInsertRows(QModelIndex(), d->apps.count(), d->apps.count());
        d->apps << installApp;
        endInsertRows();
        emit applicationAdded(installApp->id());
        try {
            if (d->database)
                d->database->write(d->apps);
        } catch (const Exception &) {
            emit applicationAboutToBeRemoved(installApp->id());
            beginRemoveRows(QModelIndex(), d->apps.count() - 1, d->apps.count() - 1);
            d->apps.removeLast();
            endRemoveRows();
            delete installApp;
            return false;
        }
        emitDataChanged(app);
    }
    return true;
}

bool ApplicationManager::startingApplicationRemoval(const QString &id)
{
    const Application *app = fromId(id);
    if (!app || app->m_locked.load() || (app->m_state != Application::Installed))
        return false;

    if (app->isBuiltIn())
        return false;

    if (!lockApplication(id))
        return false;

    app->m_state = Application::BeingRemoved;
    app->m_progress = 0;
    emitDataChanged(app, QVector<int> { IsUpdating });
    return true;
}

void ApplicationManager::progressingApplicationInstall(const QString &id, qreal progress)
{
    const Application *app = fromId(id);
    if (!app || app->m_state == Application::Installed)
        return;
    app->m_progress = progress;
    emitDataChanged(app, QVector<int> { UpdateProgress });
}

bool ApplicationManager::finishedApplicationInstall(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;

    switch (app->m_state) {
    case Application::Installed:
        return false;

    case Application::BeingInstalled:
    case Application::BeingUpdated:
        app->m_state = Application::Installed;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });

        unlockApplication(id);
        break;

    case Application::BeingRemoved: {
        int row = d->apps.indexOf(app);
        if (row >= 0) {
            emit applicationAboutToBeRemoved(app->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->apps.removeAt(row);
            endRemoveRows();
        }
        delete app;
        try {
            if (d->database)
                d->database->write(d->apps);
        } catch (const Exception &e) {
            qCCritical(LogSystem) << "ERROR: Application was removed successfully, but we could not save the app database:" << e.what();
            return false;
        }
        break;
    }
    }

    return true;
}

bool ApplicationManager::canceledApplicationInstall(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;

    switch (app->m_state) {
    case Application::Installed:
        return false;

    case Application::BeingInstalled: {
        int row = d->apps.indexOf(app);
        if (row >= 0) {
            emit applicationAboutToBeRemoved(app->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->apps.removeAt(row);
            endRemoveRows();
        }
        delete app;
        break;
    }
    case Application::BeingUpdated:
    case Application::BeingRemoved:
        app->m_state = Application::Installed;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });

        unlockApplication(id);
        break;
    }
    return true;
}

void ApplicationManager::preload()
{
    bool forcePreload = d->database && d->database->isTemporary();

    foreach (const Application *app, d->apps) {
        if (forcePreload || app->isPreloaded()) {
            if (!startApplication(app)) {
                qCWarning(LogSystem) << "WARNING: unable to start preload-enabled application" << app->id();
            }
        }
    }
}

void ApplicationManager::openUrlRelay(const QUrl &url)
{
    openUrl(url.toString());
}

void ApplicationManager::emitDataChanged(const Application *app, const QVector<int> &roles)
{
    int row = d->apps.indexOf(app);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), roles);

        static const auto appChanged = QMetaMethod::fromSignal(&ApplicationManager::applicationChanged);
        if (isSignalConnected(appChanged)) {
            QStringList stringRoles;
            foreach (const auto &role, roles)
                stringRoles << d->roleNames[role];
            emit applicationChanged(app->id(), stringRoles);
        }
    }
}

// item model part

int ApplicationManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->apps.count();
}

QVariant ApplicationManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    const Application *app = d->apps.at(index.row());

    switch (role) {
    case Id:
        return app->id();
    case Name: {
        QString name;
        if (!app->names().isEmpty()) {
            name = app->name(d->currentLocale);
            if (name.isEmpty())
                name = app->name(qSL("en"));
            if (name.isEmpty())
                name = app->name(qSL("en_US"));
            if (name.isEmpty())
                name = *app->names().constBegin();
        } else {
            name = app->id();
        }
        return name;
    }
    case Icon:
        return QUrl::fromLocalFile(app->icon());

    case IsRunning:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Active) : false;
    case IsStartingUp:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Startup) : false;
    case IsShutingDown:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Shutdown) : false;
    case IsBlocked:
        return app->isLocked();
    case IsUpdating:
        return app->state() != Application::Installed;
    case UpdateProgress:
        return app->progress();
    case IsRemovable:
        return !app->isBuiltIn();

    case CodeFilePath:
        return app->absoluteCodeFilePath();
    case RuntimeName:
        return app->runtimeName();
    case RuntimeParameters:
        return app->runtimeParameters();
    case BackgroundMode: {
        switch (app->backgroundMode()) {
        case Application::Auto:
            return qL1S("Auto");
        case Application::Never:
            return qL1S("Never");
        case Application::ProvidesVoIP:
            return qL1S("ProvidesVoIP");
        case Application::PlaysAudio:
            return qL1S("PlaysAudio");
        case Application::TracksLocation:
            return qL1S("TracksLocation");
        }
        return QString();
    }
    case Capabilities:
        return app->capabilities();
    case Categories:
        return app->categories();
    case Importance:
        return app->importance();
    case Preload:
        return app->isPreloaded();
    case Version:
        return app->version();
    case ApplicationItem:
        return QVariant::fromValue(app);
    }
    return QVariant();
}

QHash<int, QByteArray> ApplicationManager::roleNames() const
{
    return d->roleNames;
}

int ApplicationManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object ApplicationManager::get(int row)

    Retrieves the model data at \a row as a JavaScript object. Please see the
    \l {ApplicationManager Roles}{role names} for the expected object fields.

    Will return an empty object, if the specified \a row is invalid.

    \note This is very inefficient if you only want to access a single property from QML; use
          application instead to access the Application object's properties directly.
*/
QVariantMap ApplicationManager::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogSystem) << "invalid index:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(it.value(), data(index(row), it.key()));
    return map;
}

/*!
    \qmlmethod Application ApplicationManager::application(int index)

    Returns the Application object corresponding to the given \a index in the model, or null if
    the index is invalid.
*/
const Application *ApplicationManager::application(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "invalid index:" << index;
        return nullptr;
    }
    return d->apps.at(index);
}

/*!
    \qmlmethod Application ApplicationManager::application(string id)

    Returns the Application object corresponding to the given application \a id or null if the
    id does not exist.
*/
const Application *ApplicationManager::application(const QString &id) const
{
    return application(indexOfApplication(id));
}

/*!
    \qmlmethod int ApplicationManager::indexOfApplication(string id)

    Maps the application \a id to its position within the model.

    Will return \c -1, if the specified \a id is invalid.
*/
int ApplicationManager::indexOfApplication(const QString &id) const
{
    for (int i = 0; i < d->apps.size(); ++i) {
        if (d->apps.at(i)->id() == id)
            return i;
    }
    return -1;
}

/*!
    \qmlmethod list<string> ApplicationManager::applicationIds()

    Returns a list of all available application ids. This can be used to further query for specific
    information via get().
*/
QStringList ApplicationManager::applicationIds() const
{
    AM_AUTHENTICATE_DBUS(QStringList)

    QStringList ids;
    ids.reserve(d->apps.size());
    for (int i = 0; i < d->apps.size(); ++i)
        ids << d->apps.at(i)->id();
    return ids;
}

/*!
    \qmlmethod object ApplicationManager::get(string id)

    Retrieves the model data for the application identified by \a id as a JavaScript object. Please
    see the \l {ApplicationManager Roles}{role names} for the expected object fields.

    Will return an empty object, if the specified \a id is invalid.
*/
QVariantMap ApplicationManager::get(const QString &id) const
{
    AM_AUTHENTICATE_DBUS(QVariantMap)

    int index = indexOfApplication(id);
    if (index >= 0) {
        auto map = get(index);
        map.remove("application"); // cannot marshall QObject *
        return map;
    }
    return QVariantMap();
}

ApplicationManager::RunState ApplicationManager::applicationRunState(const QString &id) const
{
    AM_AUTHENTICATE_DBUS(ApplicationManager::RunState)

    int index = indexOfApplication(id);
    if (index < 0)
        return NotRunning;
    const Application *app = d->apps.at(index);
    if (!app->currentRuntime())
        return NotRunning;
    switch (app->currentRuntime()->state()) {
    case AbstractRuntime::Startup: return StartingUp;
    case AbstractRuntime::Active: return Running;
    case AbstractRuntime::Shutdown: return ShutingDown;
    default: return NotRunning;
    }
}
