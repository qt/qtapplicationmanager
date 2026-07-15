// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
// Qt-Security score:critical reason:data-parser

#include <memory>

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
#include <QLibraryInfo>
#include <QTimer>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QLoggingCategory>
#include <private/qabstractanimation_p.h> // For QUnifiedTimer
#include <qplatformdefs.h>

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>

#include "applicationmain.h"
#include "qtyaml.h"
#include "global.h"
#include "logging.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "startupinterface.h"
#include "dbus-utilities.h"
#include "startuptimer.h"
#include "processtitle.h"
#include "qml-utilities.h"
#include "launcher-qml_p.h"
#include "systemd.h"
#include "applicationmanagerwindow.h"

using namespace Qt::StringLiterals;


QT_USE_NAMESPACE_AM


int main(int argc, char *argv[])
{
    StartupTimer::instance()->checkpoint("entered main");

    QCoreApplication::setApplicationName(u"Qt Application Manager QML Launcher"_s);
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_AM_VERSION_STR));

    if (Logging::isDltAvailable()) {
        if (qEnvironmentVariableIntValue("AM_NO_DLT_LOGGING") == 1)
            Logging::setDltEnabled(false);

        // The common-lib is already registering the DLT Application for the application manager.
        // As the appID needs to be unique within the system, we cannot use the same appID and
        // need to change it as early as possible.

        // As we don't know the app-id yet, we are registering a place holder so we are able to see
        // something in the dlt logs if general errors occur.
        Logging::setDltApplicationId("QTLQ", "Qt Application Manager Launcher QML");
    }
    Logging::setApplicationId("qml-launcher");
    Logging::initialize();

    std::unique_ptr<ApplicationMain> am;
    std::unique_ptr<Controller> controller; // this needs to die BEFORE qApp does

    try {
        const QString socket = QDir(qEnvironmentVariable("XDG_RUNTIME_DIR"))
                          .filePath(qEnvironmentVariable("WAYLAND_DISPLAY"));
        if (!QFileInfo::exists(socket))
            throw Exception("Cannot start application: no wayland display - expected socket at: %1").arg(socket);

        am = std::make_unique<ApplicationMain>(argc, argv);

        QCommandLineParser clp;
        clp.addHelpOption();
        clp.addOption({ u"qml-debug"_s,   u"Enables QML debugging and profiling."_s });
        clp.addOption({ u"quicklaunch"_s, u"Starts the launcher in the quicklaunching mode."_s });
        clp.process(*am);

        bool quicklaunched = clp.isSet(u"quicklaunch"_s);
        if (quicklaunched)
            ProcessTitle::setTitle("[quicklaunch]");

        am->loadConfiguration();

        Systemd::instance()->setExtraJournalFields(am->extraJournalFields());

        const QVariantMap ca = am->runtimeConfiguration().value(u"crashAction"_s).toMap();
        const QVariantMap ca_sfti = ca.value(u"stackFramesToIgnore"_s).toMap();
        CrashHandler::setCrashActionConfiguration(ca.value(u"printBacktrace"_s, true).toBool(),
                                                  ca.value(u"printQmlStack"_s, true).toBool(),
                                                  ca.value(u"waitForGdbAttach"_s, 0).toInt(),
                                                  ca.value(u"dumpCore"_s, true).toBool(),
                                                  ca.value(u"dumpCoreOnWatchdogKill"_s, false).toBool(),
                                                  ca_sfti.value(u"onCrash"_s, -1).toInt(),
                                                  ca_sfti.value(u"onException"_s, -1).toInt());
        // the verbose flag has already been factored into the rules:
        am->setupLogging(false, am->loggingRules(), QString(), am->useAMConsoleLogger());
        am->setupQmlDebugging(clp.isSet(u"qml-debug"_s));
        am->setupWatchdog(am->watchdogConfiguration());
        am->setupOpenGL(am->openGLConfiguration());
        am->setupIconTheme(am->iconThemeSearchPaths(), am->iconThemeName());
        am->registerWaylandExtensions();

        if (Logging::isDltAvailable())
            Logging::setDltLongMessageBehavior(am->dltLongMessageBehavior());

        StartupTimer::instance()->checkpoint("after basic initialization");

        am->setupDBusConnections();
        StartupTimer::instance()->checkpoint("after dbus initialization");
        controller = std::make_unique<Controller>(am.get(), quicklaunched);

    } catch (const std::exception &e) {
        qCCritical(LogQmlRuntime) << "ERROR:" << e.what();
        ApplicationMain::errorExit();
    }

    // we want the exec() outside of the try/catch block, so stray user exceptions trigger the
    // CrashHandler's set_terminate callback.
    return ApplicationMain::exec();
}

