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
    ~Watchdog() override;

    void setEventLoopTimeouts(std::chrono::milliseconds check,
                              std::chrono::milliseconds warn, std::chrono::milliseconds kill);
    void setQuickWindowTimeouts(std::chrono::milliseconds check,
                                std::chrono::milliseconds warnSync, std::chrono::milliseconds killSync,
                                std::chrono::milliseconds warnRender, std::chrono::milliseconds killRender,
                                std::chrono::milliseconds warnSwap, std::chrono::milliseconds killSwap);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Watchdog();
    void shutdown();
    Q_DISABLE_COPY_MOVE(Watchdog)

    WatchdogPrivate *d = nullptr;
    bool m_cleanShutdown = false;
};

QT_END_NAMESPACE_AM

#endif // WATCHDOG_H
