/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/


#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlIncubationController>

#include <QSocketNotifier>
#include <QFile>
#include <QDir>
#include <QtEndian>
#include <private/qcrashhandler_p.h>
#include <QRegularExpression>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QLoggingCategory>

#include <qplatformdefs.h>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QQuickItem>
#  include <QQuickView>
#  include <QQuickWindow>

#  include "applicationmanagerwindow.h"
#else
#  include <QCoreApplication>
#endif

#include "qmlapplicationinterface.h"
#include "notification.h"
#include "qtyaml.h"
#include "global.h"


// maybe make this configurable for specific workloads?
class HeadlessIncubationController : public QObject, public QQmlIncubationController
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
            QList<QQmlError> errors = comp.errors();
            foreach (const QQmlError &error, errors)
                qWarning() << error;
        }

        if (dummyData) {
            qWarning() << "Loaded dummy data:" << dir.filePath(qml);
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
    Controller();

public slots:
    void startApplication(const QString &qmlFile, const QString &argument, const QVariantMap &runtimeParameters);

private:
    QQmlApplicationEngine m_engine;
    QQmlComponent *m_component;
    QmlApplicationInterface *m_applicationInterface = 0;
    QVariantMap m_configuration;
    bool m_launched = false;
#if !defined(AM_HEADLESS)
    QQuickWindow *m_window = 0;
#endif
};

static void noGdbHandler()
{
    fprintf(stderr, "\n*** qml-launcher (%d) aborting, since no gdb attached ***\n\n", getpid());
    abort();
}

static void crashHandler()
{
    pid_t pid = getpid();
    fprintf(stderr, "\n*** appman-launcher-qml (%d) crashed ***\nthe process will be suspended for 60 seconds and you can attach a debugger to it via\n\n   gdb -p %d\n\n", pid, pid);
    signal(SIGALRM, (sighandler_t) noGdbHandler);
    alarm(60);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigsuspend(&mask);
}

