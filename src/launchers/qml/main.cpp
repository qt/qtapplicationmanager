/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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


#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlIncubationController>
#include <QQmlDebuggingEnabler>

#include <QSocketNotifier>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>
#include <QTimer>
#include <QRegularExpression>
#include <QCommandLineParser>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QLoggingCategory>

#include <QtCore/private/qcoreapplication_p.h>
#include <qplatformdefs.h>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QQuickItem>
#  include <QQuickView>
#  include <QQuickWindow>

#  include <QtAppManLauncher/private/applicationmanagerwindow_p.h>
#else
#  include <QCoreApplication>
#endif

#include "qmlapplicationinterface.h"
#include "qmlapplicationinterfaceextension.h"
#include "qmlnotification.h"
#include "notification.h"
#include "qtyaml.h"
#include "global.h"
#include "logging.h"
#include "utilities.h"
#include "crashhandler.h"
#include "yamlapplicationscanner.h"
#include "application.h"
#include "startupinterface.h"
#include "dbus-utilities.h"
#include "startuptimer.h"
#include "processtitle.h"

QT_BEGIN_NAMESPACE_AM

// maybe make this configurable for specific workloads?
class HeadlessIncubationController : public QObject, public QQmlIncubationController // clazy:exclude=missing-qobject-macro
{
public:
    HeadlessIncubationController(QObject *parent)
        : QObject(parent)
    {
        startTimer(50);
    }

protected:
    void timerEvent(QTimerEvent *) override
    {
        incubateFor(25);
    }
};



// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory + qSL("/dummydata"), qSL("*.qml"));
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(&engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if (comp.isError()) {
            const QList<QQmlError> errors = comp.errors();
            for (const QQmlError &error : errors)
                qWarning() << error;
        }

        if (dummyData) {
            qml.truncate(qml.length()-4);
            engine.rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(&engine);
        }
    }
}

class Controller : public QObject
{
    Q_OBJECT

public:
    Controller(QCoreApplication *a, bool quickLaunched, const QString &directLoad = QString());

public slots:
    void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                          const QString &mimeType, const QVariantMap &application, const QVariantMap systemProperties);

private:
    QQmlApplicationEngine m_engine;
    QmlApplicationInterface *m_applicationInterface = nullptr;
    QVariantMap m_configuration;
    bool m_launched = false;
    bool m_quickLaunched;
#if !defined(AM_HEADLESS)
    QQuickWindow *m_window = nullptr;
#endif
};

static QString p2pBusName = qSL("am");
static QString notificationBusName = qSL("am_notification_bus");
static QCommandLineParser cp;
static QCommandLineOption qmlDebugOption(qSL("qml-debug"), qSL("Enables QML debugging and profiling."));
static QCommandLineOption quickLaunchOption(qSL("quicklaunch"), qSL("Starts the launcher in the quicklaunching mode."));
static QCommandLineOption directLoadOption(qSL("directload") , qSL("The info.yaml to start."), qSL("info.yaml"));


QT_END_NAMESPACE_AM

QT_USE_NAMESPACE_AM


