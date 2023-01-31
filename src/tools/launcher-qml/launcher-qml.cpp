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

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>

#include <QSocketNotifier>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>
#include <QMetaObject>
#include <QRegularExpression>
#include <QCommandLineParser>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QLoggingCategory>
#include <private/qabstractanimation_p.h> // For QUnifiedTimer
#include <qplatformdefs.h>

#include <QtAppManLauncher/launchermain.h>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QQuickItem>
#  include <QQuickView>
#  include <QQuickWindow>

#  include <QtAppManLauncher/private/applicationmanagerwindow_p.h>
#else
#  include <QCoreApplication>
#endif

#include "dbusapplicationinterface.h"
#include "dbusapplicationinterfaceextension.h"
#include "dbusnotification.h"
#include "notification.h"
#include "qtyaml.h"
#include "global.h"
#include "logging.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "yamlpackagescanner.h"
#include "packageinfo.h"
#include "applicationinfo.h"
#include "startupinterface.h"
#include "dbus-utilities.h"
#include "startuptimer.h"
#include "processtitle.h"
#include "qml-utilities.h"
#include "launcher-qml_p.h"

// shared-main-lib
#include "cpustatus.h"
#if !defined(AM_HEADLESS)
#  include "frametimer.h"
#  include "gpustatus.h"
#endif
#include "iostatus.h"
#include "memorystatus.h"
#include "monitormodel.h"

#if defined(AM_HEADLESS)
#  include <QCoreApplication>
using Application = QCoreApplication;
#elif defined(AM_ENABLE_WIDGETS)
#  include <QApplication>
using Application = QApplication;
#else
#  include <QGuiApplication>
using Application = QGuiApplication;
#endif


QT_USE_NAMESPACE_AM


int main(int argc, char *argv[])
{
    StartupTimer::instance()->checkpoint("entered main");

    ProcessTitle::adjustArgumentCount(argc);

    QCoreApplication::setApplicationName(qSL("Qt Application Manager QML Launcher"));
    QCoreApplication::setOrganizationName(qSL("QtProject"));
    QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    if (qEnvironmentVariableIsSet("AM_NO_DLT_LOGGING"))
        Logging::setDltEnabled(false);

    // The common-lib is already registering the DLT Application for the application manager.
    // As the appID needs to be unique within the system, we cannot use the same appID and
    // need to change it as early as possible.

    // As we don't know the app-id yet, we are registering a place holder so we are able to see
    // something in the dlt logs if general errors occur.
    Logging::setDltApplicationId("QTLQ", "Qt Application Manager Launcher QML");
    Logging::setApplicationId("qml-launcher");
    Logging::initialize();

    try {
        const QString socket = QDir(qEnvironmentVariable("XDG_RUNTIME_DIR"))
                          .filePath(qEnvironmentVariable("WAYLAND_DISPLAY"));
        if (!QFileInfo::exists(socket))
            throw Exception("Cannot start application: no wayland display - expected socket at: %1").arg(socket);

        LauncherMain::initialize();
        Application app(argc, argv);
        LauncherMain launcher;

        QCommandLineParser clp;
        clp.addHelpOption();
        clp.addOption({ qSL("qml-debug"),   qSL("Enables QML debugging and profiling.") });
        clp.addOption({ qSL("quicklaunch"), qSL("Starts the launcher in the quicklaunching mode.") });
        clp.addOption({ qSL("directload") , qSL("The info.yaml to start (you can add '@<appid>' to start a specific app within the package, instead of the first one)."), qSL("info.yaml") });
        clp.process(app);

        bool quicklaunched = clp.isSet(qSL("quicklaunch"));
        QString directLoadManifest = clp.value(qSL("directload"));

        if (directLoadManifest.isEmpty())
            launcher.loadConfiguration();

        CrashHandler::setCrashActionConfiguration(launcher.runtimeConfiguration().value(qSL("crashAction")).toMap());
        // the verbose flag has already been factored into the rules:
        launcher.setupLogging(false, launcher.loggingRules(), QString(), launcher.useAMConsoleLogger());
        launcher.setupQmlDebugging(clp.isSet(qSL("qml-debug")));
        launcher.setupOpenGL(launcher.openGLConfiguration());
        launcher.setupIconTheme(launcher.iconThemeSearchPaths(), launcher.iconThemeName());
        launcher.registerWaylandExtensions();

        Logging::setDltLongMessageBehavior(launcher.dltLongMessageBehavior());

        StartupTimer::instance()->checkpoint("after basic initialization");

        if (!directLoadManifest.isEmpty()) {
            QString directLoadAppId;
            int appPos = directLoadManifest.indexOf(qSL("@"));
            if (appPos > 0) {
                directLoadAppId = directLoadManifest.mid(appPos + 1);
                directLoadManifest.truncate(appPos);
            }

            QFileInfo fi(directLoadManifest);
            if (!fi.exists() || fi.fileName() != qSL("info.yaml"))
                throw Exception("--directload needs a valid info.yaml file as parameter");
            directLoadManifest = fi.absoluteFilePath();
            new Controller(&launcher, quicklaunched, qMakePair(directLoadManifest, directLoadAppId));
        } else {
            launcher.setupDBusConnections();
            StartupTimer::instance()->checkpoint("after dbus initialization");
            new Controller(&launcher, quicklaunched);
        }

        return app.exec();

    } catch (const std::exception &e) {
        qCCritical(LogQmlRuntime) << "ERROR:" << e.what();
        return 2;
    }
}

