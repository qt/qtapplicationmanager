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

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QCoreApplication>
#include <QTimer>

#if !defined(AM_HEADLESS)
#  include <QQuickView>

#  include "fakeapplicationmanagerwindow.h"
#  include "inprocesssurfaceitem.h"
#endif

#include "logging.h"
#include "application.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"
#include "abstractcontainer.h"
#include "global.h"
#include "utilities.h"
#include "runtimefactory.h"
#include "qml-utilities.h"

#if defined(Q_OS_UNIX)
#  include <signal.h>
#endif

QT_BEGIN_NAMESPACE_AM


const char *QmlInProcessRuntime::s_runtimeKey = "_am_runtime";


QmlInProcessRuntime::QmlInProcessRuntime(Application *app, QmlInProcessRuntimeManager *manager)
    : AbstractRuntime(nullptr, app, manager)
{ }

QmlInProcessRuntime::~QmlInProcessRuntime()
{
#if !defined(AM_HEADLESS)
    // if there is still a window present at this point, fire the 'closing' signal (probably) again,
    // because it's still the duty of WindowManager together with qml-ui to free and delete this item!!
    for (int i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);
#endif
}

bool QmlInProcessRuntime::start()
{
#if !defined(AM_HEADLESS)
    Q_ASSERT(!m_rootObject);
#endif
    setState(Am::StartingUp);

    if (!m_inProcessQmlEngine)
        return false;

    if (!m_app) {
        qCCritical(LogSystem) << "tried to start without an app object";
        return false;
    }

    if (m_app->runtimeParameters().value(qSL("loadDummyData")).toBool()) {
        qCDebug(LogSystem) << "Loading dummy-data";
        loadQmlDummyDataFiles(m_inProcessQmlEngine, QFileInfo(m_app->nonAliasedInfo()->absoluteCodeFilePath()).path());
    }

    const QStringList importPaths = variantToStringList(configuration().value(qSL("importPaths")))
                                  + variantToStringList(m_app->runtimeParameters().value(qSL("importPaths")));
    if (!importPaths.isEmpty()) {
        const QString codeDir = m_app->codeDir() + QDir::separator();
        for (const QString &path : importPaths)
            m_inProcessQmlEngine->addImportPath(QFileInfo(path).isRelative() ? codeDir + path : path);

        qCDebug(LogSystem) << "Updated Qml import paths:" << m_inProcessQmlEngine->importPathList();
    }

    m_componentError = false;
    QQmlComponent *component = new QQmlComponent(m_inProcessQmlEngine, m_app->nonAliasedInfo()->absoluteCodeFilePath());

    if (!component->isReady()) {
        qCDebug(LogSystem) << "qml-file (" << m_app->nonAliasedInfo()->absoluteCodeFilePath() << "): component not ready:\n" << component->errorString();
        return false;
    }

    // We are running each application in it's own, separate Qml context.
    // This way, we can export an unique ApplicationInterface object for each app
    QQmlContext *appContext = new QQmlContext(m_inProcessQmlEngine->rootContext());
    m_applicationIf = new QmlInProcessApplicationInterface(this);
    appContext->setContextProperty(qSL("ApplicationInterface"), m_applicationIf);
    connect(m_applicationIf, &QmlInProcessApplicationInterface::quitAcknowledged,
            this, [this]() { finish(0, Am::NormalExit); });

    if (appContext->setProperty(s_runtimeKey, QVariant::fromValue(this)))
        qCritical() << "Could not set" << s_runtimeKey << "property in QML context";

    QObject *obj = component->beginCreate(appContext);

    QTimer::singleShot(0, this, [component, appContext, obj, this]() {
        component->completeCreate();
        if (!obj || m_componentError) {
            qCCritical(LogSystem) << "could not load" << m_app->nonAliasedInfo()->absoluteCodeFilePath() << ": no root object";
            delete obj;
            delete appContext;
            delete m_applicationIf;
            m_applicationIf = nullptr;
            finish(3, Am::NormalExit);
        } else {
#if !defined(AM_HEADLESS)
            if (!qobject_cast<FakeApplicationManagerWindow*>(obj)) {
                QQuickItem *item = qobject_cast<QQuickItem*>(obj);
                if (item) {
                    auto surfaceItem = new InProcessSurfaceItem;
                    item->setParentItem(surfaceItem);
                    addSurfaceItem(QSharedPointer<InProcessSurfaceItem>(surfaceItem));
                }
            }
            m_rootObject = obj;
#endif
            if (!m_document.isEmpty())
                openDocument(m_document, QString());
            setState(Am::Running);
        }
        delete component;
    });
    return true;
}

