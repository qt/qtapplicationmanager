// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#pragma once

#include <QQmlEngine>
#include <QThread>


class Terminator2 : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int signum MEMBER m_signum NOTIFY signumChanged FINAL)

public:
    Terminator2(QObject *parent = nullptr);

    Q_INVOKABLE void accessIllegalMemory() const;
    Q_INVOKABLE void accessIllegalMemoryInThread();
    Q_INVOKABLE void forceStackOverflow() const;
    Q_INVOKABLE void divideByZero() const;
    Q_INVOKABLE void raise() const;

signals:
    void signumChanged();

private:
    int m_signum;
};


class TerminatorThread : public QThread
{
    Q_OBJECT

public:
    explicit TerminatorThread(Terminator2 *t2);

private:
    void run() override;
    Terminator2 *m_terminator2;
};