int main(int argc, char *argv[])
{
    StartupTimer::instance()->checkpoint("entered main");

    if (qEnvironmentVariableIsSet("AM_NO_DLT_LOGGING"))
        Logging::setDltEnabled(false);

    // The common-lib is already registering the DLT Application for the application manager.
    // As the appID needs to be unique within the system, we cannot use the same appID and
    // need to change it as early as possible.

    // As we don't know the app-id yet, we are registering a place holder so we are able to see
    // something in the dlt logs if general errors occur.
    Logging::setDltApplicationId("PCLQ", "Pelagicore Application-Manager Launcher QML");
    Logging::setApplicationId("qml-launcher");
    Logging::initialize();

    QLoggingCategory::setFilterRules(QString::fromUtf8(qgetenv("AM_LOGGING_RULES")));

#if defined(AM_HEADLESS)
    QCoreApplication a(argc, argv);
#else
    // this is needed for WebEngine
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QGuiApplication a(argc, argv);
    a.setApplicationDisplayName(qL1S("appman-launcher-qml"));
    cp.addHelpOption();
    cp.addOption(directLoadOption);
    cp.addOption(qmlDebugOption);
    cp.addOption(quickLaunchOption);

    cp.process(a);

    if (!static_cast<QCoreApplicationPrivate *>(QObjectPrivate::get(&a))->qmljsDebugArgumentsString().isEmpty()
            || cp.isSet(qmlDebugOption)) {
#if !defined(QT_NO_QML_DEBUGGER)
        (void) new QQmlDebuggingEnabler(true);
        if (!QLoggingCategory::defaultCategory()->isDebugEnabled()) {
            qCCritical(LogQmlRuntime) << "The default 'debug' logging category was disabled. "
                                         "Re-enabling it for the QML Debugger interface to work correctly.";
            QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
        }
#else
        qCWarning(LogQmlRuntime) << "The --qml-debug/-qmljsdebugger options are ignored, because Qt was built without support for QML Debugging!";
#endif
    }

    qmlRegisterType<ApplicationManagerWindow>("QtApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif

    qmlRegisterType<QmlNotification>("QtApplicationManager", 1, 0, "Notification");
    qmlRegisterType<QmlApplicationInterfaceExtension>("QtApplicationManager", 1, 0, "ApplicationInterfaceExtension");

    StartupTimer::instance()->checkpoint("after logging and qml register initialization");

    if (cp.isSet(directLoadOption)) {
        QFileInfo fi = cp.value(directLoadOption);

        if (!fi.exists() || fi.fileName() != qSL("info.yaml")) {
            qCCritical(LogQmlRuntime) << "ERROR: --directload needs a valid info.yaml file as parameter";
            return 2;
        }

        new Controller(&a, cp.isSet(quickLaunchOption), fi.absoluteFilePath());
    } else {
        QByteArray dbusAddress = qgetenv("AM_DBUS_PEER_ADDRESS");
        if (dbusAddress.isEmpty()) {
            qCCritical(LogQmlRuntime) << "ERROR: $AM_DBUS_PEER_ADDRESS is empty";
            return 2;
        }
        QDBusConnection dbusConnection = QDBusConnection::connectToPeer(QString::fromUtf8(dbusAddress), p2pBusName);

        if (!dbusConnection.isConnected()) {
            qCCritical(LogQmlRuntime) << "ERROR: could not connect to the P2P D-Bus via:" << dbusAddress;
            return 3;
        }
        qCDebug(LogQmlRuntime) << "Connected to the P2P D-Bus via:" << dbusAddress;

        dbusAddress = qgetenv("AM_DBUS_NOTIFICATION_BUS_ADDRESS");
        if (dbusAddress.isEmpty())
            dbusAddress = "session";

        if (dbusAddress == "system")
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SystemBus, notificationBusName);
        else if (dbusAddress == "session")
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, notificationBusName);
        else
            dbusConnection = QDBusConnection::connectToBus(QString::fromUtf8(dbusAddress), notificationBusName);

        if (!dbusConnection.isConnected()) {
            qCCritical(LogQmlRuntime) << "ERROR: could not connect to the Notification D-Bus via:" << dbusAddress;
            return 3;
        }
        qCDebug(LogQmlRuntime) << "Connected to the Notification D-Bus via:" << dbusAddress;

        StartupTimer::instance()->checkpoint("after dbus initialization");

        new Controller(&a, cp.isSet(quickLaunchOption));
    }

    return a.exec();
}

