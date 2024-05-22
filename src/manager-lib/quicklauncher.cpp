// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QTimer>
#include <QDateTime>
#include <QMetaObject>

#include "logging.h"
#include "abstractcontainer.h"
#include "abstractruntime.h"
#include "containerfactory.h"
#include "runtimefactory.h"
#include "quicklauncher.h"
#include "systemreader.h"

#include <memory>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

QuickLauncher *QuickLauncher::s_instance = nullptr;

QuickLauncher *QuickLauncher::createInstance(const QMap<std::pair<QString, QString>, int> &runtimesPerContainer,
                                             qreal idleLoad, int failedStartLimit,
                                             int failedStartLimitIntervalSec)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("QuickLauncher instance already exists");

    s_instance = new QuickLauncher(runtimesPerContainer, idleLoad, failedStartLimit,
                                   failedStartLimitIntervalSec);
    return s_instance;
}

QuickLauncher *QuickLauncher::instance()
{
    return s_instance;
}

QuickLauncher::~QuickLauncher()
{
    if (m_idleTimerId)
        killTimer(m_idleTimerId);
    delete m_idleCpu;
    s_instance = nullptr;
}

QuickLauncher::QuickLauncher(const QMap<std::pair<QString, QString>, int> &runtimesPerContainer,
                             qreal idleLoad, int failedStartLimit, int failedStartLimitIntervalSec,
                             QObject *parent)
    : QObject(parent)
    , m_failedStartLimit(qMax(0, failedStartLimit))
    , m_failedStartLimitIntervalSec(qMax(0, failedStartLimitIntervalSec))
{
    auto findMaximum = [&runtimesPerContainer](const QuickLaunchEntry &qle) -> int {
        static const QString anyId = u"*"_s;

        for (const auto &key : { std::make_pair(qle.m_containerId, qle.m_runtimeId),
                               std::make_pair(qle.m_containerId, anyId),
                               std::make_pair(anyId, qle.m_runtimeId),
                               std::make_pair(anyId, anyId) }) {
            auto it = runtimesPerContainer.find(key);
            if (it != runtimesPerContainer.end())
                return it.value();
        }
        return 0;
    };

    ContainerFactory *cf = ContainerFactory::instance();
    RuntimeFactory *rf = RuntimeFactory::instance();

    qCDebug(LogQuickLaunch) << "Setting up the quick-launch pool:";

    const QStringList allContainerIds = cf->containerIds();
    for (const QString &containerId : allContainerIds) {
        if (!cf->manager(containerId)->supportsQuickLaunch()) {
            qCDebug(LogQuickLaunch).noquote() << " * container:" << containerId << "does not support quick-launch";
            continue;
        }

        bool foundRuntimeWithoutQuickLaunch = false;

        const QStringList allRuntimeIds = rf->runtimeIds();
        for (const QString &runtimeId : allRuntimeIds) {
            if (rf->manager(runtimeId)->inProcess())
                continue;

            QuickLaunchEntry entry;
            entry.m_containerId = containerId;

            if (rf->manager(runtimeId)->supportsQuickLaunch()) {
                entry.m_runtimeId = runtimeId;
            } else {
                // we can still quick-launch a container without a runtime
                if (foundRuntimeWithoutQuickLaunch)
                    continue;
                foundRuntimeWithoutQuickLaunch = true;
            }

            // if you need more than 10 quicklaunchers per runtime, you're probably doing something wrong
            // or you have a typo in your YAML, which could potentially freeze your target (container
            // construction can be expensive)
            entry.m_maximum = qBound(0, findMaximum(entry), 10);

            if (!entry.m_maximum)
                continue;

            m_quickLaunchPool << entry;

            qCDebug(LogQuickLaunch).nospace().noquote()
                << " * container: " << entry.m_containerId << " / runtime: "
                << (entry.m_runtimeId.isEmpty() ? u"(none)"_s : entry.m_runtimeId)
                << " [at max: " << entry.m_maximum << "]";
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
        if (entry->m_disabled)
            continue;

        if (entry->m_containersAndRuntimes.size() < entry->m_maximum) {
            todo += (entry->m_maximum - entry->m_containersAndRuntimes.size());
            if (done >= 1)
                continue;

            std::unique_ptr<AbstractContainer> ac(ContainerFactory::instance()->create(entry->m_containerId, nullptr));
            if (!ac) {
                entry->addFailure();
                qCWarning(LogQuickLaunch) << "ERROR: Could not create quick-launch container"
                                          << entry->m_containerId;
                continue;
            }

            std::unique_ptr<AbstractRuntime> ar;
            if (!entry->m_runtimeId.isEmpty()) {
                ar.reset(RuntimeFactory::instance()->createQuickLauncher(ac.release(), entry->m_runtimeId));
                if (!ar) {
                    entry->addFailure();
                    qCWarning(LogQuickLaunch) << "ERROR: Could not create quick-launch runtime"
                                              << entry->m_runtimeId << "within container"
                                              << entry->m_containerId;
                    continue;
                }
                if (!ar->start()) {
                    entry->addFailure();
                    qCWarning(LogQuickLaunch) << "ERROR: Could not start quick-launch runtime"
                                              << entry->m_runtimeId << "within container"
                                              << entry->m_containerId;
                    continue;
                }
            }
            AbstractContainer *container = ar ? ar.get()->container() : ac.release();
            AbstractRuntime *runtime = ar.release();

            if (container->process()) {
                connect(container->process(), &AbstractContainerProcess::finished,
                        this, [this, container, runtime](int exitCode, Am::ExitStatus status) {
                    if ((status == Am::CrashExit) || ((status == Am::NormalExit) && exitCode)) {
                        for (auto e = m_quickLaunchPool.begin(); e != m_quickLaunchPool.end(); ++e) {
                            for (int i = 0; i < e->m_containersAndRuntimes.size(); ++i) {
                                auto car = e->m_containersAndRuntimes.at(i);
                                if ((car.first == container) && (car.second == runtime)) {
                                    e->addFailure();
                                    checkFailedStarts();
                                    break;
                                }
                            }
                        }
                    }
                });
            }
            connect(container, &AbstractContainer::destroyed,
                    this, [this, container]() {
                removeEntry(container, nullptr);
            });
            if (runtime) {
                connect(runtime, &AbstractRuntime::destroyed,
                        this, [this, runtime]() {
                    removeEntry(nullptr, runtime);
                });
            }

            entry->m_containersAndRuntimes << qMakePair(container, runtime);
            ++done;

            qCDebug(LogQuickLaunch) << "Added new quick-launch entry for container:"
                                    << entry->m_containerId << "/ runtime:"
                                    << (entry->m_runtimeId.isEmpty() ? u"(none)"_s : entry->m_runtimeId);
        }
    }

    checkFailedStarts();

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
                qCDebug(LogQuickLaunch) << "Removed quick-launch entry for container:"
                                        << entry->m_containerId << "/ runtime:"
                                        << (entry->m_runtimeId.isEmpty() ? u"(none)"_s : entry->m_runtimeId);

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

void QuickLauncher::checkFailedStarts()
{
    qint64 intervalStart = QDateTime::currentMSecsSinceEpoch() - (m_failedStartLimitIntervalSec * 1000);

    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        if (entry->m_disabled)
            continue;

        entry->m_failedTimeStamps.removeIf([=](qint64 ts) { return ts < intervalStart; });

        if (m_failedStartLimit && m_failedStartLimitIntervalSec
                && (entry->m_failedTimeStamps.size() >= m_failedStartLimit)) {
            qCWarning(LogQuickLaunch) << "Disabling quick-launch for container:"
                                      << entry->m_containerId << "/ runtime:"
                                      << (entry->m_runtimeId.isEmpty() ? u"(none)"_s : entry->m_runtimeId)
                                      << "due to too many failed start attempts:"
                                      << entry->m_failedTimeStamps.size() << "failed starts within"
                                      << m_failedStartLimitIntervalSec << "seconds";
            entry->m_disabled = true;
            entry->m_failedTimeStamps.clear();
        }
    }
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
        for (const auto &car : std::as_const(entry->m_containersAndRuntimes)) {
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

void QuickLauncher::QuickLaunchEntry::addFailure()
{
    m_failedTimeStamps << QDateTime::currentMSecsSinceEpoch();
}

QT_END_NAMESPACE_AM

#include "moc_quicklauncher.cpp"