void QmlInProcessRuntime::stop(bool forceKill)
{
    setState(Am::ShuttingDown);
    emit aboutToStop();

#if !defined(AM_HEADLESS)
    for (int i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);

    if (m_surfaces.isEmpty()) {
        delete m_rootObject;
        m_rootObject = nullptr;
    }
#endif

    if (forceKill) {
#if defined(Q_OS_UNIX)
        int exitCode = SIGKILL;
#else
        int exitCode = 0;
#endif
        finish(exitCode, Am::CrashExit);
        return;
    }

    bool ok;
    int qt = configuration().value(qSL("quitTime")).toInt(&ok);
    if (!ok || qt < 0)
        qt = 250;
    QTimer::singleShot(qt, this, [this]() {
#if defined(Q_OS_UNIX)
        int exitCode = SIGTERM;
#else
        int exitCode = 0;
#endif
        finish(exitCode, Am::CrashExit);
    });
}

void QmlInProcessRuntime::finish(int exitCode, Am::ExitStatus status)
{
    QTimer::singleShot(0, this, [this, exitCode, status]() {
        qCDebug(LogSystem) << "QmlInProcessRuntime (id:" << (m_app ? m_app->id() : qSL("(none)"))
                           << ") exited with code:" << exitCode << "status:" << status;
        emit finished(exitCode, status);
        setState(Am::NotRunning);
#if !defined(AM_HEADLESS)
        if (m_surfaces.isEmpty())
            deleteLater();
#else
        deleteLater();
#endif
    });
}

#if !defined(AM_HEADLESS)

void QmlInProcessRuntime::stopIfNoVisibleSurfaces()
{
    if (!hasVisibleSurfaces())
        stop();
    else
        m_stopIfNoVisibleSurfaces = true;
}

bool QmlInProcessRuntime::hasVisibleSurfaces() const
{
    for (int i = 0; i < m_surfaces.count(); ++i) {
        if (m_surfaces[i]->visibleClientSide())
            return true;
    }
    return false;
}

void QmlInProcessRuntime::onSurfaceItemReleased(InProcessSurfaceItem *surface)
{
    for (int i = 0; i < m_surfaces.count(); ++i) {
        if (surface == m_surfaces[i].data()) {
            m_surfaces.removeAt(i);
            disconnect(surface, nullptr, this, nullptr);
            break;
        }
    }

    if (state() != Am::Running && m_surfaces.isEmpty()) {
        delete m_rootObject;
        m_rootObject = nullptr;
        if (state() == Am::NotRunning)
            deleteLater();
    }
}

void QmlInProcessRuntime::addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface)
{
    // Below check is only needed if the root element is a QtObject.
    // It should be possible to remove this, once proper visible handling is in place.
    if (state() != Am::NotRunning && state() != Am::ShuttingDown) {
        m_stopIfNoVisibleSurfaces = false; // the appearance of a new surface disarms that death trigger
        if (!m_surfaces.contains(surface)) {
            m_surfaces.append(surface);
            InProcessSurfaceItem *surfacePtr = surface.data();
            connect(surfacePtr, &InProcessSurfaceItem::released, this, [this, surfacePtr]() {
                onSurfaceItemReleased(surfacePtr);
            });
            connect(surfacePtr, &InProcessSurfaceItem::visibleClientSideChanged, this, [this, surfacePtr]() {
                if (m_stopIfNoVisibleSurfaces && !hasVisibleSurfaces())
                    stop();
            });
        }
        emit inProcessSurfaceItemReady(surface);
    }
}

#endif // !AM_HEADLESS

void QmlInProcessRuntime::openDocument(const QString &document, const QString &mimeType)
{
    m_document = document;
    if (m_applicationIf)
        emit m_applicationIf->openDocument(document, mimeType);
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

AbstractRuntime *QmlInProcessRuntimeManager::create(AbstractContainer *container, Application *app)
{
    if (container) {
        delete container;
        return nullptr;
    }
    return new QmlInProcessRuntime(app, this);
}

QT_END_NAMESPACE_AM
