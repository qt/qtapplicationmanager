// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <QtCore/QObject>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QEventLoop)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)

QT_BEGIN_NAMESPACE_AM

class WatchdogPrivate;

class Watchdog : public QObject
{
    Q_OBJECT
public:
    static Watchdog *create();
    inline static Watchdog *instance() { return s_instance; }
    ~Watchdog() override;

    void setEventLoopTimeouts(std::chrono::milliseconds check,
                              std::chrono::milliseconds warn, std::chrono::milliseconds kill);
    void setQuickWindowTimeouts(std::chrono::milliseconds check,
                                std::chrono::milliseconds warn, std::chrono::milliseconds kill);

    inline bool isActive() const { return m_active.loadRelaxed(); }

    // callback API that needs to be fed from QCoreApplication::notify
    void eventCallback(const QThread *thread, bool begin, QObject *receiver, QEvent *event);

private:
    Watchdog();
    void shutdown();
    Q_DISABLE_COPY_MOVE(Watchdog)
    static Watchdog *s_instance;

    WatchdogPrivate *d = nullptr;
    QAtomicInteger<bool> m_active = false;
    bool m_cleanShutdown = false;

    friend class WatchdogPrivate;
};

QT_END_NAMESPACE_AM

#endif // WATCHDOG_H