Controller::Controller(ApplicationMain *am, bool quickLaunched)
    : QObject(am)
    , m_quickLaunched(quickLaunched)
{
    am->setQuitOnLastWindowClosed(false);
    connect(am, &QGuiApplication::lastWindowClosed, this, [am] {
        am->handleQuit();
        bool ok;
        int qt = am->runtimeConfiguration().value(u"quitTime"_s).toInt(&ok);
        if (!ok || qt < 0)
            qt = 250;
        qt *= timeoutFactor();
        QTimer::singleShot(qt, QCoreApplication::instance(), [] {
            QCoreApplication::instance()->quit();
        });
    });

    connect(&m_engine, &QObject::destroyed, QCoreApplication::instance(), &QCoreApplication::quit);
    CrashHandler::setQmlEngine(&m_engine);

    m_configuration = am->runtimeConfiguration();

    const QStringList resources = variantToStringList(m_configuration.value(u"resources"_s));
    for (const QString &resource: resources) {
        const QString path = QFileInfo(resource).isRelative() ? am->baseDir() + resource : resource;

        try {
            loadResource(path);
        } catch (const Exception &e) {
            qCWarning(LogSystem).noquote() << e.errorString();
        }
    }

    QString absolutePluginPath;
    QStringList pluginPaths = variantToStringList(m_configuration.value(u"pluginPaths"_s));
    for (QString &path : pluginPaths) {
        if (QFileInfo(path).isRelative())
            path.prepend(am->baseDir());
        else if (absolutePluginPath.isEmpty())
            absolutePluginPath = path;

        qApp->addLibraryPath(path);
    }

    if (!absolutePluginPath.isEmpty()) {
        qCWarning(LogDeployment).nospace() << "Absolute plugin path in the runtime configuration "
                            "can lead to problems inside containers (e.g. " << absolutePluginPath << ")";
    }

    QString absoluteImportPath;
    QStringList importPaths = variantToStringList(m_configuration.value(u"importPaths"_s));
    for (QString &path : importPaths) {
        const QFileInfo fi(path);
        if (fi.isNativePath() && fi.isAbsolute() && absoluteImportPath.isEmpty())
            absoluteImportPath = path;
        m_engine.addImportPath(toAbsoluteFilePath(path, am->baseDir()));
    }

    if (!absoluteImportPath.isEmpty()) {
        qCWarning(LogDeployment).nospace() << "Absolute import path in the runtime configuration "
                            "can lead to problems inside containers (e.g. " << absoluteImportPath << ")";
    }

    StartupTimer::instance()->checkpoint("after application config initialization");

    am->connectDBusInterfaces(true);

    connect(am, &ApplicationMain::startApplication,
            this, &Controller::startApplication);

    StartupTimer::instance()->checkpoint("after D-Bus connections");

    if (quickLaunched) {
        const QString quicklaunchQml = m_configuration.value((u"quicklaunchQml"_s)).toString();
        if (!quicklaunchQml.isEmpty()) {
            QQmlComponent quicklaunchComp(&m_engine, filePathToUrl(quicklaunchQml, am->baseDir()));
            if (!quicklaunchComp.isError()) {
                std::unique_ptr<QObject> quicklaunchInstance(quicklaunchComp.create());
            } else {
                const QList<QQmlError> errors = quicklaunchComp.errors();
                for (const QQmlError &error : errors)
                    qCCritical(LogQmlRuntime) << error;
            }
            StartupTimer::instance()->checkpoint("after quicklaunchQml instantiation");
        }
        StartupTimer::instance()->createReport(u"[QML quicklauncher]"_s);
    }
}

