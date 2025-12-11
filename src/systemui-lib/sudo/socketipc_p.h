// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:communication-protocol

#ifndef SOCKETIPC_P_H
#define SOCKETIPC_P_H

#include "socketipc.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE_AM

class SocketIpcPrivate;


class Q_AUTOTEST_EXPORT SocketIpcConfigurationPrivate
{
public:
    quint64 m_messageCounterStartServer = 0;
    quint64 m_messageCounterStartClient = 0;
    int m_serverSocket = -1;
    int m_clientSocket = -1;

    // for unit testing:
    static SocketIpcConfigurationPrivate *get(SocketIpcConfiguration *cfg) noexcept
    { return cfg ? cfg->d.get() : nullptr; }
    static const SocketIpcConfigurationPrivate *get(const SocketIpcConfiguration *cfg) noexcept
    { return cfg ? cfg->d.get() : nullptr; }
};


class SocketIpcMessage
{
public:
    enum class Call : quint8 {
        InvokeMethod,
        EmitSignal,
        InvokeMethodResult,
        InvokeMethodException,
    };

    SocketIpcMessage() = default;
    SocketIpcMessage(Call call, quint64 counter, const QByteArray &className, uint instanceId,
                     const QByteArray &function, const QVariantList &args,
                     std::unique_ptr<SocketIpcPromiseBase> promise = { });
    ~SocketIpcMessage() = default;
    Q_DISABLE_COPY_MOVE(SocketIpcMessage)

private:
    quint64 m_counter = 0;
    Call m_call { };
    QByteArray m_className;
    uint m_instanceId = 0;
    QByteArray m_function;
    QVariantList m_arguments;
    std::unique_ptr<SocketIpcPromiseBase> m_promise;

    friend class SocketIpc;
    friend class SocketIpcPrivate;
};


class SocketIpcRegistry : public QObject
{
    Q_OBJECT
public:
    SocketIpcRegistry(SocketIpc *ipc, SocketIpcPrivate *ipcPrivate);

    Q_INVOKABLE uint createInstance(const QByteArray &className, const QVariantList &args);
    Q_INVOKABLE void destroyInstance(const QByteArray &className, uint instanceId);
    Q_INVOKABLE void bindSingleton(const QByteArray &className);
    Q_INVOKABLE void unbindSingleton(const QByteArray &className);

private:
    SocketIpc *q = nullptr;
    SocketIpcPrivate *d = nullptr;
};


class SocketIpcPrivate : public QObject
{
    Q_OBJECT
public:
    explicit SocketIpcPrivate(SocketIpc *ipc)
        : q(ipc)
    { }
    ~SocketIpcPrivate() override;

    // for unit testing:
    static SocketIpcPrivate *get(SocketIpc *ipc) noexcept
    { return ipc ? ipc->d.get() : nullptr; }
    static const SocketIpcPrivate *get(const SocketIpc *ipc) noexcept
    { return ipc ? ipc->d.get() : nullptr; }

    SocketIpc *q;
    int m_socket = -1;
    SocketIpc::Role m_role { };
    QMutex m_sendMutex;

    struct Class
    {
        QByteArray m_name;
        bool m_isSingleton = false; // if true, then m_instances = { 0: obj }

        using FactoryFunction = std::unique_ptr<QObject> (SocketIpc *, const QByteArray &,
                                                         const QVariantList &);
        std::function<FactoryFunction> m_factory;

        QMap<uint, QObject *> m_instances;
        QVector<uint> m_deadInstanceIds; // FIFO

        static constexpr qsizetype minimumInstanceIdReuseDelta = 20;

        uint getNextInstanceId()
        {
            if (m_deadInstanceIds.size() > minimumInstanceIdReuseDelta)
                return m_deadInstanceIds.takeFirst();
            return m_instances.isEmpty() ? 1 : (m_instances.lastKey() + 1);
        }

        void releaseInstanceId(uint instanceId)
        {
            m_deadInstanceIds.emplace_back(instanceId);
        }
    };

    QList<Class *> m_classes;
    mutable QMutex m_classesMutex;

    Class *findClass(const QByteArray &name) const;
    std::pair<Class *, uint> findInstance(const QObject *object) const noexcept(false);

    void addSingletonClass(const QByteArray &className, std::unique_ptr<QObject> obj);
    void addObjectInstance(const QByteArray &className, uint instanceId, QObject *obj);
    void addFactoryClass(const QByteArray &className, const std::function<Class::FactoryFunction> &factory);
    QObject *removeObjectInstance(const QByteArray &className, uint instanceId);

    SocketIpcRegistry *m_registry = nullptr; // owned by m_classes (added under "#Registry")

    QList<SocketIpcMessage *> m_sentInvokeMethods;
    std::unique_ptr<QThread> m_receiverThread;
    quint64 m_messageCounterIncoming = 0;
    quint64 m_messageCounterOutgoing = 0;

    qint64 m_requestTimeoutMs = 0;
    int m_maxConsecutiveTimeouts = 0;
    int m_consecutiveTimeouts = 0;

    void scheduleRequestTimeout(quint64 counter);
    void handleRequestTimeout(quint64 counter);

    std::unique_ptr<SocketIpcMessage> messageFromPacket(const QByteArray &packet) noexcept(false);
    QByteArray messageToPacket(SocketIpcMessage *msg) const;

    void send(std::unique_ptr<SocketIpcMessage> msg);
    void receive();
    std::unique_ptr<SocketIpcMessage> handle(const SocketIpcMessage *msg);

    Q_SLOT void relaySlot(QMethodRawArguments argv);
    static int relaySlotMethodIndex();
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // SOCKETIPC_P_H
