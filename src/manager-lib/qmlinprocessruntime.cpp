/****************************************************************************
**
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

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <private/qv4engine_p.h>
#include <private/qqmlcontext_p.h>

#if !defined(AM_HEADLESS)
#  include <QQuickView>

#  include "qmlinprocessapplicationmanagerwindow.h"
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

    const QString codeDir = m_app->codeDir() + QDir::separator();

    const QStringList resources = variantToStringList(configuration().value(qSL("resources")))
                                  + variantToStringList(m_app->runtimeParameters().value(qSL("resources")));
    for (const QString &resource : resources) {
        const QString path = QFileInfo(resource).isRelative() ? codeDir + resource : resource;
        static QStringList cache;
        if (!cache.contains(path)) {
            if (!loadResource(path))
                qCWarning(LogQmlRuntime) << "Cannot register resource:" << path;
            cache.append(path);
        }
    }

    if (m_app->runtimeParameters().value(qSL("loadDummyData")).toBool()) {
        qCDebug(LogSystem) << "Loading dummy-data";
        loadQmlDummyDataFiles(m_inProcessQmlEngine, QFileInfo(m_app->info()->absoluteCodeFilePath()).path());
    }

    const QStringList pluginPaths = variantToStringList(configuration().value(qSL("pluginPaths")))
                                  + variantToStringList(m_app->runtimeParameters().value(qSL("pluginPaths")));

    if (!pluginPaths.isEmpty()) {
        const QString codeDir = m_app->codeDir() + QDir::separator();
        for (const QString &path : pluginPaths)
            qApp->addLibraryPath(QFileInfo(path).isRelative() ? codeDir + path : path);

        qCDebug(LogSystem) << "Updated plugin paths:" << qApp->libraryPaths();
    }

    const QStringList importPaths = variantToStringList(configuration().value(qSL("importPaths")))
                                  + variantToStringList(m_app->runtimeParameters().value(qSL("importPaths")));
    if (!importPaths.isEmpty()) {
        for (const QString &path : importPaths)
            m_inProcessQmlEngine->addImportPath(toAbsoluteFilePath(path, codeDir));

        qCDebug(LogSystem) << "Updated Qml import paths:" << m_inProcessQmlEngine->importPathList();
    }

    const QUrl qmlFileUrl = filePathToUrl(m_app->info()->absoluteCodeFilePath(), codeDir);
    QQmlComponent *component = new QQmlComponent(m_inProcessQmlEngine, qmlFileUrl);

    if (!component->isReady()) {
        qCCritical(LogSystem).noquote().nospace() << "Failed to load component "
                                                  << m_app->info()->absoluteCodeFilePath()
                                                  << ":\n" << component->errorString();
        return false;
    }

    // We are running each application in it's own, separate Qml context.
    // This way, we can export an unique ApplicationInterface object for each app
    QQmlContext *appContext = new QQmlContext(m_inProcessQmlEngine->rootContext(), this);
    m_applicationIf = new QmlInProcessApplicationInterface(this);
    appContext->setContextProperty(qSL("ApplicationInterface"), m_applicationIf);
    connect(m_applicationIf, &QmlInProcessApplicationInterface::quitAcknowledged,
            this, [this]() { finish(0, Am::NormalExit); });

    if (appContext->setProperty(s_runtimeKey, QVariant::fromValue(this)))
        qCritical() << "Could not set" << s_runtimeKey << "property in QML context";

    QObject *obj = component->beginCreate(appContext);

    QMetaObject::invokeMethod(this, [component, obj, this]() {
        component->completeCreate();
        delete component;
        if (!obj) {
            qCCritical(LogSystem) << "could not load" << m_app->info()->absoluteCodeFilePath() << ": no root object";
            finish(3, Am::NormalExit);
        } else {
            if (state() == Am::ShuttingDown) {
                delete obj;
                return;
            }
#if !defined(AM_HEADLESS)
            if (!qobject_cast<QmlInProcessApplicationManagerWindow*>(obj)) {
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
    }, Qt::QueuedConnection);
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
        finish(exitCode, Am::ForcedExit);
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
        finish(exitCode, Am::ForcedExit);
    });
}

void QmlInProcessRuntime::finish(int exitCode, Am::ExitStatus status)
{
    QMetaObject::invokeMethod(this, [this, exitCode, status]() {
        qCDebug(LogSystem) << "QmlInProcessRuntime (id:" << (m_app ? m_app->id() : qSL("(none)"))
                           << ") exited with code:" << exitCode << "status:" << status;
        emit finished(exitCode, status);
        if (m_app)
            m_app->setCurrentRuntime(nullptr);
        setState(Am::NotRunning);
#if !defined(AM_HEADLESS)
        if (m_surfaces.isEmpty())
            deleteLater();
#else
        deleteLater();
#endif
    }, Qt::QueuedConnection);
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
            connect(surfacePtr, &InProcessSurfaceItem::visibleClientSideChanged, this, [this]() {
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

/*! \internal
  In single process mode, every app plus the system-ui itself run within the same QQmlEngine. For
  some operations, we need to figure out though, which app/system-ui is the currently "active" one.
  In order to do that we need an anchor or hint: the \a object parameter. Each QObject exposed to
  QML has an associated QQmlContext. For objects created by apps, we can deduce the apps's root
  context from this object context. This fails however, if the object is a singleton or an object
  exposed from C++. In this case we can examine the "calling context" of the V4 JS engine to
  determine who called into this object.
*/
QmlInProcessRuntime *QmlInProcessRuntime::determineRuntime(QObject *object)
{
    auto findRuntime = [](QQmlContext *context) -> QmlInProcessRuntime * {
        while (context) {
            if (context->property(s_runtimeKey).isValid())
                return context->property(s_runtimeKey).value<QmlInProcessRuntime *>();
            context = context->parentContext();
        }
        return nullptr;
    };

    // check the context the object lives in
    QmlInProcessRuntime *runtime = findRuntime(QQmlEngine::contextForObject(object));
    if (!runtime) {
        // if this didn't work out, check out the calling context
        if (QQmlEngine *engine = qmlEngine(object)) {
            if (QV4::ExecutionEngine *v4 = engine->handle()) {
                if (QQmlContextData *callingContext = v4->callingQmlContext())
                    runtime = findRuntime(callingContext->asQQmlContext());
            }
        }
    }
    return runtime;
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