void Controller::startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                                  const QString &mimeType, const QVariantMap &application,
                                  const QVariantMap &systemProperties, const QVariantMap &extraAppDirs)
{
    if (m_launched)
        return;
    m_launched = true;

    static const QString applicationId = application.value(u"id"_s).toString();

    if (applicationId.isEmpty()) {
        qCCritical(LogQmlRuntime) << "did not receive an application id";
        QCoreApplication::exit(2);
        return;
    }

    if (m_quickLaunched) {
        StartupTimer::instance()->reset();
        StartupTimer::instance()->checkpoint("quick-launching application");
    } else {
        StartupTimer::instance()->checkpoint("starting application");
    }

    const QStringList applicationIdParts = applicationId.split(u'.');
    if (applicationIdParts.size() < 2) {
        Logging::setApplicationId(applicationId.toLocal8Bit());
        QCoreApplication::setApplicationName(applicationId);
    } else {
        const QString appName = applicationIdParts.last();
        QString domainName;
        QString loggingId;

        for (qsizetype i = 0; i < applicationIdParts.size() - 1; ++i) {
            // reverse the reverse-DNS domain name
            if (!domainName.isEmpty())
                domainName.prepend(u'.');
            domainName.prepend(applicationIdParts.at(i));

            // shorten application id to make the debug output more readable
            loggingId.append(applicationIdParts.at(i).at(0));
            loggingId.append(u'.');
        }
        loggingId.append(applicationIdParts.last());
        Logging::setApplicationId(loggingId.toLocal8Bit());

        // we need these to be unique, because other libraries (e.g. QtInsights) expect them to be
        QCoreApplication::setApplicationName(appName);
        QCoreApplication::setOrganizationName(domainName);
        QCoreApplication::setOrganizationDomain(domainName);
    }
    if (const auto version = application.value(u"version"_s).toString(); !version.isEmpty())
        QCoreApplication::setApplicationVersion(version);

    if (Logging::isDltAvailable() && Logging::isDltEnabled()) {
        // Change the DLT Application description, to easily identify the application on the DLT logs.
        const auto dlt = qdbus_cast<QVariantMap>(application.value(u"dlt"_s));
        QByteArray dltId = dlt.value(u"id"_s).toString().toLocal8Bit();
        QByteArray dltDescription = dlt.value(u"description"_s).toString().toLocal8Bit();

        if (dltId.isEmpty()) {
            qCCritical(LogQmlRuntime) << "did not receive a DLT id, but DLT is enabled";
            QCoreApplication::exit(2);
            return;
        }
        if (dltDescription.isEmpty())
            dltDescription = QByteArray("Qt Application Manager App: ") + applicationId.toLocal8Bit();

        Logging::setDltApplicationId(dltId, dltDescription);
        Logging::registerUnregisteredDltContexts();
    }
    // Dress up the ps output to make it easier to correlate all the launcher processes
    ProcessTitle::setTitle(applicationId.toLocal8Bit());

    auto *am = ApplicationMain::instance();
    am->setApplication(convertFromDBusVariant(application).toMap());
    am->setSystemProperties(convertFromDBusVariant(systemProperties).toMap());
    am->setExtraAppDirs(convertFromDBusVariant(extraAppDirs).toMap());

    auto runtimeParameters = qdbus_cast<QVariantMap>(application.value(u"runtimeParameters"_s));

    qCDebug(LogQmlRuntime) << "Loading" << applicationId << "from" << qmlFile;
    if (!document.isEmpty())
        qCDebug(LogQmlRuntime) << " * document:" << document;
    if (!runtimeParameters.isEmpty())
        qCDebug(LogQmlRuntime) << " * parameters:" << runtimeParameters;

    if (!QDir::setCurrent(baseDir)) {
        qCCritical(LogQmlRuntime) << "could not set the current directory to" << baseDir;
        QCoreApplication::exit(2);
        return;
    }

    QVariant resVar = runtimeParameters.value(u"resources"_s);
    const QVariantList resources = (resVar.metaType() == QMetaType::fromType<QString>())
            ? QVariantList{resVar}
            : qdbus_cast<QVariantList>(resVar);

    for (const QVariant &resource : resources) {
        try {
            loadResource(resource.toString());
        } catch (const Exception &e) {
            qCWarning(LogSystem).noquote() << e.errorString();
        }
    }

    const QUrl qmlFileUrl = filePathToUrl(qmlFile, baseDir);
    const QString qmlFileStr = urlToLocalFilePath(qmlFileUrl);

    if (!QFile::exists(qmlFileStr)) {
        qCCritical(LogQmlRuntime) << "could not load" << qmlFile << ": file does not exist";
        QCoreApplication::exit(2);
        return;
    }

    connect(ApplicationMain::instance(), &ApplicationMain::slowAnimationsChanged,
            this, &Controller::updateSlowAnimations);
    updateSlowAnimations(ApplicationMain::instance()->slowAnimations());

    // we need to catch all show events to apply the slow-animations
    QCoreApplication::instance()->installEventFilter(this);

    QStringList systemStartupPluginPaths;
    const QDir systemStartupPluginDir(QLibraryInfo::path(QLibraryInfo::PluginsPath) + QDir::separator() + u"appman_startup"_s);
    const QStringList pluginNames = systemStartupPluginDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &pluginName : pluginNames) {
        const QString filePath = systemStartupPluginDir.absoluteFilePath(pluginName);
        if (!QLibrary::isLibrary(filePath))
            continue;
        systemStartupPluginPaths += filePath;
    }

    QStringList startupPluginFiles = variantToStringList(m_configuration.value(u"plugins"_s).toMap().value(u"startup"_s));
    QVector<StartupInterface *> startupPlugins;
    try {
        startupPlugins = loadPlugins<StartupInterface>("startup", systemStartupPluginPaths + startupPluginFiles);
    } catch (const Exception &e) {
        qCCritical(LogQmlRuntime) << qPrintable(e.errorString());
        QCoreApplication::exit(2);
        return;
    }
    for (StartupInterface *iface : std::as_const(startupPlugins))
        iface->initialize(am->systemProperties());

    QVariant pluginPaths = runtimeParameters.value(u"pluginPaths"_s);
    const QVariantList ppvl = (pluginPaths.metaType() == QMetaType::fromType<QString>())
            ? QVariantList{pluginPaths}
            : qdbus_cast<QVariantList>(pluginPaths);

    for (const QVariant &v : ppvl) {
        const QString path = v.toString();
        if (QFileInfo(path).isRelative())
            qApp->addLibraryPath(QDir().absoluteFilePath(path));
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute plugin path in info file for safety reasons:" << path;
    }
    qCDebug(LogQmlRuntime) << " * plugin paths:" << qApp->libraryPaths();

    QVariant imports = runtimeParameters.value(u"importPaths"_s);
    const QVariantList ipvl = (imports.metaType() == QMetaType::fromType<QString>())
            ? QVariantList{imports}
            : qdbus_cast<QVariantList>(imports);

    for (const QVariant &v : ipvl) {
        const QString path = v.toString();
        const QFileInfo fi(path);
        if (!(fi.isNativePath() && fi.isAbsolute()))
            m_engine.addImportPath(toAbsoluteFilePath(path));
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute import path in info file for safety reasons:" << path;
    }
    qCDebug(LogQmlRuntime) << " * Qml import paths:" << m_engine.importPathList();

    for (StartupInterface *iface : std::as_const(startupPlugins))
        iface->beforeQmlEngineLoad(&m_engine);

    StartupTimer::instance()->checkpoint("after loading plugins and import paths");

    // protect our namespace from this point onward
    qmlProtectModule("QtApplicationManager", 1);
    qmlProtectModule("QtApplicationManager.Application", 1);
    qmlProtectModule("QtApplicationManager", 2);
    qmlProtectModule("QtApplicationManager.Application", 2);

    m_engine.load(qmlFileUrl);

    StartupTimer::instance()->checkpoint("after engine loading main qml file");

    auto topLevels = m_engine.rootObjects();

    if (Q_UNLIKELY(topLevels.isEmpty() || !topLevels.at(0))) {
        qCCritical(LogSystem) << "Failed to load component" << qmlFile << ": no root object";
        QCoreApplication::exit(3);
        return;
    }

    for (StartupInterface *iface : std::as_const(startupPlugins))
        iface->afterQmlEngineLoad(&m_engine);

    QObject *topLevel = topLevels.at(0);
    if (auto amw = qobject_cast<ApplicationManagerWindow*>(topLevel)) {
        m_window = qobject_cast<QQuickWindow*>(amw->backingObject());
    } else {
        m_window = qobject_cast<QQuickWindow *>(topLevel);
        if (!m_window) {
            QQuickItem *contentItem = qobject_cast<QQuickItem *>(topLevel);
            if (contentItem) {
                auto* view = new QQuickView(&m_engine, nullptr);
                m_window = view;
                view->setContent(qmlFileUrl, nullptr, topLevel);
                view->setVisible(contentItem->isVisible());
                connect(contentItem, &QQuickItem::visibleChanged, this, [view, contentItem]() {
                    view->setVisible(contentItem->isVisible());
                });
            }
        }
    }

    StartupTimer::instance()->checkpoint("after creating and setting application window");

    // needed, even though we do not explicitly show() the window any more
    for (StartupInterface *iface : std::as_const(startupPlugins))
        iface->beforeWindowShow(m_window);

    qCDebug(LogQmlRuntime) << " * component loading and creating complete.";

    StartupTimer::instance()->checkpoint("component loading and creating complete.");

    if (m_window) {
        Q_ASSERT(m_engine.incubationController());

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

                for (StartupInterface *iface : std::as_const(startupPlugins))
                    iface->afterWindowShow(m_window);
            }
        });
    } else {
        StartupTimer::instance()->createAutomaticReport(applicationId);
    }

    if (!document.isEmpty())
        emit am->openDocument(document, mimeType);
}

