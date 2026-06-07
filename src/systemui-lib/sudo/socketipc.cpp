// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:communication-protocol

#include "socketipc.h"
#include "socketipc_p.h"

#include <unistd.h>
#include <sys/socket.h>

#include <QtCore/private/qcore_unix_p.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QRandomGenerator>
#include <QSocketNotifier>
#include <QTimer>

/*! \class SocketIpc
    \internal

    \brief RPC-style IPC channel between two processes connected by a local socket pair.

    One \c SocketIpc instance lives on each side of the channel - one in the \l{Role}{Server} role,
    one in the \l{Role}{Client} role. The client sends \c InvokeMethod requests; the server
    dispatches them to registered \c Q_INVOKABLE methods and returns the result (or an
    exception) as a reply. The server can also emit \c Q_SIGNALS that are forwarded to the
    client.

    \section2 Security

    Each \c SocketIpc instance keeps a per-direction message counter; the receiving side rejects any
    message whose counter does not match the next expected value. The starting counter values
    are seeded from a cryptographically secure random source, so a blind injector would have
    to guess a random 64-bit number on the very first message to succeed. Any mismatch is
    treated as a fatal protocol violation and terminates the process.

    \note This does not protect against an attacker that can load code into either process:
    they can read the counters out of process memory or call the target methods directly. The
    counter scheme is meant to defend against blind in-process injection (a plugin that spams
    the socket fd without inspecting our state) and accidental third-party interference.
    In-process attackers with full memory access are considered out of scope - they share the
    address space, and there is no security boundary inside a process.

    \section2 Setup

    The typical flow is:

    \list 1
    \li Call \l SocketIpcConfiguration::createSocketPair() in the parent process.
    \li \c fork(). Both processes inherit a copy of the configuration.
    \li In each side, construct a \c SocketIpc instance with \c std::move(cfg) and the appropriate
        \l Role. The constructor adopts the local end of the socket pair and closes the peer's.
    \li On the server side, register all singletons and factories via \l registerSingleton(),
        \l registerCallbackObjectFactory() and \l registerMetaObjectFactory(). On the client
        side there is nothing to register up front.
    \li Call \l start() on both sides. Calling \c start() on the server only after all
        registrations are in place prevents a race where the client's first request would
        arrive before the server knows about the requested class.
    \endlist

    \section2 Defining interfaces

    Declare a common interface as a \c QObject-derived class with pure virtual \c Q_INVOKABLE
    methods (client -> server) and \c Q_SIGNALS (server -> client).

    Implement the server side by deriving from that interface and actually implementing the
    pure virtual \c Q_INVOKABLE methods. Simply emit the \c Q_SIGNALS when you want to notify
    the client. You can throw an \c QtAM::Exception in any \c Q_INVOKABLE method to signal an
    error and have it propagated to the client side.

    Implement the client side by deriving from the same interface and forwarding each method
    to the IPC:

    \code
    int InterfaceClient::add(int v1, int v2) final
    {
        return m_ipc->invokeMethod<int>(this, __func__, v1, v2);
    }

    QFuture<int> InterfaceClient::addAsync(int v1, int v2) final
    {
        return m_ipc->invokeMethodAsync<int>(this, "add", v1, v2);
    }
    \endcode

    \section2 Supported types

    Any built-in or registered type that is supported by \c QVariant can be used for arguments
    and return values. User types must be registered with the same type id on both sides, which
    means registering them \e before forking. Late or one-sided registration may lead to failed
    calls or, worse, crashes if the same type id gets reused for different classes on the two
    sides.
*/

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

Q_LOGGING_CATEGORY(LogSIpc, "am.socketipc", QtWarningMsg)

static quint64 incrementMessageCounter(quint64 &counter)
{
    // avoid '0' as the next (+1) message number, as that has a special meaning (reply)
    if (++counter == std::numeric_limits<quint64>::max())
        counter = 0;
    return counter;
}

static const char *roleName(SocketIpc::Role role)
{
    switch (role) {
    case SocketIpc::Role::Server:
        return "Server";
    case SocketIpc::Role::Client:
        return "Client";
    default:
        return "Unknown";
    }
}

// only used in qCDebug output
static const char *callName(SocketIpcMessage::Call call)
{
    switch (call) {
    case SocketIpcMessage::Call::InvokeMethod:
        return "InvokeMethod";
    case SocketIpcMessage::Call::EmitSignal:
        return "EmitSignal";
    case SocketIpcMessage::Call::InvokeMethodResult:
        return "InvokeMethodResult";
    case SocketIpcMessage::Call::InvokeMethodException:
        return "InvokeMethodException";
    default:
        return "Unknown";
    }
}

/*! \internal

    Constructs a SocketIpc instance in the given \a role, adopting the corresponding socket end out
    of \a config. The other end of the pair is closed during construction - it is the peer's
    side and not needed in this process. \a config is left empty after the call.

    The receiver thread is created but not yet running; it is started by \l start(). This lets
    the caller register all server-side classes (or set up client-side state) before any
    incoming message can be dispatched.

    \a parent follows the usual \c QObject parent-child semantics. The typical lifetime is a
    stack frame or a \c std::unique_ptr; passing a Qt parent is unusual but supported.
*/
SocketIpc::SocketIpc(SocketIpcConfiguration &&config, Role role, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<SocketIpcPrivate>(this))
{
    bool isServer = (role == Role::Server);

    d->m_role = role;

    // Transfer ownership of the fd we keep out of config so config.clear() won't close it.
    auto *cd = config.d.get();
    if (isServer) {
        d->m_socket = cd->m_serverSocket;
        cd->m_serverSocket = -1;

        d->m_messageCounterIncoming = cd->m_messageCounterStartClient;
        d->m_messageCounterOutgoing = cd->m_messageCounterStartServer;
    } else {
        d->m_socket = cd->m_clientSocket;
        cd->m_clientSocket = -1;

        d->m_messageCounterIncoming = cd->m_messageCounterStartServer;
        d->m_messageCounterOutgoing = cd->m_messageCounterStartClient;
    }

    config.clear();

    // the client side object is just a dummy. Registered into m_classes for ownership; the
    // m_registry raw pointer is just a convenience handle for call sites in this file.
    d->m_registry = new SocketIpcRegistry(this, d.get());
    if (d->m_role == Role::Server)
        d->addSingletonClass("#Registry", std::unique_ptr<QObject>(d->m_registry));
    else
        d->addObjectInstance("#Registry", 0, d->m_registry);

    // Create a thread to not block (and also not be blocked by) the main thread
    // Do not start it immediately though to give users a chance to register all server-side
    // classes before incoming messages are dispatched.
    d->m_receiverThread.reset(QThread::create([this]() {
        QSocketNotifier sn(d->m_socket, QSocketNotifier::Read);
        QObject::connect(&sn, &QSocketNotifier::activated,
                         &sn, [this] { d->receive(); }, Qt::DirectConnection);
        QEventLoop().exec();
    }));
    d->m_receiverThread->setObjectName("SocketIpc Thread");
    d->moveToThread(d->m_receiverThread.get());
    d->m_registry->moveToThread(d->m_receiverThread.get());
}

