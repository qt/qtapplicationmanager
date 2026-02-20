// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlIncubator>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <private/qv4engine_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlcontextdata_p.h>

#include "applicationmanagerwindow.h"
#include "inprocesssurfaceitem.h"
#include "logging.h"
#include "exception.h"
#include "applicationinterface.h"
#include "application.h"
#include "qmlinprocruntime.h"
#include "qmlinprocapplicationinterfaceimpl.h"
#include "abstractcontainer.h"
#include "utilities.h"
#include "qml-utilities.h"

using namespace std::chrono_literals;
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
                qCCritical(LogQmlRuntime) << "Cannot determine Runtime to setup a ApplicationInterface object";
            Q_ASSERT(rt);
            return impl;
        });
    }
}

QmlInProcRuntime::~QmlInProcRuntime()
{
    QmlInProcRuntime::stop(Am::ForcedExit); // force non-virtual call

    // if there is still a window present at this point, fire the 'closing' signal (probably) again,
    // because it's still the duty of WindowManager together with qml-ui to free and delete this item!!
    for (auto i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setVisibleClientSide(false);
}

bool QmlInProcRuntime::start()
{
    Q_ASSERT(!m_rootObject);
    Q_ASSERT(state() == Am::NotRunning);

    if (!m_inProcessQmlEngine)
        return false;

    if (!m_app) {
        qCCritical(LogQmlRuntime) << "tried to start without an app object";
        return false;
    }

    qCDebug(LogQmlRuntime) << "Loading" << m_app->id() << "from" << m_app->info()->absoluteCodeFilePath();
    if (!m_document.isEmpty())
        qCDebug(LogQmlRuntime) << " * document:" << m_document;

    const QString currentDir = QDir::currentPath() + QDir::separator();
    const QString codeDir = m_app->codeDir() + QDir::separator();
    const QUrl qmlFileUrl = filePathToUrl(m_app->info()->absoluteCodeFilePath(), codeDir);

    if (!m_component) {
        m_component = new QQmlComponent(m_inProcessQmlEngine, this);
        connect(m_component, &QQmlComponent::statusChanged,
                this, [this](QQmlComponent::Status status) {
            if (status == QQmlComponent::Loading) {
                // ignore
            } else if (status == QQmlComponent::Error) {
                m_componentErrorString = m_component->errorString();
                qCCritical(LogQmlRuntime).noquote().nospace() << "Failed to load component "
                                                              << m_app->info()->absoluteCodeFilePath()
                                                              << ":\n" << m_componentErrorString;
                finish(3, Am::NormalExit);
            } else if (status == QQmlComponent::Ready) {
                incubate();
            }
        });
    }

    switch (m_component->status()) {
    case QQmlComponent::Loading:
        Q_ASSERT(false);
        return false;

    case QQmlComponent::Error: // we tried before and failed
        qCCritical(LogQmlRuntime).noquote().nospace() << "Failed to load component "
                                                      << m_app->info()->absoluteCodeFilePath()
                                                      << "earlier:\n" << m_componentErrorString;
        return false;

    case QQmlComponent::Null: { // first time - load the component
        const QStringList configPluginPaths = variantToStringList(configuration().value(u"pluginPaths"_s));
        const QStringList runtimePluginPaths = variantToStringList(m_app->runtimeParameters().value(u"pluginPaths"_s));
        if (!configPluginPaths.isEmpty() || !runtimePluginPaths.isEmpty()) {
            addPluginPaths(configPluginPaths, currentDir);
            addPluginPaths(runtimePluginPaths, codeDir);
        }
        qCDebug(LogQmlRuntime) << " * plugin paths:" << qApp->libraryPaths();

        const QStringList configImportPaths = variantToStringList(configuration().value(u"importPaths"_s));
        const QStringList runtimeImportPaths = variantToStringList(m_app->runtimeParameters().value(u"importPaths"_s));
        if (!configImportPaths.isEmpty() || !runtimeImportPaths.isEmpty()) {
            addImportPaths(configImportPaths, currentDir);
            addImportPaths(runtimeImportPaths, codeDir);
        }
        qCDebug(LogQmlRuntime) << " * Qml import paths:" << m_inProcessQmlEngine->importPathList();

        loadResources(variantToStringList(configuration().value(u"resources"_s)), currentDir);
        loadResources(variantToStringList(m_app->runtimeParameters().value(u"resources"_s)), codeDir);

        m_component->loadUrl(qmlFileUrl, QQmlComponent::Asynchronous);

        if (m_component->isError()) { // undocumented, but this can happen without a statusChange signal
            qCCritical(LogQmlRuntime).noquote().nospace() << "Failed to load component "
                                                          << m_app->info()->absoluteCodeFilePath()
                                                          << ":\n" << m_component->errorString();
            return false;
        }

        emit signaler()->aboutToStart(this);
        setState(Am::StartingUp);
        return true;
    }
    case QQmlComponent::Ready: // already loaded and ready to go
        emit signaler()->aboutToStart(this);
        setState(Am::StartingUp);

        incubate();
        return true;
    }
    Q_ASSERT(false);
    return false;
}

void QmlInProcRuntime::incubate()
{
    // We are running each application in its own, separate Qml context.
    // This way, we can export a unique runtime-tag to this context to then later
    // determine at runtime which application is currently active.
    auto appContext = new QQmlContext(m_inProcessQmlEngine->rootContext(), this);
    if (!tagQmlContext(appContext, QVariant::fromValue(this))) {
        qCCritical(LogQmlRuntime) << "Could not tag the application QML context";
        finish(4, Am::NormalExit);
        return;
    }

    m_component->create(m_incubator, appContext, nullptr);

    // QQmlIncubator has no signal for when it's done, so we have to poll via a timer
    if (!m_incubationTimer) {
        m_incubationTimer = new QTimer(this);
        m_incubationTimer->setInterval(16ms);

        connect(m_incubationTimer, &QTimer::timeout, this, [this]() {
            if (m_incubator.isError()) {
                const auto errors = m_incubator.errors();
                QStringList strErrors;
                for (const auto &e : errors)
                    strErrors << e.toString();
                qCCritical(LogQmlRuntime) << "Failed to load component:" << m_app->info()->absoluteCodeFilePath()
                                          << ":\n *" << strErrors.join(u"\n *");
                finish(3, Am::NormalExit);
            } else if (m_incubator.isReady()) {
                auto *obj = m_incubator.object();
                Q_ASSERT(obj);

                if (state() == Am::ShuttingDown) {
                    delete obj;
                } else {
                    if (!qobject_cast<ApplicationManagerWindow *>(obj)) {
                        QQuickItem *item = qobject_cast<QQuickItem *>(obj);
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
            } else {
                return; // wait for the next timer tick
            }
            // cleanup regardless if we succeeded or failed
            m_incubator.clear();
            m_incubationTimer->stop();
            m_incubationTimer->deleteLater();
            m_incubationTimer = nullptr;
        });
    }
    m_incubationTimer->start();
}

void QmlInProcRuntime::stop(Am::ExitStatus exitStatus)
{
    switch (state()) {
    case Am::NotRunning:
    case Am::ShuttingDown:
        return;

    case Am::StartingUp:
        setState(Am::ShuttingDown);
        if (m_component && m_component->isLoading()) {
            // QML can deal with a loading component being deleted
            delete m_component;
            m_component = nullptr;
        } else {
            // the incubation finished, but the incubationTimer didn't fire yet
            if (m_incubator.isReady() && m_incubator.object())
                delete m_incubator.object();
            // cancel any active incubations
            m_incubator.clear();
            if (m_incubationTimer)
                m_incubationTimer->stop();
        }
        finish(exitStatus);
        return;

    case Am::Running:
        break;
    }

    setState(Am::ShuttingDown);

    if (exitStatus == Am::NormalExit)
        emit aboutToStop();

    for (auto i = m_surfaces.size(); i; --i)
        m_surfaces.at(i-1)->setDeadClientSide();

    if (m_surfaces.isEmpty()) {
        delete m_rootObject;
        m_rootObject = nullptr;
    }

    if (exitStatus == Am::NormalExit) {
        bool ok;
        int qt = configuration().value(u"quitTime"_s).toInt(&ok);
        if (!ok || qt < 0)
            qt = 250;
        QTimer::singleShot(qt, this, [this]() {
            if (state() != Am::NotRunning)
                finish(m_allWindowsClosed ? Am::NormalExit : Am::ForcedExit);
        });
    } else {
        finish(exitStatus);
    }
}

void QmlInProcRuntime::finish(Am::ExitStatus status)
{
    int exitCode = [=] {
        switch (status) {
        case Am::NormalExit:   return 0;
        case Am::CrashExit:    return 11; // POSIX SIGSEGV
        case Am::ForcedExit:   return 9;  // POSIX SIGKILL
        case Am::WatchdogExit: return 16; // POSIX SIGSTKFLT
        }
        Q_UNREACHABLE_RETURN(-1);
    }();

    finish(exitCode, status);
}

void QmlInProcRuntime::finish(int exitCode, Am::ExitStatus status)
{
    QMetaObject::invokeMethod(this, [this, exitCode, status]() {
        QByteArray cause = "exited";
        bool printWarning = false;
        switch (status) {
        case Am::WatchdogExit:
            cause = "was killed by the watchdog";
            printWarning = true;
            break;
        case Am::ForcedExit:
            cause = "was force exited";
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
            qCWarning(LogQmlRuntime, "In-process runtime for application '%s' %s",
                      (m_app ? qPrintable(m_app->id()) : "<null>"), cause.constData());
        } else {
            qCDebug(LogQmlRuntime, "In-process runtime for application '%s' %s",
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

void QmlInProcRuntime::stopIfLastWindowClosed()
{
    m_allWindowsClosed = true;
    for (const auto &s : std::as_const(m_surfaces)) {
        if (s->visibleClientSide() || !s->isClosed()) {
            m_allWindowsClosed = false;
            break;
        }
    }

    if (m_allWindowsClosed)
        stop(Am::NormalExit);
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
        if (!m_surfaces.contains(surface)) {
            m_surfaces.append(surface);
            InProcessSurfaceItem *surfacePtr = surface.data();
            connect(surfacePtr, &InProcessSurfaceItem::released, this, [this, surfacePtr]() {
                onSurfaceItemReleased(surfacePtr);
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

void QmlInProcRuntime::setApplicationExtraDirs(const QMap<QString, QString> &extraPaths)
{
    m_applicationExtraDirs = extraPaths;
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
    : QmlInProcRuntimeManager(u"qml-inprocess"_s, parent)
{ }

QmlInProcRuntimeManager::QmlInProcRuntimeManager(const QString &id, QObject *parent)
    : AbstractRuntimeManager(id, parent)
{ }

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
