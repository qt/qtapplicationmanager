// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#pragma once

#include <QQmlExtensionPlugin>
#include <QThread>


class TerminatorPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override;
};


class Terminator : public QObject
{
    Q_OBJECT

public:
    Terminator(QObject *parent) : QObject(parent) {}

    Q_INVOKABLE void accessIllegalMemory() const;
    Q_INVOKABLE void accessIllegalMemoryInThread();
    Q_INVOKABLE void forceStackOverflow() const;
    Q_INVOKABLE void divideByZero() const;
    Q_INVOKABLE void abort() const;
    Q_INVOKABLE void raise(int sig) const;
    Q_INVOKABLE void throwUnhandledException() const;
    Q_INVOKABLE void exitGracefully() const;
};


class TerminatorThread : public QThread
{
    Q_OBJECT

public:
    explicit TerminatorThread(Terminator *parent);

private:
    void run() override;
    Terminator *m_terminator;
};