/*! \internal

    Stops the receiver thread, closes the socket, finishes any outstanding async-call promises
    with an exception so blocked callers do not hang, and deletes every SocketIpc-owned QObject
    (server-side singletons and factory-created instances).
*/
SocketIpc::~SocketIpc()
{
    d->m_receiverThread->quit();
    d->m_receiverThread->wait();

    if (d->m_socket != -1)
        ::close(d->m_socket);
}

/*! \internal

    Begins processing incoming messages. Must be called explicitly after construction:

    \list
    \li Server: call \b after all \c registerSingleton(), \c registerCallbackObjectFactory() and
        \c registerMetaObjectFactory() calls, so the receiver thread cannot dispatch a request
        for a class that has not been registered yet.
    \li Client: call \b before any \c bindSingleton(), \c createInstance() or \c invokeMethod()
        call, since the receiver thread is also what dispatches replies to client-initiated
        requests.
    \endlist

    Calling \c start() more than once is a no-op.
*/
void SocketIpc::start()
{
    Q_ASSERT(QCoreApplication::instance()); // the receiver thread needs an event loop to run

    if (!d->m_receiverThread->isRunning())
        d->m_receiverThread->start();
}

/*! \internal
    \fn SocketIpc::setRequestTimeout(std::chrono::milliseconds timeout)

    Configures a per-request soft deadline. With a non-zero \a timeout every async request will
    have its promise finished with a "request timed out" error if no reply arrives within the
    configured time; the channel itself stays open and further requests can still succeed (a
    successful exchange will also reset the consecutive-timeout counter that feeds the
    \l setMaxConsecutiveTimeouts() cascade). A zero \a timeout (the default) disables the
    timeout entirely.

    Should be called before \l start(). Calling it after \c start() is permitted but only
    affects requests that are sent thereafter.

    \sa setMaxConsecutiveTimeouts
*/
void SocketIpc::setRequestTimeout(std::chrono::milliseconds timeout)
{
    d->m_requestTimeoutMs = timeout.count();
}

/*! \internal
    \fn SocketIpc::setMaxConsecutiveTimeouts(int maxCount)

    Configures the stuck-peer detection: once \a maxCount request timeouts have fired
    back-to-back with no successful reply in between, the IPC layer treats the peer as
    unresponsive and calls \c qFatal() to bring the local process down. Pair with
    \l setRequestTimeout() - the cascade does nothing unless a non-zero timeout is also set.

    Any successful reply resets the internal counter to zero, so a flaky peer that recovers
    in time will not trigger the cascade. A zero \a maxCount (the default) disables the
    cascade entirely; timeouts then surface only as individual exceptions on the affected
    QFutures.

    Should be called before \l start().

    \sa setRequestTimeout
*/
void SocketIpc::setMaxConsecutiveTimeouts(int maxCount)
{
    d->m_maxConsecutiveTimeouts = maxCount;
}

/*! \internal

    Registers \a obj as the server-side singleton for its IPC class. The class name is read
    from the \c Q_CLASSINFO("SocketIpcClassName", ...) of \a obj's meta-object; if that classinfo
    is missing an exception is thrown.

    Ownership of \a obj is transferred to the SocketIpc instance; it is destroyed when this SocketIpc is.

    Must only be called on the server side, and before \l start().
*/
void SocketIpc::registerSingleton(std::unique_ptr<QObject> obj)
{
    int ci = obj ? obj->metaObject()->indexOfClassInfo("SocketIpcClassName") : -1;
    if (ci < 0)
        throw Exception("Cannot call registerSingleton on a QObject without a Q_CLASSINFO(\"SocketIpcClassName\", ...)");
    QByteArray className = obj->metaObject()->classInfo(ci).value();

    d->addSingletonClass(className, std::move(obj));
}

/*! \internal

    Registers a server-side factory for \a className. The given \a factory is invoked each
    time a client calls \c createInstance for that class, with the request's arguments. It
    must return a new \c QObject; ownership is transferred to the SocketIpc instance.

    Must only be called on the server side, and before \l start(). See also
    \l registerMetaObjectFactory() for the \c QMetaObject-based variant.
*/
void SocketIpc::registerCallbackObjectFactory(const QByteArray &className,
                                const std::function<std::unique_ptr<QObject> (SocketIpc *, const QByteArray &, const QVariantList &)> &factory)
{
    d->addFactoryClass(className, factory);
}

/*! \internal
    \fn template<typename T> void SocketIpc::registerMetaObjectFactory()

    Registers a server-side factory that constructs instances of \c T using its
    \c Q_INVOKABLE constructor(s) and the request's arguments. \c T must have at least one
    constructor decorated with \c Q_INVOKABLE; if there are several, the one matching the
    arguments given by the client is selected. The class name is read from
    \c Q_CLASSINFO("SocketIpcClassName", ...) of \c T's meta-object.

    Instances are created synchronously on the SocketIpc thread. If they need to live elsewhere,
    push them in the constructor body or use \l registerCallbackObjectFactory() instead.

    Must only be called on the server side, and before \l start().
*/

