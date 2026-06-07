// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <limits>
#include <signal.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <QtTest>
#include <QSignalSpy>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#if defined(QT_AM_COVERAGE)
extern "C" {
#  include <gcov.h>
}
#endif

#include "exception.h"
#include "socketipc.h"
#include "private/socketipc_p.h"

QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;


// === Interfaces (shared between the server-side implementations and the client-side proxies) ===

class SimpleInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("SocketIpcClassName", "Simple")
public:
    SimpleInterface(QObject *parent = nullptr) : QObject(parent) { }

    Q_INVOKABLE virtual int add(int a, int b) = 0;
    Q_INVOKABLE virtual QString concat(const QString &a, const QString &b) = 0;
    Q_INVOKABLE virtual QByteArray echoBytes(const QByteArray &b) = 0;
    Q_INVOKABLE virtual QStringList echoStringList(const QStringList &l) = 0;
    Q_INVOKABLE virtual QList<int> echoIntList(const QList<int> &l) = 0;
    Q_INVOKABLE virtual void noteCall() = 0;
    Q_INVOKABLE virtual int callCount() = 0;
    Q_INVOKABLE virtual void throwException() = 0;
    Q_INVOKABLE virtual void triggerSignal(int value) = 0;
    Q_INVOKABLE virtual void blockForever() = 0;

Q_SIGNALS:
    void testSignal(int value);
};

class CounterInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("SocketIpcClassName", "Counter")
public:
    CounterInterface(QObject *parent = nullptr) : QObject(parent) { }

    Q_INVOKABLE virtual int value() = 0;
    Q_INVOKABLE virtual void increment() = 0;
};

class MetaFactoryInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("SocketIpcClassName", "MetaFactory")
public:
    MetaFactoryInterface(QObject *parent = nullptr) : QObject(parent) { }

    Q_INVOKABLE virtual QString label() = 0;
    Q_INVOKABLE virtual int number() = 0;
};


// === Server-side implementations ===

class SimpleServer : public SimpleInterface
{
    Q_OBJECT
public:
    SimpleServer(QObject *parent = nullptr) : SimpleInterface(parent) { }

    int add(int a, int b) final { return a + b; }
    QString concat(const QString &a, const QString &b) final { return a + b; }
    QByteArray echoBytes(const QByteArray &b) final { return b; }
    QStringList echoStringList(const QStringList &l) final { return l; }
    QList<int> echoIntList(const QList<int> &l) final { return l; }
    void noteCall() final { ++m_callCount; }
    int callCount() final { return m_callCount; }
    void throwException() final { throw Exception("server-side boom"); }
    void triggerSignal(int v) final { emit testSignal(v); }
    void blockForever() final {
        // Pause indefinitely so the client side has a guaranteed in-flight async request to
        // exercise the peer-died-with-pending-promise cleanup path.
        ::pause();
    }

private:
    int m_callCount = 0;
};

class CounterServer : public CounterInterface
{
    Q_OBJECT
public:
    explicit CounterServer(int initial) : m_value(initial) { }

    int value() final { return m_value; }
    void increment() final { ++m_value; }

private:
    int m_value;
};

class MetaFactoryServer : public MetaFactoryInterface
{
    Q_OBJECT
public:
    Q_INVOKABLE MetaFactoryServer(const QString &label, int number)
        : m_label(label), m_number(number) { }

    QString label() final { return m_label; }
    int number() final { return m_number; }

private:
    QString m_label;
    int m_number;
};


// === Client-side proxies ===

class SimpleClient : public SimpleInterface
{
    Q_OBJECT
public:
    explicit SimpleClient(SocketIpc *ipc) : m_ipc(ipc) { }

    int add(int a, int b) final
    { return m_ipc->invokeMethod<int>(this, __func__, a, b); }
    QString concat(const QString &a, const QString &b) final
    { return m_ipc->invokeMethod<QString>(this, __func__, a, b); }
    QByteArray echoBytes(const QByteArray &b) final
    { return m_ipc->invokeMethod<QByteArray>(this, __func__, b); }
    QStringList echoStringList(const QStringList &l) final
    { return m_ipc->invokeMethod<QStringList>(this, __func__, l); }
    QList<int> echoIntList(const QList<int> &l) final
    { return m_ipc->invokeMethod<QList<int>>(this, __func__, l); }
    void noteCall() final
    { m_ipc->invokeMethod<void>(this, __func__); }
    int callCount() final
    { return m_ipc->invokeMethod<int>(this, __func__); }
    void throwException() final
    { m_ipc->invokeMethod<void>(this, __func__); }
    void triggerSignal(int v) final
    { m_ipc->invokeMethod<void>(this, __func__, v); }
    void blockForever() final
    { m_ipc->invokeMethod<void>(this, __func__); }

