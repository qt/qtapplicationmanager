// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <private/qv4engine_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlcontextdata_p.h>
#include <QQuickView>

#include "qmlinprocessapplicationmanagerwindow.h"
#include "inprocesssurfaceitem.h"
#include "logging.h"
#include "exception.h"
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
    // if there is still a window present at this point, fire the 'closing' signal (probably) again,
    // because it's still the duty of WindowManager together with qml-ui to free and delete this item!!
    for (int i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);
}

bool QmlInProcessRuntime::start()
{
    Q_ASSERT(!m_rootObject);

    if (!m_inProcessQmlEngine)
        return false;

    if (!m_app) {
        qCCritical(LogSystem) << "tried to start without an app object";
        return false;
    }

    const QString currentDir = QDir::currentPath() + QDir::separator();
    const QString codeDir = m_app->codeDir() + QDir::separator();

    loadResources(variantToStringList(configuration().value(qSL("resources"))), currentDir);
    loadResources(variantToStringList(m_app->runtimeParameters().value(qSL("resources"))), codeDir);

    if (m_app->runtimeParameters().value(qSL("loadDummyData")).toBool()) {
        qCDebug(LogSystem) << "Loading dummy-data";
        loadQmlDummyDataFiles(m_inProcessQmlEngine, QFileInfo(m_app->info()->absoluteCodeFilePath()).path());
    }

    const QStringList configPluginPaths = variantToStringList(configuration().value(qSL("pluginPaths")));
    const QStringList runtimePluginPaths = variantToStringList(m_app->runtimeParameters().value(qSL("pluginPaths")));
    if (!configPluginPaths.isEmpty() || !runtimePluginPaths.isEmpty()) {
        addPluginPaths(configPluginPaths, currentDir);
        addPluginPaths(runtimePluginPaths, codeDir);
        qCDebug(LogSystem) << "Updated plugin paths:" << qApp->libraryPaths();
    }

    const QStringList configImportPaths = variantToStringList(configuration().value(qSL("importPaths")));
    const QStringList runtimeImportPaths = variantToStringList(m_app->runtimeParameters().value(qSL("importPaths")));
    if (!configImportPaths.isEmpty() || !runtimeImportPaths.isEmpty()) {
        addImportPaths(configImportPaths, currentDir);
        addImportPaths(runtimeImportPaths, codeDir);
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

    emit signaler()->aboutToStart(this);

    setState(Am::StartingUp);

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

            if (!qobject_cast<QmlInProcessApplicationManagerWindow*>(obj)) {
                QQuickItem *item = qobject_cast<QQuickItem*>(obj);
                if (item) {
                    auto surfaceItem = new InProcessSurfaceItem;
                    item->setParentItem(surfaceItem);
                    addSurfaceItem(QSharedPointer<InProcessSurfaceItem>(surfaceItem));
                }
            }
            m_rootObject = obj;
            setState(Am::Running);

            if (!m_document.isEmpty())
                openDocument(m_document, QString());
        }
    }, Qt::QueuedConnection);
    return true;
}

void QmlInProcessRuntime::stop(bool forceKill)
{
    setState(Am::ShuttingDown);
    emit aboutToStop();

    for (int i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);

    if (m_surfaces.isEmpty()) {
        delete m_rootObject;
        m_rootObject = nullptr;
    }

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

        if (m_surfaces.isEmpty())
            deleteLater();
    }, Qt::QueuedConnection);
}

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

void QmlInProcessRuntime::loadResources(const QStringList &resources, const QString &baseDir)
{
    for (const QString &resource : resources) {
        const QString path = QFileInfo(resource).isRelative() ? baseDir + resource : resource;
        static QStringList cache;
        if (!cache.contains(path)) {
            try {
                loadResource(path);
            } catch (const Exception &e) {
                qCWarning(LogQmlRuntime).noquote() << e.errorString();
            }
            cache.append(path);
        }
    }
}

void QmlInProcessRuntime::addPluginPaths(const QStringList &pluginPaths, const QString &baseDir)
{
    for (const QString &path : pluginPaths)
        QCoreApplication::addLibraryPath(QFileInfo(path).isRelative() ? baseDir + path : path);
}

void QmlInProcessRuntime::addImportPaths(const QStringList &importPaths, const QString &baseDir)
{
    for (const QString &path : importPaths)
        m_inProcessQmlEngine->addImportPath(toAbsoluteFilePath(path, baseDir));
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
        emit signaler()->inProcessSurfaceItemReady(this, surface);
    }
}

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
  In single process mode, every app plus the System UI itself run within the same QQmlEngine. For
  some operations, we need to figure out though, which app/System UI is the currently "active" one.
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
                if (QQmlContextData *callingContext = v4->callingQmlContext().data())
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

#include "moc_qmlinprocessruntime.cpp"