void SocketIpc::doRegisterMetaObjectFactory(const QMetaObject *metaObject)
{
    int ci = metaObject->indexOfClassInfo("SocketIpcClassName");
    if (ci < 0)
        throw Exception("Cannot register QMetaObject without an Q_CLASSINFO(\"SocketIpcClassName\", ...)");
    QByteArray className = metaObject->classInfo(ci).value();

    auto metaObjectFactory = [metaObject](SocketIpc *ipc, const QByteArray &className,
                                          const QVariantList &vargs) -> std::unique_ptr<QObject> {
        Q_UNUSED(className);
        Q_UNUSED(ipc);

        for (int ci = 0; ci < metaObject->constructorCount(); ++ci) {
            QMetaMethod mm = metaObject->constructor(ci);

            if (mm.parameterCount() != vargs.size())
                continue;

            QStringList debugParams;
            std::vector<void *> argv(mm.parameterCount() + 1);

            bool match = true;
            for (int pi = 0; pi < mm.parameterCount(); ++pi) {
                if (mm.parameterMetaType(pi) != vargs.at(pi).metaType()) {
                    match = false;
                    break;
                }
                argv[pi + 1] = const_cast<void *>(vargs.at(pi).constData());
                debugParams << vargs.at(pi).toString();
            }
            if (match) {
                // QMetaMethod::invoke would be a nice high-level API, but it only accepts
                // template parameter packs and not the simple QVariantList it uses internally

                QObject *obj = nullptr;
                argv[0] = &obj; // the return value is QObject **

                if (metaObject->static_metacall(QMetaObject::CreateInstance, ci, argv.data()) >= 0) {
                    throw Exception("Failed to call constructor of class %1 via metaobject (arguments: %2)")
                        .arg(className).arg(debugParams);
                }
                if (!obj) {
                    throw Exception("Failed to create instance of class %1 via metaobject (arguments: %2)")
                        .arg(className).arg(debugParams);
                }
                return std::unique_ptr<QObject>(obj);
            }
        }
        return { };
    };

    d->addFactoryClass(className, metaObjectFactory);
}

/*! \internal
    \fn template<typename R, typename ...ARGS> QFuture<R> SocketIpc::invokeMethodAsync(QObject *object, const QByteArray &function, ARGS && ...args)

    Sends an asynchronous \c InvokeMethod request to the server side, addressed at the
    server-side counterpart of the client proxy \a object. \a function is the method name
    (typically passed as \c "name" or via \c __func__ in a forwarding override), and \a args
    are the arguments. Returns a \c QFuture<R> that resolves with the server's return value or
    carries the server's exception.

    Client-side only. \a object must be a registered proxy obtained from \l bindSingleton() or
    \l createInstance().
*/

/*! \internal
    \fn template<typename R, typename ...ARGS> R SocketIpc::invokeMethod(QObject *object, const QByteArray &function, ARGS && ...args)

    Synchronous form of \l invokeMethodAsync(). Blocks until the reply arrives and returns the
    result. Re-throws any exception thrown by the server-side method.

    Client-side only.
*/

void SocketIpc::doInvokeMethodAsync(QObject *object, const QByteArray &function,
                              const QVariantList &vargs, std::unique_ptr<SocketIpcPromiseBase> promise)
{
    try {
        if (d->m_role == Role::Server)
            throw Exception("The server cannot send 'InvokeMethod' calls");

        QMutexLocker locker(&d->m_classesMutex);

        auto [cls, instanceId] = d->findInstance(object);
        if (!cls)
            throw Exception("The object instance is not registered");

        QByteArray className = cls->m_name;
        cls = nullptr; // make sure we don't access cls without holding a lock
        locker.unlock();

        auto msg = std::make_unique<SocketIpcMessage>(SocketIpcMessage::Call::InvokeMethod, 0, className,
                                                      instanceId, function, vargs, std::move(promise));
        d->send(std::move(msg));
    } catch (const Exception &e) {
        qFatal() << "SocketIpc::invokeMethodAsync failed on the"
                 << roleName(d->m_role)
                 << "side:" << e.what();
    }
}

/*! \internal
    \fn template<typename T> std::unique_ptr<T> SocketIpc::bindSingleton()

    Creates a local proxy of type \c T for the server-side singleton of the same IPC class
    name. The class name is read from \c Q_CLASSINFO("SocketIpcClassName", ...) of \c T. Blocks
    until the server has acknowledged the binding; throws (via \c qFatal) if the server has
    no such singleton registered. The returned \c unique_ptr is owned by the caller.

    If the proxy is destroyed before this \c SocketIpc instance, an \c unbindSingleton request is
    sent to the server. Using a proxy after the \c SocketIpc instance has been destroyed throws an
    \c Exception ("IPC connection no longer available"); the raw \c SocketIpc* held by the proxy is
    nulled out via \c QPointer.

    Client-side only.
*/

void SocketIpc::doBindSingleton(QObject *obj, const QByteArray &className)
{
    auto promise = std::make_unique<SocketIpcPromise<void>>();
    auto future = promise->future();
    doInvokeMethodAsync(d->m_registry, "bindSingleton",
                        QVariantList { className }, std::move(promise));
    future.waitForFinished();

    d->addObjectInstance(className, 0 /* == singleton*/, obj);
}

/*! \internal
    \fn template<typename T, typename ...ARGS> std::unique_ptr<T> SocketIpc::createInstance(ARGS && ...args)

    Asks the server to create a new instance of the IPC class associated with \c T (looked up
    by \c Q_CLASSINFO("SocketIpcClassName", ...)) using \a args as constructor arguments, and
    returns a local proxy bound to it. Blocks until the server confirms creation.

    The proxy is owned by the caller. Destroying it sends a \c destroyInstance request that
    deletes the server-side object. As with \l bindSingleton(), using a proxy after this
    \c SocketIpc instance has been destroyed throws an \c Exception rather than crashing.

    Client-side only.
*/