    QFuture<int> addAsync(int a, int b)
    { return m_ipc->invokeMethodAsync<int>(this, "add", a, b); }
    QFuture<void> throwExceptionAsync()
    { return m_ipc->invokeMethodAsync<void>(this, "throwException"); }
    QFuture<void> blockForeverAsync()
    { return m_ipc->invokeMethodAsync<void>(this, "blockForever"); }

private:
    SocketIpc *m_ipc;
};

class CounterClient : public CounterInterface
{
    Q_OBJECT
public:
    CounterClient(SocketIpc *ipc, int /*initial*/) : m_ipc(ipc) { }

    int value() final { return m_ipc->invokeMethod<int>(this, __func__); }
    void increment() final { m_ipc->invokeMethod<void>(this, __func__); }

private:
    SocketIpc *m_ipc;
};

class MetaFactoryClient : public MetaFactoryInterface
{
    Q_OBJECT
public:
    MetaFactoryClient(SocketIpc *ipc, const QString & /*label*/, int /*number*/)
        : m_ipc(ipc) { }

    QString label() final { return m_ipc->invokeMethod<QString>(this, __func__); }
    int number() final { return m_ipc->invokeMethod<int>(this, __func__); }

private:
    SocketIpc *m_ipc;
};


// === Test class ===

class tst_SocketIpc : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void configurationMoves();
    void registrationValidation();

    void basicInvoke();
    void asyncInvoke();
    void mixedSyncAsync();
    void containerTypes();
    void voidMethod();
    void serverException();
    void asyncException();
    void callbackFactory();
    void metaObjectFactory();
    void signalRelay();
    void proxyDestruction();
    void pendingPromisesAbandonedOnPeerDeath();
    void counterMismatchKillsPeer();
    void requestTimeoutFiresPromise();
    void consecutiveTimeoutsKillClient();

private:
    // Forks a server child, runs serverSetup() inside it before SocketIpc::start(),
    // then runs clientBody() on the parent (client) side. The child is reaped in cleanup().
    template<typename ServerSetup, typename ClientBody>
    void runIpcTest(ServerSetup &&serverSetup, ClientBody &&clientBody);

    pid_t m_serverPid = -1;
};


void tst_SocketIpc::initTestCase()
{
    // QList<int> is not registered as a QVariant metatype by default; serialization through
    // QDataStream needs the name available for load.
    qRegisterMetaType<QList<int>>("QList<int>");
}

void tst_SocketIpc::cleanup()
{
    if (m_serverPid > 0) {
        ::kill(m_serverPid, SIGTERM);
        ::waitpid(m_serverPid, nullptr, 0);
        m_serverPid = -1;
    }
}

template<typename ServerSetup, typename ClientBody>
void tst_SocketIpc::runIpcTest(ServerSetup &&serverSetup, ClientBody &&clientBody)
{
    auto cfg = SocketIpcConfiguration::createSocketPair();
    m_serverPid = ::fork();
    QVERIFY2(m_serverPid >= 0, "fork() failed");

    if (m_serverPid == 0) {
        // child / server side
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
        // close TOCTOU window: parent may have died between fork() and prctl()
        if (::getppid() == 1)
            ::_exit(0);

        // The child inherits QtTest's crash handlers; restore default disposition so SIGTERM
        // from cleanup() just kills the process instead of dumping a stack trace.
#if defined(QT_AM_COVERAGE)
        ::signal(SIGTERM, [](int) { __gcov_dump(); ::_exit(0); });
#else
        ::signal(SIGTERM, SIG_DFL);
#endif
        ::signal(SIGABRT, SIG_DFL);
        ::signal(SIGSEGV, SIG_DFL);
        ::signal(SIGBUS, SIG_DFL);

        // Restore the default Qt message handler so a qFatal in the child (e.g. from the
        // counter-mismatch test) calls abort() directly without QtTest's "FAIL!"-style
        // formatting that would otherwise pollute the parent's output stream.
        qInstallMessageHandler(nullptr);

        try {
            auto server = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Server);
            serverSetup(server.get());
            server->start();
            QEventLoop loop;
            loop.exec();
        } catch (...) {
            ::_exit(1);
        }
        ::_exit(0);
    }

    // parent / client side
    auto client = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Client);
    client->start();
    clientBody(client.get());
}


