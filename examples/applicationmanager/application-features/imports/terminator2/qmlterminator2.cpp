// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QQmlEngine>
#include <QJSEngine>

#include "qmlterminator2.h"

#include <signal.h>


static QObject *terminator_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    return new Terminator(engine);
}

void TerminatorPlugin::registerTypes(const char *uri)
{
    qmlRegisterSingletonType<Terminator>(uri, 2, 0, "Terminator", terminator_provider);
}


void Terminator::accessIllegalMemory() const
{
    *(int*)1 = 42;
}

void Terminator::accessIllegalMemoryInThread()
{
    TerminatorThread *t = new TerminatorThread(this);
    t->start();
}

void Terminator::forceStackOverflow() const
{
    static constexpr int len = 100000;
    volatile char buf[len];
    buf[len-1] = 42;
    if (buf[len-1] == 42)
        forceStackOverflow();
}

void Terminator::divideByZero() const
{
    int d = 0;
    volatile int x = 42 / d;
    Q_UNUSED(x)
}

void Terminator::abort() const
{
    ::abort();
}

void Terminator::raise(int sig) const
{
    ::raise(sig);
}

void Terminator::throwUnhandledException() const
{
    throw 42;
}

void Terminator::exitGracefully() const
{
    exit(5);
}


TerminatorThread::TerminatorThread(Terminator *parent)
    : QThread(parent), m_terminator(parent)
{
}

void TerminatorThread::run()
{
    m_terminator->accessIllegalMemory();
}

#include "moc_qmlterminator2.cpp"