void SocketIpc::doCreateInstance(QObject *obj, const QByteArray &className, const QVariantList &vargs)
{
    auto promise = std::make_unique<SocketIpcPromise<uint>>(); // instance counter
    auto future = promise->future();

    QVariantList classNameAndVargs { className, vargs };

    doInvokeMethodAsync(d->m_registry, "createInstance", classNameAndVargs, std::move(promise));
    future.waitForFinished();

    Q_ASSERT(future.resultCount() == 1);
    uint instanceId = future.result();

    if (!instanceId)
        throw Exception("Received invalid instance number (0) from server");

    d->addObjectInstance(className, instanceId, obj);
}


/////////////////////////////////////////////////////////////////////////
// SocketIpcPrivate
/////////////////////////////////////////////////////////////////////////


SocketIpcPrivate::~SocketIpcPrivate()
{
    // The receiver thread is already quit and waited on by SocketIpc::~SocketIpc, so we have exclusive
    // access to m_sentInvokeMethods - no lock needed.
    // Wake up any caller still blocked on a QFuture by finishing their promise with an exception,
    // so .result() throws instead of hanging forever. Then delete the owned messages.
    for (SocketIpcMessage *sent : std::as_const(m_sentInvokeMethods)) {
        if (sent->m_promise)
            sent->m_promise->finish(false, u"IPC shutdown: request abandoned"_s);
    }
    qDeleteAll(m_sentInvokeMethods);
    m_sentInvokeMethods.clear();

    // Delete every SocketIpc-owned QObject instance. That covers server-side singletons + factory
    // instances, and the #Registry on both sides (an internal object SocketIpc allocates itself:
    // m_registry is just a borrowed handle into m_classes).
    // User-supplied client-side proxies are caller-owned; QPointer keeps them safe after SocketIpc dies.
    for (Class *cls : std::as_const(m_classes)) {
        if ((m_role == SocketIpc::Role::Server) || (cls->m_name == "#Registry"))
            qDeleteAll(cls->m_instances);
    }
    qDeleteAll(m_classes);
    m_classes.clear();
}

SocketIpcPrivate::Class *SocketIpcPrivate::findClass(const QByteArray &name) const
{
    // we need to be locked at this point
    Q_ASSERT(!m_classesMutex.tryLock());

    for (Class *cls : m_classes) {
        if (cls->m_name == name)
            return cls;
    }
    return nullptr;
}

std::pair<SocketIpcPrivate::Class *, uint> SocketIpcPrivate::findInstance(const QObject *object) const noexcept(false)
{
    // we need to be locked at this point
    Q_ASSERT(!m_classesMutex.tryLock());

    if (!object)
        return { };

    //TODO: maintain a reverse Object -> { cls, instanceId } map for performance

    for (Class *cls : m_classes) {
        for (const auto [instanceId, instance] : std::as_const(cls->m_instances).asKeyValueRange()) {
            if (instance == object)
                return { cls, instanceId };
        }
    }
    return { };
}

int SocketIpcPrivate::relaySlotMethodIndex()
{
    // this idea of a single, generic relay slot originates from QDBusAbstractAdaptor
    static const int relayIndex = [] {
        int idx = staticMetaObject.indexOfMethod("relaySlot()");
        if (idx <= 0)
            throw Exception("SocketIpcPrivate::relaySlot() method not found in metaobject");
        return idx;
    }();
    return relayIndex;
}

void SocketIpcPrivate::addSingletonClass(const QByteArray &className, std::unique_ptr<QObject> obj)
{
    // server side only

    if (!obj)
        throw Exception("Cannot register 'null' singletons");
    if (m_role != SocketIpc::Role::Server)
        throw Exception("Cannot register a singleton class on the client side");
    if (className.isEmpty())
        throw Exception("Cannot register with empty class name");

    QMutexLocker locker(&m_classesMutex);

    if (findClass(className))
        throw Exception("Class '%1' is already registered").arg(className);

    QObject *rawObj = obj.release();
    m_classes << new Class { className, /*m_isSingleton*/ true, /*m_factory*/ { },
                             /*m_instances*/ { { 0, rawObj } }, /*m_deadInstanceIds*/ { } };

    locker.unlock();

    // obj might not live on the ipc thread, so we need to use Qt::AutoConnection
    QMetaObject::connect(rawObj, -1, this, relaySlotMethodIndex(), Qt::AutoConnection);
}

void SocketIpcPrivate::addObjectInstance(const QByteArray &className, uint instanceId, QObject *obj)
{
    // server + client side

    if (!obj)
        throw Exception("Cannot register 'null' objects");
    if (className.isEmpty())
        throw Exception("Cannot register with empty class name");

    QMutexLocker locker(&m_classesMutex);

    Class *cls = findClass(className);
    if (!cls) {
        if (m_role == SocketIpc::Role::Server)
            throw Exception("Class '%1' is not registered").arg(className);

        cls = new Class { className, /*m_isSingleton*/ !instanceId, /*m_factory*/ nullptr,
                          /*m_instances*/ { }, /*m_deadInstanceIds*/ { } };
        m_classes << cls;
    }
    if (cls->m_isSingleton && !cls->m_instances.isEmpty())
        throw Exception("Cannot register singleton instance again for class '%1'").arg(className);

    if (cls->m_instances.contains(instanceId))
        throw Exception("Instance ID %1 is already registered for class '%2'").arg(instanceId).arg(className);

    cls->m_instances.insert(instanceId, obj);
    cls->m_deadInstanceIds.removeAll(instanceId);

    cls = nullptr; // make sure we don't access cls without holding a lock
    locker.unlock();

    if (m_role == SocketIpc::Role::Server) {
        // obj might not live on the ipc thread, so we need to use Qt::AutoConnection
        QMetaObject::connect(obj, -1, this, relaySlotMethodIndex(), Qt::AutoConnection);
    } else {
        QObject::connect(obj, &QObject::destroyed,
                         this, [this, className, instanceId](QObject *) {
            removeObjectInstance(className, instanceId);
            if (instanceId)
                q->invokeMethodAsync<void>(m_registry, "destroyInstance", className, instanceId);
            else
                q->invokeMethodAsync<void>(m_registry, "unbindSingleton", className);
        });
    }
}

