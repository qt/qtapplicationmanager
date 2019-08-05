/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QtAppManCommon/global.h>
#include <QObject>
#include <QAtomicInteger>

#include <initializer_list>
#include <list>
#include <vector>
#include <functional>
#include <signal.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#elif defined(Q_OS_WIN)
#  include <QMutex>
#  include <QWinEventNotifier>
#endif

QT_BEGIN_NAMESPACE_AM

class UnixSignalHandler : public QObject
{
    Q_OBJECT

public:
    static UnixSignalHandler *instance();

    static const char *signalName(int sig);

    void resetToDefault(int sig);
    void resetToDefault(const std::initializer_list<int> &sigs);

    enum Type {
        RawSignalHandler,
        ForwardedToEventLoopHandler
    };

    bool install(Type handlerType, int sig, const std::function<void(int)> &handler);
    bool install(Type handlerType, const std::initializer_list<int> &sigs,
                 const std::function<void(int)> &handler);

private:
    UnixSignalHandler();
    static UnixSignalHandler *s_instance;

    struct SigHandler
    {
        SigHandler(int signal, bool qt, const std::function<void(int)> &handler);

        int m_signal;
        bool m_qt;
        std::function<void(int)> m_handler;
    };

    std::list<SigHandler> m_handlers; // we're using STL to avoid (accidental) implicit copies
    int m_currentSignal = 0;

#if defined(Q_ATOMIC_INT64_IS_SUPPORTED)
    // 64 bits are currently enough to map all Linux signals
    typedef quint64 am_sigmask_t;
#else
    // 32 bits are sufficient for Win32 (which does not support 64 bit atomics)
    typedef quint32 am_sigmask_t;
#endif
    static am_sigmask_t am_sigmask(int sig);
    QAtomicInteger<am_sigmask_t> m_resetSignalMask;
#if defined(Q_OS_UNIX)
    int m_pipe[2];
#elif defined(Q_OS_WIN)
    QMutex m_winLock;
    QWinEventNotifier *m_winEvent = nullptr;
    QList<int> m_signalsForEventLoop;
#endif
};

QT_END_NAMESPACE_AM
