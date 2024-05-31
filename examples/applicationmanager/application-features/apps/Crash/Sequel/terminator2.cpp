// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "terminator2.h"
#include <signal.h>


Terminator2::Terminator2(QObject *parent) : QObject(parent)
{
}

void Terminator2::accessIllegalMemory() const
{
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Warray-bounds")
    *reinterpret_cast<int *>(1) = 42;
QT_WARNING_POP
}

void Terminator2::accessIllegalMemoryInThread()
{
    TerminatorThread *t = new TerminatorThread(this);
    t->start();
}

void Terminator2::forceStackOverflow() const
{
    static constexpr int len = 100000;
    volatile char buf[len];
    buf[len-1] = 42;
    if (buf[len-1] == 42)
        forceStackOverflow();
}

void Terminator2::divideByZero() const
{
    int d = 0;
    volatile int x = 42 / d;
    Q_UNUSED(x)
}

void Terminator2::raise() const
{
    ::raise(m_signum);
}


TerminatorThread::TerminatorThread(Terminator2 *t2)
    : QThread(t2)
    , m_terminator2(t2)
{
}

void TerminatorThread::run()
{
    m_terminator2->accessIllegalMemory();
}

#include "moc_terminator2.cpp"