void SocketIpcPrivate::addFactoryClass(const QByteArray &className, const std::function<Class::FactoryFunction> &factory)
{
    // server side only

    if (!factory)
        throw Exception("Cannot register 'null' factory class");
    if (m_role != SocketIpc::Role::Server)
        throw Exception("Cannot register a factory class on the client side");
    if (className.isEmpty())
        throw Exception("Cannot register with empty class name");

    QMutexLocker locker(&m_classesMutex);

    if (findClass(className))
        throw Exception("Class '%1' is already registered").arg(className);

    m_classes << new Class { className, /*m_isSingleton*/ false, /*m_factory*/ factory,
                             /*m_instances*/ { }, /*m_deadInstanceIds*/ { } };
}

QObject *SocketIpcPrivate::removeObjectInstance(const QByteArray &className, uint instanceId)
{
    if (className.isEmpty())
        return nullptr;

    QMutexLocker locker(&m_classesMutex);

    if (Class *cls = findClass(className)) {
        bool removeClass = false;

        if (bool(instanceId) == cls->m_isSingleton)
            throw Exception("Instance ID %1 is invalid for class '%2'").arg(instanceId).arg(className);

        if (cls->m_isSingleton) {
            if (m_role == SocketIpc::Role::Server)
                throw Exception("Currently singleton objects cannot be removed");
            else
                removeClass = true;
        }
        auto it = cls->m_instances.find(instanceId);
        if (it == cls->m_instances.end())
            throw Exception("Instance ID %1 of class '%2' not found").arg(instanceId).arg(className);
        QObject *obj = *it;
        cls->m_instances.erase(it);
        if (removeClass) {
            Q_ASSERT(cls->m_instances.isEmpty());
            m_classes.removeOne(cls);
            delete cls;
        }
        return obj;
    }
    return nullptr;
}

void SocketIpcPrivate::send(std::unique_ptr<SocketIpcMessage> msg)
{
    // We could be called from multiple threads in parallel and without a mutex, the message
    // counter would be messed up and not reflect the actual sending order.
    // Also, the m_sentInvokeMethods list needs to be protected.

    QMutexLocker locker(&m_sendMutex);

    if (!msg->m_counter)
        msg->m_counter = incrementMessageCounter(m_messageCounterOutgoing);

    const QByteArray packet = messageToPacket(msg.get());

    qCDebug(LogSIpc) << "Send" << roleName(m_role) << callName(msg->m_call) << msg->m_counter
                     << msg->m_className << msg->m_instanceId << msg->m_function << msg->m_arguments;

    int r = 0;
    QT_EINTR_LOOP(r, ::send(m_socket, packet.constData(), packet.size(), MSG_NOSIGNAL));

    if (r != packet.size()) {
        if ((r == -1) && ((errno == EPIPE) || (errno == ECONNRESET))) {
            // Peer closed the socket. For InvokeMethod, finish the caller's promise inline so
            // future.result() doesn't hang waiting for a reply that will never come. Replies
            // and signals are silently dropped - their counterpart on the peer is gone too.
            if ((msg->m_call == SocketIpcMessage::Call::InvokeMethod) && msg->m_promise)
                msg->m_promise->finish(false, u"IPC peer closed the connection"_s);
            return;
        }
        throw Exception(errno, "Failed to write to IPC socket (wrote %1 of %2 bytes)")
            .arg(r).arg(packet.size());
    }

    if (msg->m_call == SocketIpcMessage::Call::InvokeMethod) {
        const quint64 counter = msg->m_counter;
        m_sentInvokeMethods << msg.release();
        if (m_requestTimeoutMs > 0)
            scheduleRequestTimeout(counter);
    }
}

void SocketIpcPrivate::scheduleRequestTimeout(quint64 counter)
{
    // Called from the sender thread (the one invoking invokeMethodAsync).
    QMetaObject::invokeMethod(this, [this, counter]() {
        QTimer::singleShot(m_requestTimeoutMs, this, [this, counter]() {
            handleRequestTimeout(counter);
        });
    }, Qt::QueuedConnection);
}

void SocketIpcPrivate::handleRequestTimeout(quint64 counter)
{
    std::unique_ptr<SocketIpcMessage> sent;
    bool exceededThreshold = false;
    {
        QMutexLocker locker(&m_sendMutex);
        for (qsizetype i = 0; i < m_sentInvokeMethods.size(); ++i) {
            if (m_sentInvokeMethods.at(i)->m_counter == counter) {
                sent.reset(m_sentInvokeMethods.takeAt(i));
                break;
            }
        }
        if (!sent)
            return;  // reply arrived first; nothing to do
        ++m_consecutiveTimeouts;
        if ((m_maxConsecutiveTimeouts > 0) && (m_consecutiveTimeouts >= m_maxConsecutiveTimeouts))
            exceededThreshold = true;
    }

    if (sent->m_promise) {
        sent->m_promise->finish(false,
            QStringLiteral("IPC request timed out after %1 ms").arg(m_requestTimeoutMs));
    }

    if (exceededThreshold) {
        qFatal() << "SocketIpc: peer is stuck on the" << roleName(m_role)
                 << "side:" << m_consecutiveTimeouts << "consecutive request timeouts";
    }
}