Controller::Controller(QCoreApplication *a, bool quickLaunched, const QString &directLoad)
    : QObject(a)
    , m_quickLaunched(quickLaunched)
{
    connect(&m_engine, &QObject::destroyed, &QCoreApplication::quit);
    connect(&m_engine, &QQmlEngine::quit, &QCoreApplication::quit);

    auto docs = QtYaml::variantDocumentsFromYaml(qgetenv("AM_RUNTIME_CONFIGURATION"));
    if (docs.size() == 1)
        m_configuration = docs.first().toMap();

    CrashHandler::setCrashActionConfiguration(m_configuration.value(qSL("crashAction")).toMap());

    const QString baseDir = QString::fromLocal8Bit(qgetenv("AM_BASE_DIR") + "/");

    QStringList importPaths = variantToStringList(m_configuration.value(qSL("importPaths")));
    for (QString &path : importPaths) {
        if (QFileInfo(path).isRelative()) {
            path.prepend(baseDir);
        } else {
            qCWarning(LogQmlRuntime) << "Absolute import path in config file can lead to problems inside containers:"
                                     << path;
        }
        m_engine.addImportPath(path);
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
        registerWindowComp.completeCreate();
    }

    StartupTimer::instance()->checkpoint("after window registration");

    QString quicklaunchQml = m_configuration.value((qSL("quicklaunchQml"))).toString();
    if (!quicklaunchQml.isEmpty() && cp.isSet(quickLaunchOption)) {
        if (QFileInfo(quicklaunchQml).isRelative())
            quicklaunchQml.prepend(baseDir);

        QQmlComponent quicklaunchComp(&m_engine, quicklaunchQml);
        if (!quicklaunchComp.isError()) {
            QScopedPointer<QObject> quicklaunchInstance(quicklaunchComp.create());
            quicklaunchComp.completeCreate();
        } else {
            const QList<QQmlError> errors = quicklaunchComp.errors();
            for (const QQmlError &error : errors)
                qCCritical(LogQmlRuntime) << error;
        }
    }

    if (directLoad.isEmpty()) {
        m_applicationInterface = new QmlApplicationInterface(p2pBusName, notificationBusName, this);
        connect(m_applicationInterface, &QmlApplicationInterface::startApplication,
                this, &Controller::startApplication);
        if (!m_applicationInterface->initialize()) {
            qCritical("ERROR: could not connect to the application manager's interface on the peer D-Bus");
            qApp->exit(4);
        }
    } else {
        QTimer::singleShot(0, [this, directLoad]() {
            QFileInfo fi(directLoad);
            YamlApplicationScanner yas;
            try {
                const Application *a = yas.scan(directLoad);
                startApplication(fi.absolutePath(), a->codeFilePath(), QString(), QString(), a->toVariantMap(), QVariantMap());
            } catch (const Exception &e) {
                qCritical("ERROR: could not parse info.yaml file: %s", e.what());
                qApp->exit(5);
            }
        });
    }

    StartupTimer::instance()->checkpoint("after application interface initialization");
}

