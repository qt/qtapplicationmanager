// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QQmlExtensionPlugin>
#include <QThread>


class QmlCrashPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override;
};


class QmlCrash : public QObject
{
    Q_OBJECT

public:
    QmlCrash(QObject *parent) : QObject(parent) {}

    Q_INVOKABLE void accessIllegalMemory() const;
    Q_INVOKABLE void accessIllegalMemoryInThread();
    Q_INVOKABLE void forceStackOverflow() const;
    Q_INVOKABLE void divideByZero() const;
    Q_INVOKABLE void abort() const;
    Q_INVOKABLE void raise(int sig) const;
    Q_INVOKABLE void throwUnhandledException() const;
    Q_INVOKABLE void exitGracefully() const;
};


class QmlCrashThread : public QThread
{
    Q_OBJECT

public:
    explicit QmlCrashThread(QmlCrash *parent);

private:
    void run() override;
    QmlCrash *m_qmlCrash;
};
