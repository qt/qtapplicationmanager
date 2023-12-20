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

#include "applicationmanagerwindow.h"
#include "inprocesssurfaceitem.h"
#include "logging.h"
#include "exception.h"
#include "applicationinterface.h"
#include "applicationinterfaceimpl.h"
#include "application.h"
#include "qmlinprocruntime.h"
#include "qmlinprocapplicationinterfaceimpl.h"
#include "abstractcontainer.h"
#include "global.h"
#include "utilities.h"
#include "qml-utilities.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM


const char *QmlInProcRuntime::s_runtimeKey = "_am_runtime";


QmlInProcRuntime::QmlInProcRuntime(Application *app, QmlInProcRuntimeManager *manager)
    : AbstractRuntime(nullptr, app, manager)
{ }

QmlInProcRuntime::~QmlInProcRuntime()
{
    // if there is still a window present at this point, fire the 'closing' signal (probably) again,
    // because it's still the duty of WindowManager together with qml-ui to free and delete this item!!
    for (auto i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);
}

bool QmlInProcRuntime::start()
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

    loadResources(variantToStringList(configuration().value(u"resources"_s)), currentDir);
    loadResources(variantToStringList(m_app->runtimeParameters().value(u"resources"_s)), codeDir);

    if (m_app->runtimeParameters().value(u"loadDummyData"_s).toBool()) {
        qCWarning(LogDeployment) << "Loading dummy data is deprecated and will be removed soon";
        qCDebug(LogSystem) << "Loading dummy-data";
        loadQmlDummyDataFiles(m_inProcessQmlEngine, QFileInfo(m_app->info()->absoluteCodeFilePath()).path());
    }

    const QStringList configPluginPaths = variantToStringList(configuration().value(u"pluginPaths"_s));
    const QStringList runtimePluginPaths = variantToStringList(m_app->runtimeParameters().value(u"pluginPaths"_s));
    if (!configPluginPaths.isEmpty() || !runtimePluginPaths.isEmpty()) {
        addPluginPaths(configPluginPaths, currentDir);
        addPluginPaths(runtimePluginPaths, codeDir);
        qCDebug(LogSystem) << "Updated plugin paths:" << qApp->libraryPaths();
    }

    const QStringList configImportPaths = variantToStringList(configuration().value(u"importPaths"_s));
    const QStringList runtimeImportPaths = variantToStringList(m_app->runtimeParameters().value(u"importPaths"_s));
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

    // We are running each application in its own, separate Qml context.
    // This way, we can export a unique ApplicationInterface object for each app
    QQmlContext *appContext = new QQmlContext(m_inProcessQmlEngine->rootContext(), this);

    m_applicationIf = ApplicationInterface::create<QmlInProcApplicationInterfaceImpl>(this, this);
    appContext->setContextProperty(u"ApplicationInterface"_s, m_applicationIf);

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

            if (!qobject_cast<ApplicationManagerWindow*>(obj)) {
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

void QmlInProcRuntime::stop(bool forceKill)
{
    setState(Am::ShuttingDown);
    emit aboutToStop();

    for (auto i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);

    if (m_surfaces.isEmpty()) {
        delete m_rootObject;
        m_rootObject = nullptr;
    }

    if (forceKill) {
        finish(9 /* POSIX SIGKILL */, Am::ForcedExit);
        return;
    }

    bool ok;
    int qt = configuration().value(u"quitTime"_s).toInt(&ok);
    if (!ok || qt < 0)
        qt = 250;
    QTimer::singleShot(qt, this, [this]() {
        finish(15 /* POSIX SIGTERM */, Am::ForcedExit);
    });
}

void QmlInProcRuntime::finish(int exitCode, Am::ExitStatus status)
{
    QMetaObject::invokeMethod(this, [this, exitCode, status]() {
        qCDebug(LogSystem) << "QmlInProcRuntime (id:" << (m_app ? m_app->id() : u"(none)"_s)
                           << ") exited with code:" << exitCode << "status:" << status;
        emit finished(exitCode, status);
        if (m_app)
            m_app->setCurrentRuntime(nullptr);
        setState(Am::NotRunning);

        if (m_surfaces.isEmpty())
            deleteLater();
    }, Qt::QueuedConnection);
}

void QmlInProcRuntime::stopIfNoVisibleSurfaces()
{
    if (!hasVisibleSurfaces())
        stop();
    else
        m_stopIfNoVisibleSurfaces = true;
}

bool QmlInProcRuntime::hasVisibleSurfaces() const
{
    for (int i = 0; i < m_surfaces.count(); ++i) {
        if (m_surfaces[i]->visibleClientSide())
            return true;
    }
    return false;
}

void QmlInProcRuntime::onSurfaceItemReleased(InProcessSurfaceItem *surface)
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

void QmlInProcRuntime::loadResources(const QStringList &resources, const QString &baseDir)
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

void QmlInProcRuntime::addPluginPaths(const QStringList &pluginPaths, const QString &baseDir)
{
    for (const QString &path : pluginPaths)
        QCoreApplication::addLibraryPath(QFileInfo(path).isRelative() ? baseDir + path : path);
}

void QmlInProcRuntime::addImportPaths(const QStringList &importPaths, const QString &baseDir)
{
    for (const QString &path : importPaths)
        m_inProcessQmlEngine->addImportPath(toAbsoluteFilePath(path, baseDir));
}

void QmlInProcRuntime::addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface)
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

void QmlInProcRuntime::openDocument(const QString &document, const QString &mimeType)
{
    m_document = document;
    if (m_applicationIf)
        emit m_applicationIf->openDocument(document, mimeType);
}

qint64 QmlInProcRuntime::applicationProcessId() const
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
QmlInProcRuntime *QmlInProcRuntime::determineRuntime(QObject *object)
{
    auto findRuntime = [](QQmlContext *context) -> QmlInProcRuntime * {
        while (context) {
            if (context->property(s_runtimeKey).isValid())
                return context->property(s_runtimeKey).value<QmlInProcRuntime *>();
            context = context->parentContext();
        }
        return nullptr;
    };

    // check the context the object lives in
    QmlInProcRuntime *runtime = findRuntime(QQmlEngine::contextForObject(object));
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

QmlInProcRuntimeManager::QmlInProcRuntimeManager(QObject *parent)
    : AbstractRuntimeManager(defaultIdentifier(), parent)
{ }

QmlInProcRuntimeManager::QmlInProcRuntimeManager(const QString &id, QObject *parent)
    : AbstractRuntimeManager(id, parent)
{ }

QString QmlInProcRuntimeManager::defaultIdentifier()
{
    return u"qml-inprocess"_s;
}

bool QmlInProcRuntimeManager::inProcess() const
{
    return true;
}

AbstractRuntime *QmlInProcRuntimeManager::create(AbstractContainer *container, Application *app)
{
    if (container) {
        delete container;
        return nullptr;
    }
    return new QmlInProcRuntime(app, this);
}

QT_END_NAMESPACE_AM

#include "moc_qmlinprocruntime.cpp"