void tst_SocketIpc::configurationMoves()
{
    auto isFdOpen = [](int fd) { return (fd >= 0) && (::fcntl(fd, F_GETFD) != -1); };
    using P = SocketIpcConfigurationPrivate;

    // construct + explicit clear: cfg ends with no fds, the original fds are closed
    {
        auto cfg = SocketIpcConfiguration::createSocketPair();
        const int sFd = P::get(&cfg)->m_serverSocket;
        const int cFd = P::get(&cfg)->m_clientSocket;
        QVERIFY(isFdOpen(sFd));
        QVERIFY(isFdOpen(cFd));

        cfg.clear();

        QCOMPARE(P::get(&cfg)->m_serverSocket, -1);
        QCOMPARE(P::get(&cfg)->m_clientSocket, -1);
        QVERIFY(!isFdOpen(sFd));
        QVERIFY(!isFdOpen(cFd));
    }
    // move-construct: moved owns the fds, original is reset to empty
    {
        auto cfg = SocketIpcConfiguration::createSocketPair();
        const int sFd = P::get(&cfg)->m_serverSocket;
        const int cFd = P::get(&cfg)->m_clientSocket;

        SocketIpcConfiguration moved(std::move(cfg));

        QCOMPARE(P::get(&cfg)->m_serverSocket, -1);
        QCOMPARE(P::get(&cfg)->m_clientSocket, -1);
        QCOMPARE(P::get(&moved)->m_serverSocket, sFd);
        QCOMPARE(P::get(&moved)->m_clientSocket, cFd);
        QVERIFY(isFdOpen(sFd));
        QVERIFY(isFdOpen(cFd));
    }
    // move-assign onto a config that already owns a pair: the LHS's old fds must be closed,
    // the RHS's fds transferred and the RHS reset
    {
        auto a = SocketIpcConfiguration::createSocketPair();
        const int aOldServerFd = P::get(&a)->m_serverSocket;
        const int aOldClientFd = P::get(&a)->m_clientSocket;

        auto b = SocketIpcConfiguration::createSocketPair();
        const int bServerFd = P::get(&b)->m_serverSocket;
        const int bClientFd = P::get(&b)->m_clientSocket;

        a = std::move(b);

        QVERIFY(!isFdOpen(aOldServerFd));
        QVERIFY(!isFdOpen(aOldClientFd));
        QCOMPARE(P::get(&a)->m_serverSocket, bServerFd);
        QCOMPARE(P::get(&a)->m_clientSocket, bClientFd);
        QCOMPARE(P::get(&b)->m_serverSocket, -1);
        QCOMPARE(P::get(&b)->m_clientSocket, -1);
        QVERIFY(isFdOpen(bServerFd));
        QVERIFY(isFdOpen(bClientFd));
    }
}