Controller::Controller(LauncherMain *launcher, bool quickLaunched)
    : Controller(launcher, quickLaunched, qMakePair(QString{}, QString{}))
{ }

Controller::Controller(LauncherMain *launcher, bool quickLaunched, const QPair<QString, QString> &directLoad)
    : QObject(launcher)
    , m_quickLaunched(quickLaunched)
{
    connect(&m_engine, &QObject::destroyed, QCoreApplication::instance(), &QCoreApplication::quit);
    CrashHandler::setQmlEngine(&m_engine);

#if !defined(AM_HEADLESS)
    qmlRegisterType<ApplicationManagerWindow>("QtApplicationManager.Application", 2, 0, "ApplicationManagerWindow");
#endif
    qmlRegisterType<DBusNotification>("QtApplicationManager", 2, 0, "Notification");
    qmlRegisterType<DBusApplicationInterfaceExtension>("QtApplicationManager.Application", 2, 0,
                                                       "ApplicationInterfaceExtension");

    // monitor-lib
    qmlRegisterType<CpuStatus>("QtApplicationManager", 2, 0, "CpuStatus");
#if !defined(AM_HEADLESS)
    qmlRegisterType<FrameTimer>("QtApplicationManager", 2, 0, "FrameTimer");
    qmlRegisterType<GpuStatus>("QtApplicationManager", 2, 0, "GpuStatus");
#endif
    qmlRegisterType<IoStatus>("QtApplicationManager", 2, 0, "IoStatus");
    qmlRegisterType<MemoryStatus>("QtApplicationManager", 2, 0, "MemoryStatus");
    qmlRegisterType<MonitorModel>("QtApplicationManager", 2, 0, "MonitorModel");

    // monitor-lib
    qmlRegisterType<CpuStatus>("QtApplicationManager", 1, 0, "CpuStatus");
#if !defined(AM_HEADLESS)
    qmlRegisterType<FrameTimer>("QtApplicationManager", 1, 0, "FrameTimer");
    qmlRegisterType<GpuStatus>("QtApplicationManager", 1, 0, "GpuStatus");
#endif
    qmlRegisterType<IoStatus>("QtApplicationManager", 1, 0, "IoStatus");
    qmlRegisterType<MemoryStatus>("QtApplicationManager", 1, 0, "MemoryStatus");
    qmlRegisterType<MonitorModel>("QtApplicationManager", 1, 0, "MonitorModel");

    m_configuration = launcher->runtimeConfiguration();

    const QStringList resources = variantToStringList(m_configuration.value(qSL("resources")));
    for (const QString &resource: resources) {
        const QString path = QFileInfo(resource).isRelative() ? launcher->baseDir() + resource : resource;
        if (!loadResource(path))
            qCWarning(LogSystem) << "Cannot register resource:" << path;
    }

    QString absolutePluginPath;
    QStringList pluginPaths = variantToStringList(m_configuration.value(qSL("pluginPaths")));
    for (QString &path : pluginPaths) {
        if (QFileInfo(path).isRelative())
            path.prepend(launcher->baseDir());
        else if (absolutePluginPath.isEmpty())
            absolutePluginPath = path;

        qApp->addLibraryPath(path);
    }

    if (!absolutePluginPath.isEmpty()) {
        qCWarning(LogDeployment).nospace() << "Absolute plugin path in the runtime configuration "
                            "can lead to problems inside containers (e.g. " << absolutePluginPath << ")";
    }

    QString absoluteImportPath;
    QStringList importPaths = variantToStringList(m_configuration.value(qSL("importPaths")));
    for (QString &path : importPaths) {
        const QFileInfo fi(path);
        if (fi.isNativePath() && fi.isAbsolute() && absoluteImportPath.isEmpty())
            absoluteImportPath = path;
        m_engine.addImportPath(toAbsoluteFilePath(path, launcher->baseDir()));
    }

    if (!absoluteImportPath.isEmpty()) {
        qCWarning(LogDeployment).nospace() << "Absolute import path in the runtime configuration "
                            "can lead to problems inside containers (e.g. " << absoluteImportPath << ")";
    }

    StartupTimer::instance()->checkpoint("after application config initialization");

    // This is a bit of a hack to make ApplicationManagerWindow known as a sub-class
    // of QWindow. Without this, assigning an ApplicationManagerWindow to a QWindow*
    // property will fail with [unknown property type]. First seen when trying to
    // write a nested Wayland-compositor where assigning to WaylandOutput.window failed.
    {
        static const char registerWindowQml[] = "import QtQuick 2.0\nimport QtQuick.Window 2.2\nQtObject { }\n";
        QQmlComponent registerWindowComp(&m_engine);
        registerWindowComp.setData(QByteArray::fromRawData(registerWindowQml, sizeof(registerWindowQml) - 1), QUrl());
        QScopedPointer<QObject> dummy(registerWindowComp.create());
    }

    StartupTimer::instance()->checkpoint("after window registration");

    if (directLoad.first.isEmpty()) {
        m_applicationInterface = new DBusApplicationInterface(launcher->p2pDBusName(),
                                                             launcher->notificationDBusName(), this);
        connect(m_applicationInterface, &DBusApplicationInterface::startApplication,
                this, &Controller::startApplication);
        if (!m_applicationInterface->initialize(true))
            throw Exception("Could not connect to the application manager's ApplicationInterface on the peer D-Bus");

    } else {
        QMetaObject::invokeMethod(this, [this, directLoad]() {
            PackageInfo *pi;
            try {
                pi = YamlPackageScanner().scan(directLoad.first);
            } catch (const Exception &e) {
                qCCritical(LogQmlRuntime) << "Could not parse info.yaml file:" << e.what();
                QCoreApplication::exit(20);
                return;
            }
            const auto apps = pi->applications();
            const ApplicationInfo *a = apps.constFirst();
            if (!directLoad.second.isEmpty()) {
                auto it = std::find_if(apps.cbegin(), apps.cend(),
                                       [appId = directLoad.second](ApplicationInfo *appInfo) -> bool {
                    return (appInfo->id() == appId);
                });
                if (it == apps.end()) {
                    qCCritical(LogQmlRuntime) << "Could not find the requested application id"
                                              << directLoad.second << "within the info.yaml file";
                    QCoreApplication::exit(21);
                    return;
                }
                a = *it;
            }

            startApplication(QFileInfo(directLoad.first).absolutePath(), a->codeFilePath(),
                             QString(), QString(), a->toVariantMap(), QVariantMap());
        }, Qt::QueuedConnection);
    }

    const QString quicklaunchQml = m_configuration.value((qSL("quicklaunchQml"))).toString();
    if (!quicklaunchQml.isEmpty() && quickLaunched) {
        QQmlComponent quicklaunchComp(&m_engine, filePathToUrl(quicklaunchQml, launcher->baseDir()));
        if (!quicklaunchComp.isError()) {
            QScopedPointer<QObject> quicklaunchInstance(quicklaunchComp.create());
        } else {
            const QList<QQmlError> errors = quicklaunchComp.errors();
            for (const QQmlError &error : errors)
                qCCritical(LogQmlRuntime) << error;
        }
    }

    StartupTimer::instance()->checkpoint("after application interface initialization");
}