int main(int argc, char *argv[])
{
    QSegfaultHandler::initialize(argv, argc);
    QSegfaultHandler::installCrashHandler(crashHandler);

    qInstallMessageHandler(colorLogToStderr);
    QLoggingCategory::setFilterRules(QString::fromUtf8(qgetenv("AM_LOGGING_RULES")));

#if defined(AM_HEADLESS)
    QCoreApplication a(argc, argv);
#else
#  if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    // this is needed for WebEngine
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#  endif

    QGuiApplication a(argc, argv);

    qmlRegisterType<ApplicationManagerWindow>("io.qt.ApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif

    qmlRegisterType<QmlNotification>("io.qt.ApplicationManager", 1, 0, "Notification");
    qmlRegisterType<QmlApplicationInterfaceExtension>("io.qt.ApplicationManager", 1, 0, "ApplicationInterfaceExtension");

    QByteArray dbusAddress = qgetenv("AM_DBUS_PEER_ADDRESS");
    if (dbusAddress.isEmpty())
        qCCritical(LogQmlRuntime) << "ERROR: $AM_DBUS_PEER_ADDRESS is empty - continuing anyway"; // should be qFatal()
    QDBusConnection dbusConnection = QDBusConnection::connectToPeer(QString::fromUtf8(dbusAddress), qSL("am"));

    if (!dbusConnection.isConnected())
        qCCritical(LogQmlRuntime) << "ERROR: could not connect to the application manager's peer D-Bus at" << dbusAddress; // should be qFatal()

    Controller controller;
    return a.exec();
}

Controller::Controller()
    : m_component(new QQmlComponent(&m_engine))
{
    connect(&m_engine, &QObject::destroyed, &QCoreApplication::quit);
    connect(&m_engine, &QQmlEngine::quit, &QCoreApplication::quit);

    auto docs = QtYaml::variantDocumentsFromYaml(qgetenv("AM_RUNTIME_CONFIGURATION"));
    if (docs.size() == 1)
        m_configuration = docs.first().toMap();

    QVariantMap config;
    auto additionalConfigurations = QtYaml::variantDocumentsFromYaml(qgetenv("AM_RUNTIME_ADDITIONAL_CONFIGURATION"));
    if (additionalConfigurations.size() == 1)
            config = additionalConfigurations.first().toMap();

    //qCDebug(LogQmlRuntime, 1) << " qml-runtime started with pid ==" << QCoreApplication::applicationPid () << ", waiting for qmlFile on stdin...";

    m_applicationInterface = new QmlApplicationInterface(config, qSL("am"), this);
    connect(m_applicationInterface, &QmlApplicationInterface::startApplication,
            this, &Controller::startApplication);
    if (!m_applicationInterface->initialize())
        qCritical("ERROR: could not connect to the application manager's interface on peer D-Bus"); // should be qFatal()
}

void Controller::startApplication(const QString &qmlFile, const QString &argument, const QVariantMap &runtimeParameters)
{
    if (m_launched)
        return;
    m_launched = true;

    qCDebug(LogQmlRuntime) << "loading" << qmlFile << "- start argument:" << argument;

    if (!QFile::exists(qmlFile)) {
        qCCritical(LogQmlRuntime) << "could not load" << qmlFile << ": file does not exist";
        QCoreApplication::exit(2);
        return;
    }

    bool loadDummyData = runtimeParameters.value(qSL("loadDummyData")).toBool()
            || m_configuration.value(qSL("loadDummydata")).toBool();

    if (loadDummyData) {
        qCDebug(LogQmlRuntime) << "loading dummy-data";
        loadDummyDataFiles(m_engine, QFileInfo(qmlFile).path());
    }

    QStringList importPaths = m_configuration.value(qSL("importPaths")).toStringList();
    importPaths.replaceInStrings(QRegularExpression(qSL("^(.*)$")),
                                 QString::fromLocal8Bit(qgetenv("AM_BASE_DIR") + "/\\1"));
    importPaths += runtimeParameters.value(qSL("importPaths")).toStringList();

    for (int i = 0; i < importPaths.size(); ++i)
        importPaths[i] = QDir().absoluteFilePath(importPaths[i]);
    if (!importPaths.isEmpty())
        qCDebug(LogQmlRuntime) << "setting QML2_IMPORT_PATH to" << importPaths;

    m_engine.setImportPathList(m_engine.importPathList() + importPaths);
    //qWarning() << m_engine.importPathList();

    m_engine.rootContext()->setContextProperty(qSL("ApplicationInterface"), m_applicationInterface);

    QUrl qmlFileUrl = QUrl::fromLocalFile(qmlFile);
    m_engine.load(qmlFileUrl);

    QObject *topLevel = m_engine.rootObjects().at(0);

    if (!topLevel) {
        qCCritical(LogSystem) << "could not load" << qmlFile << ": no root object";
        QCoreApplication::exit(3);
        return;
    }

#if !defined(AM_HEADLESS)
    m_window = qobject_cast<QQuickWindow *>(topLevel);
    if (!m_window) {
        QQuickItem *contentItem = qobject_cast<QQuickItem *>(topLevel);
        if (contentItem) {
            QQuickView* view = new QQuickView(&m_engine, 0);
            m_window = view;
            view->setContent(qmlFileUrl, 0, topLevel);
        } else {
            qCCritical(LogSystem) << "could not load" << qmlFile << ": root object is not a QQuickItem.";
            QCoreApplication::exit(4);
            return;
        }
    } else {
        m_engine.setIncubationController(m_window->incubationController());
    }

    Q_ASSERT(m_window);
    QObject::connect(&m_engine, &QQmlEngine::quit, m_window, &QObject::deleteLater); // not sure if this is needed .. or even the best thing to do ... see connects above, they seem to work better

    qWarning() << m_configuration;

    if (m_configuration.contains(qSL("backgroundColor"))) {
        QSurfaceFormat surfaceFormat = m_window->format();
        surfaceFormat.setAlphaBufferSize(8);
        m_window->setFormat(surfaceFormat);
        m_window->setClearBeforeRendering(true);
        qWarning() << "Setting bg color" << m_configuration.value(qSL("backgroundColor")).toString();
        m_window->setColor(QColor(m_configuration.value(qSL("backgroundColor")).toString()));
    }
    m_window->show();

#else
    m_engine.setIncubationController(new HeadlessIncubationController(&m_engine));
#endif
    qCDebug(LogQmlRuntime) << "component loading and creating complete.";

}

#include "main.moc"
