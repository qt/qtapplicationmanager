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
#include "application.h"
#include "qmlinprocruntime.h"
#include "qmlinprocapplicationinterfaceimpl.h"
#include "abstractcontainer.h"
#include "global.h"
#include "utilities.h"
#include "qml-utilities.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM


QmlInProcRuntime::QmlInProcRuntime(Application *app, QmlInProcRuntimeManager *manager)
    : AbstractRuntime(nullptr, app, manager)
    , m_applicationInterfaceImpl(new QmlInProcApplicationInterfaceImpl(this))
{
    static bool once = false;
    if (!once) {
        once = true;

        ApplicationInterfaceImpl::setFactory([](ApplicationInterface *iface) {
            ApplicationInterfaceImpl *impl = nullptr;
            QmlInProcRuntime *rt = QmlInProcRuntime::determineRuntime(iface->parent());

            if (rt)
                impl = rt->m_applicationInterfaceImpl.get();
            else
                qCCritical(LogRuntime) << "Cannot determine Runtime to setup a ApplicationInterface object";
            Q_ASSERT(rt);
            return impl;
        });
    }
}

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
    // This way, we can export a unique runtime-tag to this context to then later
    // determine at runtime which application is currently active.
    auto appContext = new QQmlContext(m_inProcessQmlEngine->rootContext(), this);
    if (!tagQmlContext(appContext, QVariant::fromValue(this)))
        qCritical() << "Could not tag the application QML context";

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

    if (!forceKill)
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
        if (state() != Am::NotRunning)
            finish(15 /* POSIX SIGTERM */, Am::ForcedExit);
    });
}

void QmlInProcRuntime::finish(int exitCode, Am::ExitStatus status)
{
    QMetaObject::invokeMethod(this, [this, exitCode, status]() {
        QByteArray cause = "exited";
        bool printWarning = false;
        switch (status) {
        case Am::ForcedExit:
            cause = "was force exited (" + QByteArray(exitCode == 15 /* POSIX SIGTERM */ ? "terminated" : "killed") + ")";
            printWarning = true;
            break;
        default:
            if (exitCode != 0) {
                cause = "exited with code: " + QByteArray::number(exitCode);
                printWarning = true;
            }
            break;
        }

        if (printWarning) {
            qCWarning(LogSystem, "In-process runtime for application '%s' %s",
                      (m_app ? qPrintable(m_app->id()) : "<null>"), cause.constData());
        } else {
            qCDebug(LogSystem, "In-process runtime for application '%s' %s",
                    (m_app ? qPrintable(m_app->id()) : "<null>"), cause.constData());
        }

        if (state() != Am::NotRunning)
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
        if (surface == m_surfaces.at(i).data()) {
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
    static QStringList cache;

    for (const QString &resource : resources) {
        const QString path = QFileInfo(resource).isRelative() ? baseDir + resource : resource;
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
    const auto ifaces = m_applicationInterfaceImpl->amInterfaces();
    for (auto *iface : ifaces)
        emit iface->openDocument(document, mimeType);
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
    return findTaggedQmlContext(object).value<QmlInProcRuntime *>();
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
