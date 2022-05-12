/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QObject>
#include <QPair>
#include <QVector>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AbstractContainer;
class AbstractRuntime;
class CpuReader;

class QuickLauncher : public QObject
{
    Q_OBJECT

public:
    static QuickLauncher *createInstance(int runtimesPerContainer, qreal idleLoad);
    static QuickLauncher *instance();
    ~QuickLauncher() override;

    QPair<AbstractContainer *, AbstractRuntime *> take(const QString &containerId, const QString &runtimeId);
    void shutDown();

public slots:
    void rebuild();

signals:
    void shutDownFinished();

protected:
    void timerEvent(QTimerEvent *te) override;

private:
    QuickLauncher(QObject *parent = nullptr);
    QuickLauncher(const QuickLauncher &);
    void initialize(int runtimesPerContainer, qreal idleLoad);
    QuickLauncher &operator=(const QuickLauncher &);
    static QuickLauncher *s_instance;

    void triggerRebuild(int delay = 0);
    void removeEntry(AbstractContainer *container, AbstractRuntime *runtime);

    struct QuickLaunchEntry
    {
        QString m_containerId;
        QString m_runtimeId;
        int m_maximum = 1;
        QList<QPair<AbstractContainer *, AbstractRuntime *>> m_containersAndRuntimes;
    };

    QVector<QuickLaunchEntry> m_quickLaunchPool;
    int m_idleTimerId = 0;
    CpuReader *m_idleCpu = nullptr;
    bool m_isIdle = false;
    qreal m_idleThreshold;
    bool m_shuttingDown = false;
};

QT_END_NAMESPACE_AM