void SocketIpcPrivate::receive()
{
    while (true) {
        QByteArray buffer;
        buffer.resize(16384);

        // MSG_DONTWAIT returns immediately with EWOULDBLOCK if there's no data to read
        int r = 0;
        QT_EINTR_LOOP(r, ::recv(m_socket, buffer.data(), buffer.size(), MSG_TRUNC | MSG_DONTWAIT));

        if (r < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                return; // nothing to receive
            if (errno != ECONNRESET)
                throw Exception(errno, "Failed to read from IPC socket");
            // ECONNRESET: peer was killed or closed with pending data still queued. Fall
            // through to the EOF handling below; from our side that is the same state.
            r = 0;
        }
        if (r == 0) {
            // Peer closed the socket (cleanly with EOF, or with a reset). Wake up any callers
            // blocked on QFutures so they don't hang, then quit the receiver thread's event
            // loop so ~SocketIpc's wait() can join.
            {
                QMutexLocker locker(&m_sendMutex);
                for (SocketIpcMessage *sent : std::as_const(m_sentInvokeMethods)) {
                    if (sent->m_promise)
                        sent->m_promise->finish(false, u"IPC peer closed the connection"_s);
                }
                qDeleteAll(m_sentInvokeMethods);
                m_sentInvokeMethods.clear();
            }
            QThread::currentThread()->quit();
            return;
        } else if (qsizetype(r) > buffer.size()) {
          throw Exception("Received message of size %1 is exceeding buffer size of %2")
              .arg(r).arg(buffer.size());
        }
        buffer.resize(r);
        try {
            auto msg = messageFromPacket(buffer);
            bool isInvokeMethod = (msg->m_call == SocketIpcMessage::Call::InvokeMethod);

            if ((m_role == SocketIpc::Role::Server) && !isInvokeMethod)
                throw Exception("The server can only receive 'InvokeMethod' calls");
            else if ((m_role == SocketIpc::Role::Client) && isInvokeMethod)
                throw Exception("The client cannot receive 'InvokeMethod' calls");

            qCDebug(LogSIpc) << "Recv" << roleName(m_role) << callName(msg->m_call)
                             << msg->m_counter << msg->m_className << msg->m_instanceId
                             << msg->m_function << msg->m_arguments;

            if (auto replyMsg = handle(msg.get()))
                send(std::move(replyMsg));
        } catch (const Exception &e) {
            // There's no point in even trying to recover from "soft" errors like "no such method".
            // In theory there cannot be any mismatch between server and client side, because they
            // are compiled into the same binary.

            qFatal() << "SocketIpcPrivate::receive failed on the" << roleName(m_role)
                     << "side:" << e.what();
        }
    }
}

std::unique_ptr<SocketIpcMessage> SocketIpcPrivate::handle(const SocketIpcMessage *msg)
{
    if (!m_receiverThread->isCurrentThread())
        throw Exception("SocketIpcPrivate::handle called from wrong thread");

    switch (msg->m_call) {
    case SocketIpcMessage::Call::InvokeMethodException:
    case SocketIpcMessage::Call::InvokeMethodResult: {
        bool isError = (msg->m_call == SocketIpcMessage::Call::InvokeMethodException);
        std::unique_ptr<SocketIpcMessage> sentMsg;

        m_sendMutex.lock();

        for (qsizetype i = 0; i < m_sentInvokeMethods.size(); ++i) {
            if (m_sentInvokeMethods.at(i)->m_counter == msg->m_counter) {
                sentMsg.reset(m_sentInvokeMethods.takeAt(i));
                break;
            }
        }
        m_sendMutex.unlock();

        if (!sentMsg) {
            if (m_requestTimeoutMs > 0) {
                qCWarning(LogSIpc) << "Late reply for already-timed-out request (counter"
                                   << msg->m_counter << ") - discarding";
                return { };
            }
            throw Exception("No matching request for reply with message counter %1")
                .arg(msg->m_counter);
        }

        if (msg->m_arguments.size() != 1)
            throw Exception("Invalid argument count in reply message");

        const QVariant &v = msg->m_arguments.at(0);

        if (sentMsg->m_promise) {
            sentMsg->m_promise->finish(!isError, v);
            m_consecutiveTimeouts = 0; // a successful exchange -> clear counter

        } else {
            throw Exception("No matching promise for reply with message counter %1")
                .arg(msg->m_counter);
        }
        return { };
    }
    case SocketIpcMessage::Call::EmitSignal:
    case SocketIpcMessage::Call::InvokeMethod: {
        if (msg->m_counter != incrementMessageCounter(m_messageCounterIncoming)) {
            throw Exception("Incoming message counter out of order: expected %1, got %2")
                .arg(m_messageCounterIncoming).arg(msg->m_counter);
        }

        // Audit: log every structurally legitimate invoke call on the server side
        if ((m_role == SocketIpc::Role::Server) && (msg->m_call == SocketIpcMessage::Call::InvokeMethod)) {
            qCInfo(LogSIpc).nospace() << "Invoke #" << msg->m_counter << ' '
                                      << msg->m_className.constData() << '@'
                                      << msg->m_instanceId << "."
                                      << msg->m_function.constData() << "("
                                      << msg->m_arguments << ")";
        }

        QMutexLocker locker(&m_classesMutex);

        Class *cls = findClass(msg->m_className);
        if (msg->m_call == SocketIpcMessage::Call::EmitSignal) {
            // We are receiving a signal in the client. Ignore if the class/instance is not known.
            if (!cls || !cls->m_instances.value(msg->m_instanceId)) {
                qCWarning(LogSIpc) << "Ignoring signal for unknown class/instance"
                                   << msg->m_className << msg->m_instanceId;
                return { };
            }
        }
        if (!cls)
            throw Exception("No such class: %1").arg(msg->m_className);
        if (!cls->m_instances.contains(msg->m_instanceId))
            throw Exception("No such instance: %1").arg(msg->m_className);
        if (cls->m_isSingleton && (msg->m_instanceId != 0)) {
            throw Exception("Class %1 is a singleton, instance ID must be 0, not %2")
                .arg(msg->m_className).arg(msg->m_instanceId);
        }

        QObject *obj = cls->m_instances.value(msg->m_instanceId);
        if (!obj) {
            throw Exception("No such object instance: class %1, instance ID %2")
                .arg(msg->m_className).arg(msg->m_instanceId);
        }

        cls = nullptr; // make sure we don't access cls without holding a lock
        locker.unlock();

        const QMetaObject *mo = obj->metaObject();
        QMetaMethod mm;
        for (int i = QObject::staticMetaObject.methodCount(); i < mo->methodCount(); ++i) {
            auto mmi = mo->method(i);
            if (mmi.name() == msg->m_function) {
                mm = mmi;
                break;
            }
        }

        if (!mm.isValid())
            throw Exception("No such method %1 in class %2").arg(msg->m_function).arg(msg->m_className);

        bool isSignal = (msg->m_call == SocketIpcMessage::Call::EmitSignal);

        if (isSignal && (mm.methodType() != QMetaMethod::Signal)) {
            throw Exception("Function %1 in class %2 is not a signal")
                .arg(msg->m_function).arg(msg->m_className);
        } else if (!isSignal && (mm.methodType() != QMetaMethod::Method) && (mm.methodType() != QMetaMethod::Slot)) {
            throw Exception("Function %1 in class %2 is not a method or slot")
                .arg(msg->m_function).arg(msg->m_className);
        }
        if (mm.parameterCount() != msg->m_arguments.size()) {
            throw Exception("Argument count mismatch for function %1: expected %2, got %3")
                .arg(msg->m_function).arg(mm.parameterCount()).arg(msg->m_arguments.size());
        }
        for (int i = 0; i < mm.parameterCount(); ++i) {
            if (mm.parameterType(i) != msg->m_arguments.at(i).metaType().id()) {
                throw Exception("Argument type mismatch for function %1, argument %2: expected %3, got %4")
                    .arg(msg->m_function).arg(i).arg(mm.parameterTypeName(i))
                    .arg(msg->m_arguments.at(i).metaType().name());
            }
        }

        std::vector<void *> argv(mm.parameterCount() + 1);
        for (int i = 0; i < mm.parameterCount(); ++i)
            argv[i + 1] = const_cast<void *>(msg->m_arguments.at(i).constData());

        if (isSignal) {
            argv[0] = nullptr;
            QMetaObject::activate(obj, mm.methodIndex(), argv.data());
            return { };
        } else {
            QMetaType returnMetaType = mm.returnMetaType();
            argv[0] = returnMetaType.create();

            std::unique_ptr<SocketIpcMessage> reply;
            try {
                QMetaObject::metacall(obj, QMetaObject::InvokeMetaMethod, mm.methodIndex(), argv.data());
                QVariant result = QVariant(mm.returnMetaType(), argv[0]);

                reply = std::make_unique<SocketIpcMessage>(SocketIpcMessage::Call::InvokeMethodResult,
                                                           msg->m_counter, msg->m_className,
                                                           msg->m_instanceId, msg->m_function,
                                                           QVariantList { result });
            } catch (const Exception &e) {
                qCWarning(LogSIpc).nospace() << "Invoke #" << msg->m_counter << " FAILED: "
                                              << e.what();

                reply = std::make_unique<SocketIpcMessage>(SocketIpcMessage::Call::InvokeMethodException,
                                                           msg->m_counter, msg->m_className,
                                                           msg->m_instanceId, msg->m_function,
                                                           QVariantList { e.errorString() });
            }
            mm.returnMetaType().destroy(argv[0]);
            return reply;
        }
    }
    }
    return { };
}