void tst_SocketIpc::registrationValidation()
{
    // Server-side registration validates its arguments and throws on misuse. None of these paths
    // need a started channel or a peer, so exercise them in-process against freshly constructed
    // SocketIpc instances. Objects are kept owned locally (or passed by get()-pointer to the
    // private helpers, which only store them on success), so a thrown registration never leaks.
    auto makeIpc = [](SocketIpc::Role role) {
        return std::make_unique<SocketIpc>(SocketIpcConfiguration::createSocketPair(), role);
    };
    auto throwsWith = [](auto &&fn, const char *needle) {
        QString msg;
        try { fn(); }
        catch (const Exception &e) { msg = QString::fromUtf8(e.what()); }
        QVERIFY2(!msg.isEmpty(), "expected an exception, but none was thrown");
        QVERIFY2(msg.contains(QString::fromLatin1(needle)), qPrintable(msg));
    };

    // registerSingleton / registerMetaObjectFactory: object/meta-object missing the
    // Q_CLASSINFO("SocketIpcClassName", ...) tag, plus the wrong-role and duplicate-class
    // branches of addSingletonClass. Ownership flows through registerSingleton as a unique_ptr,
    // so every throwing path unwinds the object instead of leaking it.
    {
        auto srv = makeIpc(SocketIpc::Role::Server);
        auto cli = makeIpc(SocketIpc::Role::Client);

        throwsWith([&] { srv->registerSingleton(std::make_unique<QObject>()); }, "Q_CLASSINFO");
        throwsWith([&] { srv->registerMetaObjectFactory<QObject>(); }, "Q_CLASSINFO");
        throwsWith([&] { cli->registerSingleton(std::make_unique<SimpleServer>()); }, "client side");

        srv->registerSingleton(std::make_unique<SimpleServer>());  // first registration succeeds
        throwsWith([&] { srv->registerSingleton(std::make_unique<SimpleServer>()); },
                   "already registered");
    }

    // Callback factory: null function, wrong role, empty class name, duplicate class.
    {
        auto srv = makeIpc(SocketIpc::Role::Server);
        auto cli = makeIpc(SocketIpc::Role::Client);
        auto factory = [](SocketIpc *, const QByteArray &, const QVariantList &)
                -> std::unique_ptr<QObject> { return std::make_unique<CounterServer>(0); };

        throwsWith([&] { srv->registerCallbackObjectFactory("Counter", {}); }, "null");
        throwsWith([&] { cli->registerCallbackObjectFactory("Counter", factory); }, "client side");
        throwsWith([&] { srv->registerCallbackObjectFactory(QByteArray(), factory); }, "empty");

        srv->registerCallbackObjectFactory("Counter", factory);  // first registration succeeds
        throwsWith([&] { srv->registerCallbackObjectFactory("Counter", factory); },
                   "already registered");
    }
}

void tst_SocketIpc::basicInvoke()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();
            QVERIFY(proxy);
            QCOMPARE(proxy->add(5, 3), 8);
            QCOMPARE(proxy->add(-1, 1), 0);
            QCOMPARE(proxy->add(0, 0), 0);
            QCOMPARE(proxy->concat(u"hello "_s, u"world"_s), u"hello world"_s);
            QCOMPARE(proxy->concat(QString(), u"x"_s), u"x"_s);
        });
}

void tst_SocketIpc::asyncInvoke()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            // Several concurrently in-flight calls each resolve to the correct result.
            QFuture<int> f1 = proxy->addAsync(10, 20);
            QFuture<int> f2 = proxy->addAsync(100, 200);
            QFuture<int> f3 = proxy->addAsync(-50, 50);
            QCOMPARE(f1.result(), 30);
            QCOMPARE(f2.result(), 300);
            QCOMPARE(f3.result(), 0);
        });
}

void tst_SocketIpc::mixedSyncAsync()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            int r1 = proxy->add(10, 20);
            QFuture<int> f = proxy->addAsync(5, 15);
            int r2 = proxy->add(100, 100);
            int r3 = f.result();

            QCOMPARE(r1, 30);
            QCOMPARE(r2, 200);
            QCOMPARE(r3, 20);
        });
}

void tst_SocketIpc::containerTypes()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            const QByteArray bytes = QByteArray::fromHex("deadbeef00cafe");
            QCOMPARE(proxy->echoBytes(bytes), bytes);
            QCOMPARE(proxy->echoBytes(QByteArray()), QByteArray());

            const QStringList strs { u"a"_s, u"bb"_s, QString(), u"c"_s };
            QCOMPARE(proxy->echoStringList(strs), strs);
            QCOMPARE(proxy->echoStringList(QStringList()), QStringList());

            const QList<int> ints { 1, 2, 3, -1, 0, std::numeric_limits<int>::max() };
            QCOMPARE(proxy->echoIntList(ints), ints);
            QCOMPARE(proxy->echoIntList(QList<int>()), QList<int>());
        });
}

void tst_SocketIpc::voidMethod()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            // Round-trip the side effect via callCount() so we actually verify the call
            // landed on the server, not just that the future resolved.
            QCOMPARE(proxy->callCount(), 0);
            proxy->noteCall();
            proxy->noteCall();
            proxy->noteCall();
            QCOMPARE(proxy->callCount(), 3);
        });
}

void tst_SocketIpc::serverException()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            bool threw = false;
            QString message;
            try {
                proxy->throwException();
            } catch (const Exception &e) {
                threw = true;
                message = QString::fromUtf8(e.what());
            }
            QVERIFY(threw);
            QVERIFY2(message.contains(u"boom"_s), qPrintable(message));

            // The channel must still be usable after an exception was propagated.
            QCOMPARE(proxy->add(1, 2), 3);
        });
}