void Controller::startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                                  const QString &mimeType, const QVariantMap &application,
                                  const QVariantMap systemProperties)
{
    if (m_launched)
        return;
    m_launched = true;

    QString applicationId = application.value(qSL("id")).toString();

    if (m_quickLaunched) {
        //StartupTimer::instance()->createReport(applicationId  + qSL(" [process launch]"));
        StartupTimer::instance()->reset();
    } else {
        StartupTimer::instance()->checkpoint("starting application");
    }

    QVariantMap runtimeParameters = qdbus_cast<QVariantMap>(application.value(qSL("runtimeParameters")));

    //Change the DLT Application description, to easily identify the application on the DLT logs.
    char dltAppId[5];
    qsnprintf(dltAppId, 5, "A%03d", application.value(qSL("uniqueNumber")).toInt());
    Logging::setDltApplicationId(dltAppId, QByteArray("Application-Manager App: ") + applicationId.toLocal8Bit());
    Logging::registerUnregisteredDltContexts();

    // Dress up the ps output to make it easier to correlate all the launcher processes
    {
        QByteArray id = applicationId.toLocal8Bit();
        QByteArray args;
        if (qApp->arguments().size() > 1)
            args = qApp->arguments().mid(1).join(qL1C(' ')).toLocal8Bit();
        ProcessTitle::setTitle("%s%s%s", id.constData(), args.isEmpty() ? "" : " ", args.constData());
    }

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

    if (!QFile::exists(qmlFile)) {
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
    }

    QStringList startupPluginFiles = variantToStringList(m_configuration.value(qSL("plugins")).toMap().value(qSL("startup")));
    auto startupPlugins = loadPlugins<StartupInterface>("startup", startupPluginFiles);
    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->initialize(m_applicationInterface ? m_applicationInterface->systemProperties() : QVariantMap());

    bool loadDummyData = runtimeParameters.value(qSL("loadDummyData")).toBool()
            || m_configuration.value(qSL("loadDummydata")).toBool();

    if (loadDummyData) {
        qCDebug(LogQmlRuntime) << "loading dummy-data";
        loadDummyDataFiles(m_engine, QFileInfo(qmlFile).path());
    }

    QVariant imports = runtimeParameters.value(qSL("importPaths"));
    const QVariantList vl = (imports.type() == QVariant::String) ? QVariantList{imports}
                                                                 : qdbus_cast<QVariantList>(imports);
    for (const QVariant &v : vl) {
        const QString path = v.toString();
        if (QFileInfo(path).isRelative())
            m_engine.addImportPath(QDir().absoluteFilePath(path));
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute import path in info file for safety reasons:" << path;
    }
    qCDebug(LogQmlRuntime) << "Qml import paths:" << m_engine.importPathList();

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->beforeQmlEngineLoad(&m_engine);

    StartupTimer::instance()->checkpoint("after loading plugins and import paths");

    QUrl qmlFileUrl = QUrl::fromLocalFile(qmlFile);
    m_engine.rootContext()->setContextProperty(qSL("StartupTimer"), StartupTimer::instance());
    m_engine.load(qmlFileUrl);

    StartupTimer::instance()->checkpoint("after engine loading main qml file");

    auto topLevels = m_engine.rootObjects();

    if (Q_UNLIKELY(topLevels.isEmpty() || !topLevels.at(0))) {
        qCCritical(LogSystem) << "could not load" << qmlFile << ": no root object";
        QCoreApplication::exit(3);
        return;
    }

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->afterQmlEngineLoad(&m_engine);

    QObject *topLevel = topLevels.at(0);

#if !defined(AM_HEADLESS)
    m_window = qobject_cast<QQuickWindow *>(topLevel);
    if (!m_window) {
        QQuickItem *contentItem = qobject_cast<QQuickItem *>(topLevel);
        if (contentItem) {
            QQuickView* view = new QQuickView(&m_engine, nullptr);
            m_window = view;
            view->setContent(qmlFileUrl, nullptr, topLevel);
        } else {
            qCCritical(LogSystem) << "could not load" << qmlFile << ": root object is not a QQuickItem.";
            QCoreApplication::exit(4);
            return;
        }
    } else {
        if (!m_engine.incubationController())
            m_engine.setIncubationController(m_window->incubationController());
    }

    StartupTimer::instance()->checkpoint("after creating and setting application window");

    Q_ASSERT(m_window);
    QObject::connect(&m_engine, &QQmlEngine::quit, m_window, &QObject::deleteLater); // not sure if this is needed .. or even the best thing to do ... see connects above, they seem to work better

    if (m_configuration.contains(qSL("backgroundColor"))) {
        QSurfaceFormat surfaceFormat = m_window->format();
        surfaceFormat.setAlphaBufferSize(8);
        m_window->setFormat(surfaceFormat);
        m_window->setClearBeforeRendering(true);
        m_window->setColor(QColor(m_configuration.value(qSL("backgroundColor")).toString()));
    }

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->beforeWindowShow(m_window);

    m_window->show();

    StartupTimer::instance()->checkpoint("after showing application window");

    for (StartupInterface *iface : qAsConst(startupPlugins))
        iface->afterWindowShow(m_window);

#else
    m_engine.setIncubationController(new HeadlessIncubationController(&m_engine));
#endif
    qCDebug(LogQmlRuntime) << "component loading and creating complete.";

    StartupTimer::instance()->checkpoint("component loading and creating complete.");
    StartupTimer::instance()->createReport(application.value(qSL("id")).toString());

    if (!document.isEmpty() && m_applicationInterface)
        emit m_applicationInterface->openDocument(document, mimeType);
}

#include "main.moc"
