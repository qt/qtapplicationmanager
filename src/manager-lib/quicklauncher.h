// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QUICKLAUNCHER_H
#define QUICKLAUNCHER_H

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QVector>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AbstractContainer;
class AbstractRuntime;
class CpuReader;

class QuickLauncher : public QObject
{
    Q_OBJECT

public:
    static QuickLauncher *createInstance(const QMap<std::pair<QString, QString>, int> &runtimesPerContainer,
                                         qreal idleLoad, int failedStartLimit,
                                         int failedStartLimitIntervalSec);

    static QuickLauncher *instance();
    ~QuickLauncher() override;

    QPair<AbstractContainer *, AbstractRuntime *> take(const QString &containerId, const QString &runtimeId);
    void shutDown();

public Q_SLOTS:
    void rebuild();

Q_SIGNALS:
    void shutDownFinished();

protected:
    void timerEvent(QTimerEvent *te) override;

private:
    QuickLauncher(const QMap<std::pair<QString, QString>, int> &runtimesPerContainer,
                  qreal idleLoad, int failedStartLimit, int failedStartLimitIntervalSec,
                  QObject *parent = nullptr);
    QuickLauncher(const QuickLauncher &);
    QuickLauncher &operator=(const QuickLauncher &);
    static QuickLauncher *s_instance;

    void triggerRebuild(int delay = 0);
    void removeEntry(AbstractContainer *container, AbstractRuntime *runtime);
    void checkFailedStarts();

    struct QuickLaunchEntry
    {
        QString m_containerId;
        QString m_runtimeId;
        int m_maximum = 1;
        bool m_disabled = false;

        void addFailure();
        QVector<qint64> m_failedTimeStamps; // msecs since epoch when the instance failed

        QList<QPair<AbstractContainer *, AbstractRuntime *>> m_containersAndRuntimes;
    };

    QVector<QuickLaunchEntry> m_quickLaunchPool;
    int m_idleTimerId = 0;
    CpuReader *m_idleCpu = nullptr;
    bool m_isIdle = false;
    qreal m_idleThreshold;
    bool m_shuttingDown = false;
    int m_failedStartLimit;
    int m_failedStartLimitIntervalSec;
};

QT_END_NAMESPACE_AM

#endif // QUICKLAUNCHER_H