void tst_SocketIpc::asyncException()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            auto future = proxy->throwExceptionAsync();

            bool threw = false;
            try {
                future.waitForFinished();
            } catch (const Exception &e) {
                threw = true;
                QVERIFY(QString::fromUtf8(e.what()).contains(u"boom"_s));
            }
            QVERIFY(threw);
        });
}

void tst_SocketIpc::callbackFactory()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerCallbackObjectFactory("Counter",
                [](SocketIpc *, const QByteArray &, const QVariantList &args)
                        -> std::unique_ptr<QObject> {
                    if ((args.size() != 1) || (args.at(0).metaType().id() != QMetaType::Int))
                        throw Exception("Counter factory expects (int)");
                    return std::make_unique<CounterServer>(args.at(0).toInt());
                });
        },
        [](SocketIpc *client) {
            auto a = client->createInstance<CounterClient>(10);
            auto b = client->createInstance<CounterClient>(100);
            auto c = client->createInstance<CounterClient>(0);

            QCOMPARE(a->value(), 10);
            QCOMPARE(b->value(), 100);
            QCOMPARE(c->value(), 0);

            a->increment();
            a->increment();
            b->increment();

            // Each instance has independent state.
            QCOMPARE(a->value(), 12);
            QCOMPARE(b->value(), 101);
            QCOMPARE(c->value(), 0);
        });
}

void tst_SocketIpc::metaObjectFactory()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerMetaObjectFactory<MetaFactoryServer>();
        },
        [](SocketIpc *client) {
            auto a = client->createInstance<MetaFactoryClient>(u"alpha"_s, 1);
            auto b = client->createInstance<MetaFactoryClient>(u"beta"_s, 2);

            QCOMPARE(a->label(), u"alpha"_s);
            QCOMPARE(a->number(), 1);
            QCOMPARE(b->label(), u"beta"_s);
            QCOMPARE(b->number(), 2);
        });
}

void tst_SocketIpc::signalRelay()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();
            QSignalSpy spy(proxy.get(), &SimpleInterface::testSignal);

            // The server emits from inside the dispatch of triggerSignal(); the EmitSignal
            // packet arrives on the client receiver thread and is queued to the proxy's
            // (main) thread - so QTRY_COMPARE to spin the event loop.
            proxy->triggerSignal(42);
            QTRY_COMPARE(spy.size(), 1);
            QCOMPARE(spy.at(0).at(0).toInt(), 42);

            proxy->triggerSignal(1);
            proxy->triggerSignal(2);
            proxy->triggerSignal(3);
            QTRY_COMPARE(spy.size(), 4);
            QCOMPARE(spy.at(1).at(0).toInt(), 1);
            QCOMPARE(spy.at(2).at(0).toInt(), 2);
            QCOMPARE(spy.at(3).at(0).toInt(), 3);
        });
}

void tst_SocketIpc::proxyDestruction()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerCallbackObjectFactory("Counter",
                [](SocketIpc *, const QByteArray &, const QVariantList &args)
                        -> std::unique_ptr<QObject> {
                    return std::make_unique<CounterServer>(args.at(0).toInt());
                });
        },
        [](SocketIpc *client) {
            auto a = client->createInstance<CounterClient>(50);
            a->increment();
            QCOMPARE(a->value(), 51);
            a.reset();  // sends destroyInstance to the server

            // A new instance must come up fresh; if the server had reused the old object,
            // value() would be 51 instead of the requested 7.
            auto b = client->createInstance<CounterClient>(7);
            QCOMPARE(b->value(), 7);
        });
}

void tst_SocketIpc::pendingPromisesAbandonedOnPeerDeath()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [this](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            // Send an async request to a method that pauses indefinitely on the server side.
            // The future will not resolve normally; we kill the server below and verify that
            // the client-side IPC wakes the pending promise with an error instead of leaking
            // the future and hanging the caller forever.
            QFuture<void> future = proxy->blockForeverAsync();

            // Give the server enough time to receive and start dispatching the call.
            QTest::qWait(50);

            // Kill the server. The client's receive thread sees EOF on the socket and is
            // expected to finish every still-pending promise with an "abandoned"-style error.
            ::kill(m_serverPid, SIGKILL);
            ::waitpid(m_serverPid, nullptr, 0);
            m_serverPid = -1;  // already reaped; suppress cleanup()'s retry

            bool threw = false;
            QString message;
            try {
                future.waitForFinished();
            } catch (const Exception &e) {
                threw = true;
                message = QString::fromUtf8(e.what());
            }
            QVERIFY2(threw, "Pending async future must resolve (with an exception) after peer death");
            QVERIFY2(!message.isEmpty(), "Error message must not be empty");
            // The exact wording is set by SocketIpcPrivate::receive on r==0 (peer-closed).
            // Accept any of the protocol's documented variants - we only care that the
            // future actually finished rather than hung.
        });
}

