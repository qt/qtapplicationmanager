// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>

#include "logging.h"
#include "abstractcontainer.h"
#include "abstractruntime.h"
#include "containerfactory.h"
#include "runtimefactory.h"
#include "quicklauncher.h"
#include "systemreader.h"

#include <memory>

QT_BEGIN_NAMESPACE_AM

QuickLauncher *QuickLauncher::s_instance = nullptr;

QuickLauncher *QuickLauncher::createInstance(int runtimesPerContainer, qreal idleLoad)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("QuickLauncher instance already exists");

    s_instance = new QuickLauncher();
    s_instance->initialize(runtimesPerContainer, idleLoad);
    return s_instance;
}

QuickLauncher *QuickLauncher::instance()
{
    return s_instance;
}

QuickLauncher::QuickLauncher(QObject *parent)
    : QObject(parent)
{ }

QuickLauncher::~QuickLauncher()
{
    if (m_idleTimerId)
        killTimer(m_idleTimerId);
    delete m_idleCpu;
    s_instance = nullptr;
}

void QuickLauncher::initialize(int runtimesPerContainer, qreal idleLoad)
{
    ContainerFactory *cf = ContainerFactory::instance();
    RuntimeFactory *rf = RuntimeFactory::instance();

    qCDebug(LogSystem) << "Setting up the quick-launch pool:";

    const QStringList allContainerIds = cf->containerIds();
    for (const QString &containerId : allContainerIds) {
        if (!cf->manager(containerId)->supportsQuickLaunch())
            continue;

        const QStringList allRuntimeIds = rf->runtimeIds();
        for (const QString &runtimeId : allRuntimeIds) {
            if (rf->manager(runtimeId)->inProcess())
                continue;

            QuickLaunchEntry entry;
            entry.m_containerId = containerId;
            entry.m_maximum = runtimesPerContainer;

            if (rf->manager(runtimeId)->supportsQuickLaunch())
                entry.m_runtimeId = runtimeId;

            m_quickLaunchPool << entry;

            qCDebug(LogSystem).nospace().noquote() << " * " << entry.m_containerId << " / "
                                                   << (entry.m_runtimeId.isEmpty() ? qSL("(no runtime)") : entry.m_runtimeId)
                                                   << " [at max: " << runtimesPerContainer << "]";
        }
    }

    if (idleLoad > 0) {
        m_idleThreshold = idleLoad;
        m_idleCpu = new CpuReader();
        m_idleTimerId = startTimer(1000);
    }
    triggerRebuild();
}

void QuickLauncher::timerEvent(QTimerEvent *te)
{
    if (te && te->timerId() == m_idleTimerId) {
        bool nowIdle = (m_idleCpu->readLoadValue() <= m_idleThreshold);
        if (nowIdle != m_isIdle) {
            m_isIdle = nowIdle;

            if (m_isIdle)
                rebuild();
        }
    }
}

void QuickLauncher::rebuild()
{
    if (m_shuttingDown)
        return;

    int todo = 0;
    int done = 0;

    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        if (entry->m_containersAndRuntimes.size() < entry->m_maximum) {
            todo += (entry->m_maximum - entry->m_containersAndRuntimes.size());
            if (done >= 1)
                continue;

            std::unique_ptr<AbstractContainer> ac(ContainerFactory::instance()->create(entry->m_containerId, nullptr));
            if (!ac) {
                qCWarning(LogSystem) << "ERROR: Could not create quick-launch container with id"
                                     << entry->m_containerId;
                continue;
            }

            std::unique_ptr<AbstractRuntime> ar;
            if (!entry->m_runtimeId.isEmpty()) {
                ar.reset(RuntimeFactory::instance()->createQuickLauncher(ac.release(), entry->m_runtimeId));
                if (!ar) {
                    qCWarning(LogSystem) << "ERROR: Could not create quick-launch runtime with id"
                                         << entry->m_runtimeId << "within container with id"
                                         << entry->m_containerId;
                    continue;
                }
                if (!ar->start()) {
                    qCWarning(LogSystem) << "ERROR: Could not start quick-launch runtime with id"
                                         << entry->m_runtimeId << "within container with id"
                                         << entry->m_containerId;
                    continue;
                }
            }
            AbstractContainer *container = ar ? ar.get()->container() : ac.release();
            AbstractRuntime *runtime = ar.release();

            connect(container, &AbstractContainer::destroyed, this, [this, container]() { removeEntry(container, nullptr); });
            if (runtime)
                connect(runtime, &AbstractRuntime::destroyed, this, [this, runtime]() { removeEntry(nullptr, runtime); });

            entry->m_containersAndRuntimes << qMakePair(container, runtime);
            ++done;

            qCDebug(LogSystem).noquote() << "Added a new entry to the quick-launch pool:"
                                         << entry->m_containerId << "/"
                                         << (entry->m_runtimeId.isEmpty() ? qSL("(no runtime)") : entry->m_runtimeId);
        }
    }
    if (todo > done)
        triggerRebuild(1000);
}

void QuickLauncher::triggerRebuild(int delay)
{
    QTimer::singleShot(delay, this, &QuickLauncher::rebuild);
}

void QuickLauncher::removeEntry(AbstractContainer *container, AbstractRuntime *runtime)
{
    int carCount = 0;
    int carRemoved = 0;

    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        for (int i = 0; i < entry->m_containersAndRuntimes.size(); ++i) {
            auto car = entry->m_containersAndRuntimes.at(i);
            if ((container && car.first == container)
                    || (runtime && car.second == runtime)) {
                qCDebug(LogSystem) << "Removed quicklaunch entry for container/runtime" << container << runtime;

                entry->m_containersAndRuntimes.removeAt(i--);
                carRemoved++;
            }
            carCount += entry->m_containersAndRuntimes.count();
        }
    }
    // make sure to only emit shutDownFinished once: when the list gets empty for the first time.
    // (removeEntry could be called again afterwards and it wouldn't actually remove anything, but
    // without this guard, it would emit the signal multiple times)
    if (m_shuttingDown && (carRemoved > 0) && (carCount == 0))
        emit shutDownFinished();
}

QPair<AbstractContainer *, AbstractRuntime *> QuickLauncher::take(const QString &containerId, const QString &runtimeId)
{
    QPair<AbstractContainer *, AbstractRuntime *> result(nullptr, nullptr);

    // 1st pass: find entry with matching container and runtime
    // 2nd pass: find entry with matching container and no runtime
    for (int pass = 1; pass <= 2; ++pass) {
        for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
            if (entry->m_containerId == containerId) {
                if (((pass == 1) && (entry->m_runtimeId == runtimeId))
                        || ((pass == 2) && (entry->m_runtimeId.isEmpty()))) {
                    if (!entry->m_containersAndRuntimes.isEmpty()) {
                        result = entry->m_containersAndRuntimes.takeFirst();
                        result.first->disconnect(this);
                        if (result.second)
                            result.second->disconnect(this);
                        triggerRebuild();

                        pass = 2;
                        break;
                    }
                }
            }
        }
    }

    return result;
}

void QuickLauncher::shutDown()
{
    m_shuttingDown = true;
    bool waitForRemove = false;

    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        for (const auto &car : qAsConst(entry->m_containersAndRuntimes)) {
            if (car.second)
                car.second->stop();
            else if (car.first)
                car.first->deleteLater();

            if (car.first || car.second)
                waitForRemove = true;
        }
    }
    if (!waitForRemove)
        QMetaObject::invokeMethod(this, &QuickLauncher::shutDownFinished, Qt::QueuedConnection);
}

QT_END_NAMESPACE_AM

#include "moc_quicklauncher.cpp"