void SocketIpcPrivate::relaySlot(QMethodRawArguments argv)
{
    // With Qt::AutoConnection on the relay, the slot is always invoked on this object's thread:
    // directly if sender lives on the same thread, queued otherwise. That is also exactly the
    // condition under which sender() / senderSignalIndex() are valid.
    Q_ASSERT(thread()->isCurrentThread());

    try {
        QObject *object = sender();
        if (!object)
            throw Exception("No sender object for relayed signal");

        QMetaMethod mm = object->metaObject()->method(senderSignalIndex());
        if (!mm.isValid())
            throw Exception("Relayed signal number does not match sender object");

        QMutexLocker locker(&m_classesMutex);

        const QByteArray function = mm.name();
        auto [cls, instanceId] = findInstance(object);

        if (!cls) // already being destroyed
            return;

        QByteArray className = cls->m_name;

        cls = nullptr; // make sure we don't access cls without holding a lock
        locker.unlock();

        QVariantList args;
        for (int i = 0; i < mm.parameterCount(); ++i)
            args << QVariant(mm.parameterMetaType(i), argv.arguments[i + 1]);

        auto msg = std::make_unique<SocketIpcMessage>(SocketIpcMessage::Call::EmitSignal, 0,
                                                      className, instanceId, function, args);
        send(std::move(msg));
    } catch (const Exception &e) {
        qFatal() << "IPC failed on server side during signal emission:" << e.what();
    }
}

QByteArray SocketIpcPrivate::messageToPacket(SocketIpcMessage *msg) const
{
    QByteArray packet;
    QDataStream ds(&packet, QIODevice::WriteOnly);

    ds << msg->m_counter
       << quint8(msg->m_call)
       << msg->m_className
       << msg->m_instanceId
       << msg->m_function
       << quint32(msg->m_arguments.size());
    for (const auto &arg : std::as_const(msg->m_arguments))
        ds << arg;
    return packet;
}

std::unique_ptr<SocketIpcMessage> SocketIpcPrivate::messageFromPacket(const QByteArray &packet) noexcept(false)
{
    auto msg = std::make_unique<SocketIpcMessage>();

    if (packet.size() < (8 + 1 + 4 + 4 + 4 + 4)) // see below for the data-types
        throw Exception("Message packet too small");

    QDataStream ds(packet);
    quint64 counter = 0;
    quint8 call = 0;
    QByteArray className;
    uint instanceId = 0;
    QByteArray function;
    quint32 argCount = 0;
    ds >> counter >> call >> className >> instanceId >> function >> argCount;
    if (ds.status() != QDataStream::Ok)
        throw Exception("Failed to decode message header");

    QVariantList arguments;
    for (quint32 i = 0; i < argCount; ++i) {
        QVariant arg;
        ds >> arg;
        if (ds.status() != QDataStream::Ok)
            throw Exception("Failed to decode message argument %1").arg(i);
        arguments.append(arg);
    }
    if (!ds.atEnd())
        throw Exception("Extra data at end of message packet");

    return std::make_unique<SocketIpcMessage>(static_cast<SocketIpcMessage::Call>(call), counter,
                                              className, instanceId, function, arguments);
}


/////////////////////////////////////////////////////////////////////////
// SocketIpcMessage
/////////////////////////////////////////////////////////////////////////


SocketIpcMessage::SocketIpcMessage(SocketIpcMessage::Call call, quint64 counter,
                                   const QByteArray &className, uint instanceId,
                                   const QByteArray &function, const QVariantList &args,
                                   std::unique_ptr<SocketIpcPromiseBase> promise)
    : m_counter(counter)
    , m_call(call)
    , m_className(className)
    , m_instanceId(instanceId)
    , m_function(function)
    , m_arguments(args)
    , m_promise(std::move(promise))
{ }