void Controller::startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                                  const QString &mimeType, const QVariantMap &application,
                                  const QVariantMap &systemProperties)
{
    if (m_launched)
        return;
    m_launched = true;

    static QString applicationId = application.value(qSL("id")).toString();
    LauncherMain::instance()->setApplicationId(applicationId);

    if (m_quickLaunched) {
        //StartupTimer::instance()->createReport(applicationId  + qSL(" [process launch]"));
        StartupTimer::instance()->reset();
    } else {
        StartupTimer::instance()->checkpoint("starting application");
    }

    //Change the DLT Application description, to easily identify the application on the DLT logs.
    const QVariantMap dlt = qdbus_cast<QVariantMap>(application.value(qSL("dlt")));
    QByteArray dltId = dlt.value(qSL("id")).toString().toLocal8Bit();
    QByteArray dltDescription = dlt.value(qSL("description")).toString().toLocal8Bit();
    if (dltId.isEmpty()) {
        char uniqueId[5];
        qsnprintf(uniqueId, sizeof(uniqueId), "A%03d", application.value(qSL("uniqueNumber")).toInt());
        dltId = uniqueId;
    }
    if (dltDescription.isEmpty())
        dltDescription = QByteArray("Qt Application Manager App: ") + applicationId.toLocal8Bit();
    Logging::setDltApplicationId(dltId, dltDescription);
    Logging::registerUnregisteredDltContexts();

    // Dress up the ps output to make it easier to correlate all the launcher processes
    ProcessTitle::augmentCommand(applicationId.toLocal8Bit().constData());

    QVariantMap runtimeParameters = qdbus_cast<QVariantMap>(application.value(qSL("runtimeParameters")));

    qCDebug(LogQmlRuntime) << "loading" << applicationId << "- main:" << qmlFile << "- document:" << document
                           << "- mimeType:" << mimeType << "- parameters:" << runtimeParameters
                           << "- baseDir:" << baseDir;

    if (!QDir::setCurrent(baseDir)) {
        qCCritical(LogQmlRuntime) << "could not set the current directory to" << baseDir;
        QCoreApplication::exit(2);
        return;
    }

    if (!applicationId.isEmpty()) {
        // shorten application id to make the debug output more readable

        auto sl = applicationId.split(qL1C('.'));
        applicationId.clear();
        for (int i = 0; i < sl.size() - 1; ++i) {
            applicationId.append(sl.at(i).at(0));
            applicationId.append(qL1C('.'));
        }
        applicationId.append(sl.last());
        Logging::setApplicationId(applicationId.toLocal8Bit());
    } else {
        qCCritical(LogQmlRuntime) << "did not receive an application id";
        QCoreApplication::exit(2);
        return;
    }

    QVariant resVar = runtimeParameters.value(qSL("resources"));
    const QVariantList resources = (resVar.type() == QVariant::String) ? QVariantList{resVar}
                                                                       : qdbus_cast<QVariantList>(resVar);
    for (const QVariant &resource : resources) {
        if (!loadResource(resource.toString()))
            qCWarning(LogQmlRuntime) << "Cannot register resource:" << resource.toString();
    }

    const QUrl qmlFileUrl = filePathToUrl(qmlFile, baseDir);
    const QString qmlFileStr = urlToLocalFilePath(qmlFileUrl);

    if (!QFile::exists(qmlFileStr)) {
        qCCritical(LogQmlRuntime) << "could not load" << qmlFile << ": file does not exist";
        QCoreApplication::exit(2);
        return;
    }

    if (m_applicationInterface) {
        m_engine.rootContext()->setContextProperty(qSL("ApplicationInterface"), m_applicationInterface);

        m_applicationInterface->m_name = qdbus_cast<QVariantMap>(application.value(qSL("displayName")));
        m_applicationInterface->m_icon = application.value(qSL("displayIcon")).toString();
        m_applicationInterface->m_version = application.value(qSL("version")).toString();

        QVariantMap &svm = m_applicationInterface->m_systemProperties;
        svm = qdbus_cast<QVariantMap>(systemProperties);
        for (auto it = svm.begin(); it != svm.end(); ++it)
            it.value() = convertFromDBusVariant(it.value());

        QVariantMap &avm = m_applicationInterface->m_applicationProperties;
        avm = qdbus_cast<QVariantMap>(application.value(qSL("applicationProperties")));
        for (auto it = avm.begin(); it != avm.end(); ++it)
            it.value() = convertFromDBusVariant(it.value());

        connect(m_applicationInterface, &ApplicationInterface::slowAnimationsChanged,
                LauncherMain::instance(), &LauncherMain::setSlowAnimations);
    }

#if !defined(AM_HEADLESS)
    // Going through the LauncherMain instance here is a bit weird, and should be refactored
    // sometime. Having the flag there makes sense though, because this class can also be used for
    // custom launchers.
    connect(LauncherMain::instance(), &LauncherMain::slowAnimationsChanged,
            this, &Controller::updateSlowAnimations);

    updateSlowAnimations(LauncherMain::instance()->slowAnimations());

    // we need to catch all show events to apply the slow-animations
    QCoreApplication::instance()->installEventFilter(this);
#endif

    QStringList startupPluginFiles = variantToStringList(m_configuration.value(qSL("plugins")).toMap().value(qSL("startup")));
    QVector<StartupInterface *> startupPlugins;
    try {
        startupPlugins = loadPlugins<StartupInterface>("startup", startupPluginFiles);
    } catch (const Exception &e) {
        qCCritical(LogQmlRuntime) << qPrintable(e.errorString());
        QCoreApplication::exit(2);
        return;
    }
    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->initialize(m_applicationInterface ? m_applicationInterface->systemProperties() : QVariantMap());

    bool loadDummyData = runtimeParameters.value(qSL("loadDummyData")).toBool()
            || m_configuration.value(qSL("loadDummydata")).toBool();

    if (loadDummyData) {
        qCDebug(LogQmlRuntime) << "loading dummy-data";
        loadQmlDummyDataFiles(&m_engine, QFileInfo(qmlFileStr).path());
    }

    QVariant pluginPaths = runtimeParameters.value(qSL("pluginPaths"));
    const QVariantList ppvl = (pluginPaths.type() == QVariant::String) ? QVariantList{pluginPaths}
                                                                       : qdbus_cast<QVariantList>(pluginPaths);
    for (const QVariant &v : ppvl) {
        const QString path = v.toString();
        if (QFileInfo(path).isRelative())
            qApp->addLibraryPath(QDir().absoluteFilePath(path));
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute plugin path in info file for safety reasons:" << path;
    }
    qCDebug(LogQmlRuntime) << "Plugin paths:" << qApp->libraryPaths();

    QVariant imports = runtimeParameters.value(qSL("importPaths"));
    const QVariantList ipvl = (imports.type() == QVariant::String) ? QVariantList{imports}
                                                                   : qdbus_cast<QVariantList>(imports);
    for (const QVariant &v : ipvl) {
        const QString path = v.toString();
        const QFileInfo fi(path);
        if (!(fi.isNativePath() && fi.isAbsolute()))
            m_engine.addImportPath(toAbsoluteFilePath(path));
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute import path in info file for safety reasons:" << path;
    }
    qCDebug(LogQmlRuntime) << "Qml import paths:" << m_engine.importPathList();

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->beforeQmlEngineLoad(&m_engine);

    StartupTimer::instance()->checkpoint("after loading plugins and import paths");

    // protect our namespace from this point onward
    qmlProtectModule("QtApplicationManager", 1);
    qmlProtectModule("QtApplicationManager.Application", 1);
    qmlProtectModule("QtApplicationManager", 2);
    qmlProtectModule("QtApplicationManager.Application", 2);

    m_engine.rootContext()->setContextProperty(qSL("StartupTimer"), StartupTimer::instance());
    m_engine.load(qmlFileUrl);

    StartupTimer::instance()->checkpoint("after engine loading main qml file");

    auto topLevels = m_engine.rootObjects();

    if (Q_UNLIKELY(topLevels.isEmpty() || !topLevels.at(0))) {
        qCCritical(LogSystem) << "Failed to load component" << qmlFile << ": no root object";
        QCoreApplication::exit(3);
        return;
    }

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->afterQmlEngineLoad(&m_engine);

    bool createStartupReportNow = true;

#if !defined(AM_HEADLESS)
    QObject *topLevel = topLevels.at(0);
    m_window = qobject_cast<QQuickWindow *>(topLevel);
    if (!m_window) {
        QQuickItem *contentItem = qobject_cast<QQuickItem *>(topLevel);
        if (contentItem) {
            QQuickView* view = new QQuickView(&m_engine, nullptr);
            m_window = view;
            view->setContent(qmlFileUrl, nullptr, topLevel);
            view->setVisible(contentItem->isVisible());
            connect(contentItem, &QQuickItem::visibleChanged, this, [view, contentItem]() {
                view->setVisible(contentItem->isVisible());
            });
        }
    } else {
        if (!m_engine.incubationController())
            m_engine.setIncubationController(m_window->incubationController());
        createStartupReportNow = false; // create the startup report later, since we have a window
    }

    StartupTimer::instance()->checkpoint("after creating and setting application window");

    if (m_window) {
        // not sure if this is needed .. or even the best thing to do ... see connects above, they seem to work better
        QObject::connect(&m_engine, &QQmlEngine::quit, m_window, &QObject::deleteLater);

        // create the startup report on first frame drawn
        static QMetaObject::Connection conn = QObject::connect(m_window, &QQuickWindow::frameSwapped,
                                                               this, [this, startupPlugins]() {
            // this is a queued signal, so there may be still one in the queue after calling disconnect()
            if (conn) {
                QObject::disconnect(conn);

                auto st = StartupTimer::instance();
                st->checkFirstFrame();
                st->createAutomaticReport(applicationId);

                for (StartupInterface *iface : qAsConst(startupPlugins))
                    iface->afterWindowShow(m_window);
            }
        });

        if (m_configuration.contains(qSL("backgroundColor"))) {
            QSurfaceFormat surfaceFormat = m_window->format();
            surfaceFormat.setAlphaBufferSize(8);
            m_window->setFormat(surfaceFormat);
            m_window->setClearBeforeRendering(true);
            m_window->setColor(QColor(m_configuration.value(qSL("backgroundColor")).toString()));
        }
    }

    // needed, even though we do not explicitly show() the window any more
    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->beforeWindowShow(m_window);

#else
    m_engine.setIncubationController(new HeadlessIncubationController(&m_engine));
#endif
    qCDebug(LogQmlRuntime) << "component loading and creating complete.";

    StartupTimer::instance()->checkpoint("component loading and creating complete.");
    if (createStartupReportNow)
        StartupTimer::instance()->createAutomaticReport(applicationId);

    if (!document.isEmpty() && m_applicationInterface)
        emit m_applicationInterface->openDocument(document, mimeType);
}