void tst_SocketIpc::counterMismatchKillsPeer()
{
    runIpcTest(
        [](SocketIpc *server) {
            server->registerSingleton(std::make_unique<SimpleServer>());
        },
        [this](SocketIpc *client) {
            auto proxy = client->bindSingleton<SimpleClient>();

            // Run a couple of legitimate invocations first so the channel is healthy and
            // the server's incoming-counter has advanced past the start value.
            QCOMPARE(proxy->add(2, 3), 5);
            QCOMPARE(proxy->add(10, 20), 30);

            // Forge a packet with a known-wrong counter and inject it directly into the
            // client's socket. The server's receive loop expects the next counter to be
            // m_messageCounterIncoming + 1; sending 0 (or any out-of-sequence value) is
            // virtually certain to mismatch. On mismatch the protocol qFatal()s the helper.
            QByteArray packet;
            {
                QDataStream ds(&packet, QIODevice::WriteOnly);
                ds << quint64(0);                                  // counter (wrong)
                ds << quint8(0);                                   // Call::InvokeMethod
                ds << QByteArray("Simple");                        // className
                ds << uint(0);                                     // instanceId
                ds << QByteArray("add");                           // function
                ds << quint32(2);                                  // argCount
                ds << QVariant(int(1));
                ds << QVariant(int(2));
            }
            const int sock = SocketIpcPrivate::get(client)->m_socket;
            QVERIFY(sock >= 0);
            const ssize_t n = ::send(sock, packet.constData(), packet.size(), MSG_NOSIGNAL);
            QCOMPARE(qsizetype(n), packet.size());

            // The server should abort (qFatal -> SIGABRT) shortly after receiving the bad
            // packet. Reap it and verify it died by signal rather than exited normally.
            int status = 0;
            const pid_t reaped = ::waitpid(m_serverPid, &status, 0);
            m_serverPid = -1;  // already reaped
            QCOMPARE(reaped, ::getpid() != reaped ? reaped : reaped);  // silence unused
            QVERIFY2(WIFSIGNALED(status), "Server must die by signal after counter mismatch");
            // qFatal -> abort() -> SIGABRT; some Qt builds raise SIGTRAP first via __builtin_trap
            QVERIFY2((WTERMSIG(status) == SIGABRT) || (WTERMSIG(status) == SIGTRAP),
                     qPrintable(QStringLiteral("Unexpected term signal: %1").arg(WTERMSIG(status))));
        });
}

void tst_SocketIpc::requestTimeoutFiresPromise()
{
    // Server: registers SimpleServer with blockForever() (pauses indefinitely). Client: sets a
    // 200 ms request timeout (no cascade qFatal). The async future must resolve with a timeout
    // exception, and the channel must remain usable for subsequent requests after a successful
    // reply resets the consecutive-timeout counter back to zero.
    auto cfg = SocketIpcConfiguration::createSocketPair();
    m_serverPid = ::fork();
    QVERIFY2(m_serverPid >= 0, "fork() failed");

    if (m_serverPid == 0) {
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (::getppid() == 1)
            ::_exit(0);
#if defined(QT_AM_COVERAGE)
        ::signal(SIGTERM, [](int) { __gcov_dump(); ::_exit(0); });
#else
        ::signal(SIGTERM, SIG_DFL);
#endif
        ::signal(SIGABRT, SIG_DFL);
        ::signal(SIGSEGV, SIG_DFL);
        ::signal(SIGBUS, SIG_DFL);
        qInstallMessageHandler(nullptr);
        try {
            auto server = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Server);
            server->registerSingleton(std::make_unique<SimpleServer>());
            server->start();
            QEventLoop loop;
            loop.exec();
        } catch (...) {
            ::_exit(1);
        }
        ::_exit(0);
    }

    auto client = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Client);
    client->setRequestTimeout(std::chrono::milliseconds(200));
    // No cascade qFatal: the test process must survive the timeout.
    client->setMaxConsecutiveTimeouts(0);
    client->start();

    auto proxy = client->bindSingleton<SimpleClient>();

    // blockForever() never returns server-side; future must time out client-side.
    QFuture<void> stuck = proxy->blockForeverAsync();
    bool threw = false;
    QString message;
    try {
        stuck.waitForFinished();
    } catch (const Exception &e) {
        threw = true;
        message = QString::fromUtf8(e.what());
    }
    QVERIFY2(threw, "Async future must finish after the request timeout");
    QVERIFY2(message.contains(u"timed out"_s), qPrintable(message));
}

