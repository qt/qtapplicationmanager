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

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QCoreApplication>

#if !defined(AM_HEADLESS)
#  include <QQuickView>

#  include "fakeapplicationmanagerwindow.h"
#endif

#include "application.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"
#include "abstractcontainer.h"
#include "global.h"
#include "utilities.h"
#include "runtimefactory.h"

QT_BEGIN_NAMESPACE_AM

// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory + qSL("/dummydata"), qSL("*.qml"));
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QFile f(dir.filePath(qml));
        f.open(QIODevice::ReadOnly);
        QByteArray data = f.readAll();
        QQmlComponent comp(&engine);
        comp.setData(data, QUrl());
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


QmlInProcessRuntime::QmlInProcessRuntime(const Application *app, QmlInProcessRuntimeManager *manager)
    : AbstractRuntime(nullptr, app, manager)
{ }

QmlInProcessRuntime::~QmlInProcessRuntime()
{
#if !defined(AM_HEADLESS)
    // if there is still a window present at this point, fire the 'closing' signal (probably) again,
    // because it's still the duty of WindowManager together with qml-ui to free and delete this item!!
    for (int i = m_windows.size(); i; --i)
        emit inProcessSurfaceItemClosing(m_windows.at(i-1));
#endif
}

bool QmlInProcessRuntime::start()
{
    setState(Startup);
#if !defined(AM_HEADLESS)
    if (m_mainWindow) {                                 // if there is already a window present, just emit ready signal and return true (=="start successful")
        emit inProcessSurfaceItemReady(m_mainWindow);
        return true;
    }
#endif

    if (!m_inProcessQmlEngine)
        return false;

    if (!m_app) {
        qCCritical(LogSystem) << "tried to start without an app object";
        return false;
    }

    if (m_app->runtimeParameters().value(qSL("loadDummyData")).toBool()) {
        qCDebug(LogSystem) << "Loading dummy-data";
        loadDummyDataFiles(*m_inProcessQmlEngine, QFileInfo(m_app->absoluteCodeFilePath()).path());
    }

    const QStringList importPaths = variantToStringList(configuration().value(qSL("importPaths")))
                                  + variantToStringList(m_app->runtimeParameters().value(qSL("importPaths")));
    if (!importPaths.isEmpty()) {
        const QString baseDir = m_app->baseDir().absolutePath() + QDir::separator();
        for (const QString &path : importPaths)
            m_inProcessQmlEngine->addImportPath(QFileInfo(path).isRelative() ? baseDir + path : path);

        qCDebug(LogSystem) << "Updated Qml import paths:" << m_inProcessQmlEngine->importPathList();
    }

    QQmlComponent component(m_inProcessQmlEngine, m_app->absoluteCodeFilePath());

    if (!component.isReady()) {
        qCDebug(LogSystem) << "qml-file (" << m_app->absoluteCodeFilePath() << "): component not ready:\n" << component.errorString();
        return false;
    }

    // We are running each application in it's own, separate Qml context.
    // This way, we can export an unique ApplicationInterface object for each app
    QQmlContext *appContext = new QQmlContext(m_inProcessQmlEngine->rootContext());
    m_applicationIf = new QmlInProcessApplicationInterface(this);
    appContext->setContextProperty(qSL("ApplicationInterface"), m_applicationIf);

    QObject *obj = component.beginCreate(appContext);

    if (!obj) {
        qCCritical(LogSystem) << "could not load" << m_app->absoluteCodeFilePath() << ": no root object";
        delete obj;
        delete appContext;
        delete m_applicationIf;
        m_applicationIf = 0;
        return false;
    }

#if !defined(AM_HEADLESS)
    FakeApplicationManagerWindow *window = qobject_cast<FakeApplicationManagerWindow*>(obj);

    if (!window) {
        qCCritical(LogSystem) << "could not load" << m_app->absoluteCodeFilePath() << ": root object is not a ApplicationManagerWindow.";
        delete obj;
        delete appContext;
        delete m_applicationIf;
        m_applicationIf = 0;
        return false;
    }
    window->m_runtime = this;
    m_mainWindow = window;
#endif

    component.completeCreate();
    if (!m_document.isEmpty())
        emit openDocument(m_document);

    setState(Active);
    return true;
}

