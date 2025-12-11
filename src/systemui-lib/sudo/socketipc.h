// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:communication-protocol

#ifndef SOCKETIPC_H
#define SOCKETIPC_H

#include <chrono>

#include <QtAppManCommon/exception.h>
#include <QtCore/QByteArray>
#include <QtCore/QFuture>
#include <QtCore/QObject>
#include <QtCore/qmetaobject.h> // on purpose, because of potential "lean" headers
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE_AM

class SocketIpcConfiguration;
class SocketIpcConfigurationPrivate;
class SocketIpcPrivate;

class Q_AUTOTEST_EXPORT SocketIpcPromiseBase
{
    // this base class is necessary to be able to store an un-templated pointer to a SocketIpcPromise<R>
protected:
    SocketIpcPromiseBase() = default;
    Q_DISABLE_COPY_MOVE(SocketIpcPromiseBase)

public:
    virtual ~SocketIpcPromiseBase() = default;
    virtual void finish(bool successful, const QVariant &result) = 0;
};


template <typename R>
class SocketIpcPromise : public SocketIpcPromiseBase
{
public:
    SocketIpcPromise()  { m_promise.start(); }
    QFuture<R> future() const  { return m_promise.future(); }

    void finish(bool successful, const QVariant &result) final
    {
        if (successful) {
            if constexpr (std::is_void_v<R>) {
                if (result.metaType().id() != QMetaType::UnknownType)
                    throw Exception("Invalid argument meta type in reply message: expected 'void', but got '%1'")
                        .arg(result.metaType().name());
            } else {
                if (result.metaType().id() != qMetaTypeId<R>()) {
                    throw Exception("Invalid argument meta type in reply message: expected '%1', but got '%2'")
                        .arg(QMetaType::fromType<R>().name())
                        .arg((result.metaType().id()!= QMetaType::UnknownType)
                                 ? result.metaType().name() : "void");
                }
                m_promise.addResult(result.value<R>());
            }
        } else {
            if (result.metaType().id() != QMetaType::QString) {
                throw Exception("Invalid argument type in error reply message: expected 'QString', but got '%1'")
                    .arg((result.metaType().id()!= QMetaType::UnknownType)
                             ? result.metaType().name() : "void");
            }
            m_promise.setException(Exception(result.toString()));
        }
        m_promise.finish();
    }

private:
    QPromise<R> m_promise;
};


class Q_AUTOTEST_EXPORT SocketIpc : public QObject
{
    Q_OBJECT
public:
    enum class Role : quint8 {
        Server,
        Client,
    };

    SocketIpc(SocketIpcConfiguration &&config, Role role, QObject *parent = nullptr);
    ~SocketIpc() override;

    void start();

    void setRequestTimeout(std::chrono::milliseconds timeout);
    void setMaxConsecutiveTimeouts(int maxCount);

    // SERVER SIDE:

    void registerSingleton(std::unique_ptr<QObject> obj);

    void registerCallbackObjectFactory(const QByteArray &className,
                                       const std::function<std::unique_ptr<QObject>(SocketIpc *ipc, const QByteArray &className,
                                                                                    const QVariantList &vargs)> &factory);

    template<typename T, std::enable_if_t<std::is_convertible_v<T *, QObject *>, bool> = true>
    void registerMetaObjectFactory()
    {
        // Keep in mind that the created objects live in the IPC thread. If you need them to live
        // in another thread, then push them out in the c'tor or use the callback factory variant.
        doRegisterMetaObjectFactory(&T::staticMetaObject);
    }

    // CLIENT SIDE:

    template<typename R, typename ...ARGS>
    QFuture<R> invokeMethodAsync(QObject *object, const QByteArray &function, ARGS && ...args)
    {
        QVariantList vargs;
        if constexpr (sizeof...(args) > 0)
            ( vargs << ... << QVariant::fromValue(std::remove_reference_t<ARGS>(args)) );
        auto promise = std::make_unique<SocketIpcPromise<R>>();
        auto future = promise->future();

        doInvokeMethodAsync(object, function, vargs, std::move(promise));
        return future;
    }

    template<typename R, typename ...ARGS>
    R invokeMethod(QObject *object, const QByteArray &function, ARGS && ...args)
    {
        auto future = invokeMethodAsync<R>(object, function, args...);
        if constexpr (std::is_void_v<R>)  // result() does not work for void futures
            future.waitForFinished();
        else
            return future.result();
    }

    template<typename T, std::enable_if_t<std::is_convertible_v<T *, QObject *>, bool> = true>
    std::unique_ptr<T> bindSingleton()
    {
        try {
            int ci = T::staticMetaObject.indexOfClassInfo("SocketIpcClassName");
            if (ci < 0)
                throw Exception("Cannot call bindSingleton on a QObject without a Q_CLASSINFO(\"SocketIpcClassName\", ...)");
            QByteArray className = T::staticMetaObject.classInfo(ci).value();

            auto obj = std::make_unique<T>(this);

            doBindSingleton(obj.get(), className);
            return obj;
        } catch (const Exception &e) {
            qFatal() << "SocketIpc::bindSingleton failed:" << e.what();
        }
        return { };
    }

    template<typename T, typename ...ARGS, std::enable_if_t<std::is_convertible_v<T *, QObject *>, bool> = true>
    std::unique_ptr<T> createInstance(ARGS && ...args)
    {
        try {
            int ci = T::staticMetaObject.indexOfClassInfo("SocketIpcClassName");
            if (ci < 0)
                throw Exception("Cannot call createInstance on a QObject without a Q_CLASSINFO(\"SocketIpcClassName\", ...)");
            QByteArray className = T::staticMetaObject.classInfo(ci).value();

            auto obj = std::make_unique<T>(this, args...);

            QVariantList vargs;
            if constexpr (sizeof...(args) > 0)
                ( vargs << ... << QVariant::fromValue(std::remove_reference_t<ARGS>(args)) );

            doCreateInstance(obj.get(), className, vargs);
            return obj;
        } catch (const Exception &e) {
            qFatal() << "SocketIpc::createInstance failed:" << e.what();
        }
        return { };
    }

private:
    // these should be in SocketIpcPrivate, but we need to call them from public template functions above
    void doRegisterMetaObjectFactory(const QMetaObject *metaObject);
    void doInvokeMethodAsync(QObject *object, const QByteArray &function,
                             const QVariantList &args, std::unique_ptr<SocketIpcPromiseBase> promise);
    void doBindSingleton(QObject *obj, const QByteArray &className);
    void doCreateInstance(QObject *obj, const QByteArray &className, const QVariantList &vargs);

    std::unique_ptr<SocketIpcPrivate> d;
    friend class SocketIpcPrivate;
};


class Q_AUTOTEST_EXPORT SocketIpcConfiguration
{
public:
    // Creates a connected socket pair and throws on failure
    static SocketIpcConfiguration createSocketPair() noexcept(false);

    // Adopts and takes ownership of existing fds
    SocketIpcConfiguration(int serverSocket, int clientSocket);
    SocketIpcConfiguration(const SocketIpcConfiguration &other) = delete;
    SocketIpcConfiguration(SocketIpcConfiguration &&other) noexcept;
    ~SocketIpcConfiguration();

    SocketIpcConfiguration &operator=(const SocketIpcConfiguration &other) = delete;
    SocketIpcConfiguration &operator=(SocketIpcConfiguration &&other) noexcept;

    void clear();

private:
    std::unique_ptr<SocketIpcConfigurationPrivate> d;

    friend class SocketIpc;
    friend class SocketIpcConfigurationPrivate;
};

QT_END_NAMESPACE_AM

#endif // SOCKETIPC_H
