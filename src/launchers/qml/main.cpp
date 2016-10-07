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


#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlIncubationController>

#include <QSocketNotifier>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>
#include <QTimer>
#include <private/qcrashhandler_p.h>
#include <QRegularExpression>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
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
#include "utilities.h"
#include "yamlapplicationscanner.h"
#include "application.h"
#include "startupinterface.h"

AM_BEGIN_NAMESPACE

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
    Controller(QCoreApplication *a, const QString &directLoad = QString());

public slots:
    void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document, const QVariantMap &application);

private:
    QQmlApplicationEngine m_engine;
    QmlApplicationInterface *m_applicationInterface = nullptr;
    QVariantMap m_configuration;
    bool m_launched = false;
#if !defined(AM_HEADLESS)
    QQuickWindow *m_window = nullptr;
#endif
};

AM_END_NAMESPACE

AM_USE_NAMESPACE


int main(int argc, char *argv[])
{
    colorLogApplicationId = "qml-launcher";

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

    qmlRegisterType<ApplicationManagerWindow>("QtApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif

    qmlRegisterType<QmlNotification>("QtApplicationManager", 1, 0, "Notification");
    qmlRegisterType<QmlApplicationInterfaceExtension>("QtApplicationManager", 1, 0, "ApplicationInterfaceExtension");

    if (a.arguments().size() >= 3 && a.arguments().at(1) == "--directload") {
        QFileInfo fi = a.arguments().at(2);

        if (!fi.exists() || fi.fileName() != "info.yaml") {
            qCCritical(LogQmlRuntime) << "ERROR: --directload needs a valid info.yaml file as parameter";
            return 2;
        }

        new Controller(&a, fi.absoluteFilePath());
    } else {
        QByteArray dbusAddress = qgetenv("AM_DBUS_PEER_ADDRESS");
        if (dbusAddress.isEmpty()) {
            qCCritical(LogQmlRuntime) << "ERROR: $AM_DBUS_PEER_ADDRESS is empty";
            return 2;
        }
        QDBusConnection dbusConnection = QDBusConnection::connectToPeer(QString::fromUtf8(dbusAddress), qSL("am"));

        if (!dbusConnection.isConnected()) {
            qCCritical(LogQmlRuntime) << "ERROR: could not connect to the application manager's peer D-Bus at" << dbusAddress;
            return 3;
        }

        new Controller(&a);
    }
    return a.exec();
}

Controller::Controller(QCoreApplication *a, const QString &directLoad)
    : QObject(a)
{
    connect(&m_engine, &QObject::destroyed, &QCoreApplication::quit);
    connect(&m_engine, &QQmlEngine::quit, &QCoreApplication::quit);

    auto docs = QtYaml::variantDocumentsFromYaml(qgetenv("AM_RUNTIME_CONFIGURATION"));
    if (docs.size() == 1)
        m_configuration = docs.first().toMap();

    setCrashActionConfiguration(m_configuration.value(qSL("crashAction")).toMap());

    QVariantMap config;
    auto additionalConfigurations = QtYaml::variantDocumentsFromYaml(qgetenv("AM_RUNTIME_ADDITIONAL_CONFIGURATION"));
    if (additionalConfigurations.size() == 1)
        config = additionalConfigurations.first().toMap();

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

    QString quicklaunchQml = m_configuration.value((qSL("quicklaunchQml"))).toString();
    if (!quicklaunchQml.isEmpty() && a->arguments().contains(qSL("--quicklaunch"))) {
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
        m_applicationInterface = new QmlApplicationInterface(config, qSL("am"), this);
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
                startApplication(fi.absolutePath(), a->codeFilePath(), QString(), a->toVariantMap());
            } catch (const Exception &e) {
                qCritical("ERROR: could not parse info.yaml file: %s", e.what());
                qApp->exit(5);
            }
        });
    }
}

void Controller::startApplication(const QString &baseDir, const QString &qmlFile, const QString &document, const QVariantMap &application)
{
    if (m_launched)
        return;
    m_launched = true;

    QString applicationId = application.value("id").toString();
    QVariantMap runtimeParameters = qdbus_cast<QVariantMap>(application.value("runtimeParameters"));

    qCDebug(LogQmlRuntime) << "loading" << applicationId << "- main:" << qmlFile << "- document:" << document << "- parameters:"
                           << runtimeParameters << "- baseDir:" << baseDir;

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
        colorLogApplicationId = applicationId.toLocal8Bit();
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

    QStringList startupPluginFiles = variantToStringList(m_configuration.value(qSL("plugins")).toMap().value(qSL("startup")));
    auto startupPlugins = loadPlugins<StartupInterface>("startup", startupPluginFiles);
    foreach (StartupInterface *iface, startupPlugins)
        iface->initialize(m_applicationInterface->additionalConfiguration());

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
        QString path = v.toString();
        if (QFileInfo(path).isRelative())
            path = QDir().absoluteFilePath(path);
        else
            qCWarning(LogQmlRuntime) << "Omitting absolute import path in info file for safety reasons:" << path;
        m_engine.addImportPath(path);
    }
    qCDebug(LogQmlRuntime) << "Qml import paths:" << m_engine.importPathList();

    if (m_applicationInterface)
        m_engine.rootContext()->setContextProperty(qSL("ApplicationInterface"), m_applicationInterface);

    foreach (StartupInterface *iface, startupPlugins)
        iface->beforeQmlEngineLoad(&m_engine);

    QUrl qmlFileUrl = QUrl::fromLocalFile(qmlFile);
    m_engine.load(qmlFileUrl);

    auto topLevels = m_engine.rootObjects();

    if (Q_UNLIKELY(topLevels.isEmpty() || !topLevels.at(0))) {
        qCCritical(LogSystem) << "could not load" << qmlFile << ": no root object";
        QCoreApplication::exit(3);
        return;
    }

    foreach (StartupInterface *iface, startupPlugins)
        iface->afterQmlEngineLoad(&m_engine);

    QObject *topLevel = topLevels.at(0);

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
        if (!m_engine.incubationController())
            m_engine.setIncubationController(m_window->incubationController());
    }

    Q_ASSERT(m_window);
    QObject::connect(&m_engine, &QQmlEngine::quit, m_window, &QObject::deleteLater); // not sure if this is needed .. or even the best thing to do ... see connects above, they seem to work better

    if (m_configuration.contains(qSL("backgroundColor"))) {
        QSurfaceFormat surfaceFormat = m_window->format();
        surfaceFormat.setAlphaBufferSize(8);
        m_window->setFormat(surfaceFormat);
        m_window->setClearBeforeRendering(true);
        m_window->setColor(QColor(m_configuration.value(qSL("backgroundColor")).toString()));
    }

    foreach (StartupInterface *iface, startupPlugins)
        iface->beforeWindowShow(m_window);

    m_window->show();

    foreach (StartupInterface *iface, startupPlugins)
        iface->afterWindowShow(m_window);

#else
    m_engine.setIncubationController(new HeadlessIncubationController(&m_engine));
#endif
    qCDebug(LogQmlRuntime) << "component loading and creating complete.";

    if (!document.isEmpty() && m_applicationInterface)
        m_applicationInterface->openDocument(document);
}

#include "main.moc"