void QmlInProcessRuntime::stop(bool forceKill)
{
    Q_UNUSED(forceKill)   // ignore forceKill: for in-process there is nothing to 'kill'

    emit stateChanged(Shutdown);
    emit aboutToStop();

#if !defined(AM_HEADLESS)
    for (int i = m_windows.size(); i; --i)
        emit inProcessSurfaceItemClosing(m_windows.at(i-1));
    m_windows.clear();
    m_mainWindow = 0;
#endif
    setState(Inactive);

    deleteLater();
}

#if !defined(AM_HEADLESS)

void QmlInProcessRuntime::onWindowClose()
{
    QQuickItem* window = reinterpret_cast<QQuickItem*>(sender()); // reinterpret_cast because the object might be broken down already!
    Q_ASSERT(window && m_windows.contains(window));

    emit inProcessSurfaceItemClosing(window);
}

void QmlInProcessRuntime::onWindowDestroyed()
{
    QQuickItem* window = reinterpret_cast<QQuickItem*>(sender()); // reinterpret_cast because the object might be broken down already!
    m_windows.removeAll(window);
    if (m_mainWindow == window)
        m_mainWindow = 0;
}

void QmlInProcessRuntime::onEnableFullscreen()
{
    FakeApplicationManagerWindow *window = qobject_cast<FakeApplicationManagerWindow *>(sender());

    emit inProcessSurfaceItemFullscreenChanging(window, true);
}

void QmlInProcessRuntime::onDisableFullscreen()
{
    FakeApplicationManagerWindow *window = qobject_cast<FakeApplicationManagerWindow *>(sender());

    emit inProcessSurfaceItemFullscreenChanging(window, false);
}

void QmlInProcessRuntime::addWindow(QQuickItem *window)
{
    if (m_windows.indexOf(window) == -1) {
        m_windows.append(window);

        if (auto pcw = qobject_cast<FakeApplicationManagerWindow *>(window)) {
            connect(pcw, &FakeApplicationManagerWindow::fakeFullScreenSignal, this, &QmlInProcessRuntime::onEnableFullscreen);
            connect(pcw, &FakeApplicationManagerWindow::fakeNoFullScreenSignal, this, &QmlInProcessRuntime::onDisableFullscreen);
            connect(pcw, &FakeApplicationManagerWindow::fakeCloseSignal, this, &QmlInProcessRuntime::onWindowClose);
            connect(pcw, &QObject::destroyed, this, &QmlInProcessRuntime::onWindowDestroyed);
        }
    }

    emit inProcessSurfaceItemReady(window);
}

#endif // !AM_HEADLESS

void QmlInProcessRuntime::openDocument(const QString &document)
{
    m_document = document;
    if (m_applicationIf)
        m_applicationIf->openDocument(document);
}

qint64 QmlInProcessRuntime::applicationProcessId() const
{
    return QCoreApplication::applicationPid();
}


QmlInProcessRuntimeManager::QmlInProcessRuntimeManager(QObject *parent)
    : AbstractRuntimeManager(defaultIdentifier(), parent)
{ }

QmlInProcessRuntimeManager::QmlInProcessRuntimeManager(const QString &id, QObject *parent)
    : AbstractRuntimeManager(id, parent)
{ }

QString QmlInProcessRuntimeManager::defaultIdentifier()
{
    return qSL("qml-inprocess");
}

bool QmlInProcessRuntimeManager::inProcess() const
{
    return true;
}

AbstractRuntime *QmlInProcessRuntimeManager::create(AbstractContainer *container, const Application *app)
{
    if (container) {
        delete container;
        return nullptr;
    }
    return new QmlInProcessRuntime(app, this);
}

QT_END_NAMESPACE_AM