/////////////////////////////////////////////////////////////////////////
// SocketIpcConfiguration
/////////////////////////////////////////////////////////////////////////


/*! \class SocketIpcConfiguration
    \internal

    \brief Bundles the connection parameters needed to construct a \l SocketIpc instance: a pair of
    connected sockets (owned) and the protocol's per-direction starting sequence numbers.

    The configuration owns both file descriptors it holds and closes them on destruction. A
    \c SocketIpc constructor adopts one of the two and the configuration closes the other. The
    normal entry point is \l createSocketPair(); the raw-fd constructor exists for cases like
    test harnesses.
*/

/*! \internal

    Creates a connected \c AF_UNIX / \c SOCK_SEQPACKET socket pair with \c SOCK_CLOEXEC set,
    and returns a \c SocketIpcConfiguration that owns both ends. Throws if \c socketpair(2) fails.
*/
SocketIpcConfiguration SocketIpcConfiguration::createSocketPair()
{
    int s[2];
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, s) < 0)
        throw Exception(errno, "Failed to create socketpair for IPC");
    return SocketIpcConfiguration(s[0], s[1]);
}

/*! \internal

    Adopts the given pair of pre-existing file descriptors. The configuration takes ownership
    of both and will close them on destruction (or when transferred into a \l SocketIpc instance).
    The starting sequence numbers are still seeded from a cryptographically secure random
    source - predictable counters would defeat the protocol's blind-injection defense.
*/
SocketIpcConfiguration::SocketIpcConfiguration(int serverSocket, int clientSocket)
    : d(std::make_unique<SocketIpcConfigurationPrivate>())
{
    d->m_serverSocket = serverSocket;
    d->m_clientSocket = clientSocket;
    auto *rng = QRandomGenerator::system();
    d->m_messageCounterStartServer = rng->generate64();
    d->m_messageCounterStartClient = rng->generate64();
}

SocketIpcConfiguration::SocketIpcConfiguration(SocketIpcConfiguration &&other) noexcept
    : d(std::move(other.d))
{
    other.d = std::make_unique<SocketIpcConfigurationPrivate>();
}

/*! \internal

    Closes any still-owned file descriptors.
*/
SocketIpcConfiguration::~SocketIpcConfiguration()
{
    if (d)
        clear();
}

SocketIpcConfiguration &SocketIpcConfiguration::operator=(SocketIpcConfiguration &&other) noexcept
{
    if (this != &other) {
        clear();
        d = std::move(other.d);
        other.d = std::make_unique<SocketIpcConfigurationPrivate>();
    }
    return *this;
}


/*! \internal

    Resets the configuration to its empty state: counters zeroed, owned file descriptors
    closed and set to \c -1. Called automatically on move and on destruction.
*/
void SocketIpcConfiguration::clear()
{
    d->m_messageCounterStartServer = d->m_messageCounterStartClient = 0;

    if (d->m_serverSocket != -1) {
        ::close(d->m_serverSocket);
        d->m_serverSocket = -1;
    }
    if (d->m_clientSocket != -1) {
        ::close(d->m_clientSocket);
        d->m_clientSocket = -1;
    }
}


/////////////////////////////////////////////////////////////////////////
// SocketIpcRegistry
/////////////////////////////////////////////////////////////////////////


SocketIpcRegistry::SocketIpcRegistry(SocketIpc *ipc, SocketIpcPrivate *ipcPrivate)
    : q(ipc)
    , d(ipcPrivate)
{ }

uint SocketIpcRegistry::createInstance(const QByteArray &className, const QVariantList &vargs)
{
    QMutexLocker locker(&d->m_classesMutex);

    SocketIpcPrivate::Class *cls = d->findClass(className);
    if (!cls)
        throw Exception("Unknown class name '%1'").arg(className);
    if (!cls->m_factory)
        throw Exception("Class '%1' does not support object creation").arg(className);

    uint instanceId = cls->getNextInstanceId();
    auto factory = cls->m_factory;

    cls = nullptr; // make sure we don't access cls without holding a lock
    locker.unlock(); // factory() might call back into the registry, so we need to unlock

    std::unique_ptr<QObject> instance = factory(q, className, vargs);

    if (!instance)
        throw Exception("Failed to create object of class '%1'").arg(className);

    // This could throw, so we have to hold off on releasing instance
    d->addObjectInstance(className, instanceId, instance.get());
    (void) instance.release(); // NOLINT(bugprone-unused-return-value)
    return instanceId;
}

void SocketIpcRegistry::destroyInstance(const QByteArray &className, uint instanceId)
{
    QMutexLocker locker(&d->m_classesMutex);

    SocketIpcPrivate::Class *cls = d->findClass(className);
    if (!cls)
        throw Exception("Unknown class name '%1'").arg(className);
    if (cls->m_isSingleton)
        throw Exception("Cannot destroy singleton class '%1'").arg(className);
    if (!cls->m_instances.contains(instanceId))
        throw Exception("Instance '%1' is not known").arg(className);

    cls->releaseInstanceId(instanceId);

    cls = nullptr; // make sure we don't access cls without holding a lock
    locker.unlock();

    delete d->removeObjectInstance(className, instanceId);
}

void SocketIpcRegistry::bindSingleton(const QByteArray &className)
{
    QMutexLocker locker(&d->m_classesMutex);

    SocketIpcPrivate::Class *cls = d->findClass(className);
    if (!cls)
        throw Exception("Unknown class name '%1'").arg(className);
    if (!cls->m_isSingleton)
        throw Exception("Class '%1' is not a singleton").arg(className);
}

void SocketIpcRegistry::unbindSingleton(const QByteArray &className)
{
    QMutexLocker locker(&d->m_classesMutex);

    SocketIpcPrivate::Class *cls = d->findClass(className);
    if (!cls)
        throw Exception("Unknown class name '%1'").arg(className);
    if (!cls->m_isSingleton)
        throw Exception("Class '%1' is not a singleton").arg(className);
}

QT_END_NAMESPACE_AM

#include "moc_socketipc_p.cpp"