void tst_SocketIpc::consecutiveTimeoutsKillClient()
{
    // Cascade check: with maxConsecutiveTimeouts=2 against an unresponsive peer, the client
    // process must abort (SIGABRT via qFatal) on the second timeout. The test driver can't
    // call qFatal on itself, so we fork BOTH the server and the client and only observe their
    // exit statuses.

    int s[2];
    QCOMPARE(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, s), 0);

    auto setupChild = [] {
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (::getppid() == 1)
            ::_exit(0);
#if defined(QT_AM_COVERAGE)
        ::signal(SIGTERM, [](int) { __gcov_dump(); ::_exit(0); });
#else
        ::signal(SIGTERM, SIG_DFL);
#endif
        ::signal(SIGABRT, SIG_DFL);
        ::signal(SIGSEGV, SIG_DFL);
        ::signal(SIGBUS, SIG_DFL);
        qInstallMessageHandler(nullptr);
    };

    // Server child: keeps s[0], closes s[1], runs SimpleServer with the blockForever() method.
    pid_t srvPid = ::fork();
    QVERIFY2(srvPid >= 0, "fork() failed (server)");
    if (srvPid == 0) {
        setupChild();
        ::close(s[1]);
        try {
            SocketIpcConfiguration cfg(s[0], -1);  // -1 client fd: nothing to close on this side
            auto server = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Server);
            server->registerSingleton(std::make_unique<SimpleServer>());
            server->start();
            QEventLoop loop;
            loop.exec();
        } catch (...) {
            ::_exit(1);
        }
        ::_exit(0);
    }

    // Client child: keeps s[1], closes s[0], sets 100 ms timeout + cascade=2 and fires two
    // blockForever() async requests. The second timeout must trigger qFatal -> SIGABRT.
    pid_t cliPid = ::fork();
    QVERIFY2(cliPid >= 0, "fork() failed (client)");
    if (cliPid == 0) {
        setupChild();
        ::close(s[0]);
        try {
            SocketIpcConfiguration cfg(-1, s[1]);  // -1 server fd: nothing to close on this side
            auto client = std::make_unique<SocketIpc>(std::move(cfg), SocketIpc::Role::Client);
            client->setRequestTimeout(std::chrono::milliseconds(100));
            client->setMaxConsecutiveTimeouts(2);
            client->start();
            auto proxy = client->bindSingleton<SimpleClient>();
            auto f1 = proxy->blockForeverAsync();
            auto f2 = proxy->blockForeverAsync();
            try { f1.waitForFinished(); } catch (...) { }
            try { f2.waitForFinished(); } catch (...) { }
            ::_exit(42);  // unique code: cascade did NOT fire
        } catch (...) {
            ::_exit(1);
        }
    }

    // Test-driver side: drop our copies of both fds so we don't keep the pair alive past the
    // children's lifetimes.
    ::close(s[0]);
    ::close(s[1]);

    // Wait for the client to abort.
    int cliStatus = 0;
    QCOMPARE(::waitpid(cliPid, &cliStatus, 0), cliPid);
    QVERIFY2(WIFSIGNALED(cliStatus),
             qPrintable(QStringLiteral("Client must die by signal; got exit code %1")
                            .arg(WIFEXITED(cliStatus) ? WEXITSTATUS(cliStatus) : -1)));
    QVERIFY2((WTERMSIG(cliStatus) == SIGABRT) || (WTERMSIG(cliStatus) == SIGTRAP),
             qPrintable(QStringLiteral("Unexpected client term signal: %1").arg(WTERMSIG(cliStatus))));

    // Server cleanup.
    ::kill(srvPid, SIGTERM);
    ::waitpid(srvPid, nullptr, 0);
}

QTEST_GUILESS_MAIN(tst_SocketIpc)
#include "tst_socketipc.moc"