bool Controller::eventFilter(QObject *o, QEvent *e)
{
    if (e && (e->type() == QEvent::Show)) {
        if (auto window = qobject_cast<QQuickWindow *>(o)) {
            m_allWindows.append(window);
            updateSlowAnimationsForWindow(window);
        }
    }
    return QObject::eventFilter(o, e);
}

void Controller::updateSlowAnimationsForWindow(QQuickWindow *window)
{
    // QUnifiedTimer are thread-local. To also slow down animations running in the SG thread
    // we need to enable the slow mode in this timer as well.
    auto *connection = new QMetaObject::Connection;
    *connection = connect(window, &QQuickWindow::beforeRendering,
                          this, [connection] {
        if (connection && *connection) {
#if defined(Q_CC_MSVC)
            qApp->disconnect(*connection); // MSVC cannot distinguish between static and non-static overloads in lambdas
#else
            QObject::disconnect(*connection);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
            QUnifiedTimer::instance()->setSlowModeEnabled(ApplicationMain::instance()->slowAnimations());
#else
            QUnifiedTimer::instance()->setSpeedModifier(ApplicationMain::instance()->slowAnimations()
                                                            ? slowAnimationSpeed() : 1.0f);
#endif
            delete connection;
        }
    }, Qt::DirectConnection);
}

void Controller::updateSlowAnimations(bool isSlow)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    QUnifiedTimer::instance()->setSlowModeEnabled(isSlow);
#else
    QUnifiedTimer::instance()->setSpeedModifier(isSlow ? slowAnimationSpeed() : 1.0f);
#endif

    for (auto it = m_allWindows.cbegin(); it != m_allWindows.cend(); ) {
        QPointer<QQuickWindow> window = *it;
        if (!window) {
            m_allWindows.erase(it);
        } else {
            updateSlowAnimationsForWindow(window.data());
            ++it;
        }
    }
}


#include "moc_launcher-qml_p.cpp"