#if !defined(AM_HEADLESS)
bool Controller::eventFilter(QObject *o, QEvent *e)
{
    if (e && (e->type() == QEvent::Show) && qobject_cast<QQuickWindow *>(o)) {
        auto window = static_cast<QQuickWindow *>(o);
        m_allWindows.append(window);
        updateSlowAnimationsForWindow(window);
    }
    return QObject::eventFilter(o, e);
}

void Controller::updateSlowAnimationsForWindow(QQuickWindow *window)
{
    // QUnifiedTimer are thread-local. To also slow down animations running in the SG thread
    // we need to enable the slow mode in this timer as well.
    QMetaObject::Connection *connection = new QMetaObject::Connection;
    *connection = connect(window, &QQuickWindow::beforeRendering,
                          this, [connection] {
        if (connection && *connection) {
#  if defined(Q_CC_MSVC)
            qApp->disconnect(*connection); // MSVC cannot distinguish between static and non-static overloads in lambdas
#  else
            QObject::disconnect(*connection);
#  endif
            QUnifiedTimer::instance()->setSlowModeEnabled(LauncherMain::instance()->slowAnimations());
            delete connection;
        }
    }, Qt::DirectConnection);
}

void Controller::updateSlowAnimations(bool isSlow)
{
    QUnifiedTimer::instance()->setSlowModeEnabled(isSlow);

    for (auto it = m_allWindows.begin(); it != m_allWindows.end(); ) {
        QPointer<QQuickWindow> window = *it;
        if (!window) {
            m_allWindows.erase(it);
        } else {
            updateSlowAnimationsForWindow(window.data());
            ++it;
        }
    }
}

#endif  // !defined(AM_HEADLESS)


HeadlessIncubationController::HeadlessIncubationController(QObject *parent)
    : QObject(parent)
{
    startTimer(50);
}

void HeadlessIncubationController::timerEvent(QTimerEvent *)
{
    incubateFor(25);
}

#include "moc_launcher-qml_p.cpp"
