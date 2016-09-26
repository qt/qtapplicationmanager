/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QCoreApplication>
#include <QTimer>

#include "abstractcontainer.h"
#include "abstractruntime.h"
#include "containerfactory.h"
#include "runtimefactory.h"
#include "quicklauncher.h"
#include "systemmonitor.h"

AM_BEGIN_NAMESPACE

QuickLauncher *QuickLauncher::s_instance = nullptr;

QuickLauncher *QuickLauncher::instance()
{
    if (!s_instance)
        s_instance = new QuickLauncher();
    return s_instance;
}

QuickLauncher::QuickLauncher(QObject *parent)
    : QObject(parent)
{ }

QuickLauncher::~QuickLauncher()
{ }

void QuickLauncher::initialize(int runtimesPerContainer, qreal idleLoad)
{
    ContainerFactory *cf = ContainerFactory::instance();
    RuntimeFactory *rf = RuntimeFactory::instance();

    foreach (const QString &containerId, cf->containerIds()) {
        if (!cf->manager(containerId)->supportsQuickLaunch())
            continue;

        foreach (const QString &runtimeId, rf->runtimeIds()) {
            if (rf->manager(runtimeId)->inProcess())
                continue;

            QuickLaunchEntry entry;
            entry.m_containerId = containerId;
            entry.m_maximum = runtimesPerContainer;

            if (rf->manager(runtimeId)->supportsQuickLaunch())
                entry.m_runtimeId = runtimeId;

            m_quickLaunchPool << entry;

            qCDebug(LogSystem) << "Created quick-launch slot for" << containerId + qSL("/") + runtimeId;
        }
    }
    if (idleLoad > 0) {
        SystemMonitor::instance()->setIdleLoadAverage(idleLoad);
        m_onlyRebuildWhenIdle = true;
        connect(SystemMonitor::instance(), &SystemMonitor::idleChanged, this, &QuickLauncher::rebuild);
    }
    triggerRebuild();
}

void QuickLauncher::rebuild()
{
    if (m_onlyRebuildWhenIdle) {
        if (!SystemMonitor::instance()->isIdle())
            return;
    }

    int todo = 0;
    int done = 0;

    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        if (entry->m_containersAndRuntimes.size() < entry->m_maximum) {
            todo += (entry->m_maximum - entry->m_containersAndRuntimes.size());
            if (done >= 1)
                continue;

            QScopedPointer<AbstractContainer> ac(ContainerFactory::instance()->create(entry->m_containerId));
            if (!ac) {
                qCWarning(LogSystem) << "ERROR: Could not create quick-launch container with id"
                                     << entry->m_containerId;
                continue;
            }

            QScopedPointer<AbstractRuntime> ar;
            if (!entry->m_runtimeId.isEmpty()) {
                ar.reset(RuntimeFactory::instance()->createQuickLauncher(ac.take(), entry->m_runtimeId));
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
            AbstractContainer *container = ar ? ar.data()->container() : ac.take();
            AbstractRuntime *runtime = ar.take();

            connect(container, &AbstractContainer::destroyed, this, [this, container]() { removeEntry(container, nullptr); });
            if (runtime)
                connect(runtime, &AbstractRuntime::destroyed, this, [this, runtime]() { removeEntry(nullptr, runtime); });

            qCDebug(LogSystem) << "Added" << entry->m_containerId << "/" << entry->m_runtimeId <<
                                  "to the quick-launch pool ->" << container << runtime;
            entry->m_containersAndRuntimes << qMakePair(container, runtime);
            ++done;
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
    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        for (int i = 0; i < entry->m_containersAndRuntimes.size(); ++i) {
            auto car = entry->m_containersAndRuntimes.at(i);
            if ((container && car.first == container)
                    || (runtime && car.second == runtime)) {
                qCDebug(LogSystem) << "Removed quicklaunch entry for container/runtime" << container << runtime;

                entry->m_containersAndRuntimes.removeAt(i--);
            }
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

void QuickLauncher::killAll()
{
    for (auto entry = m_quickLaunchPool.begin(); entry != m_quickLaunchPool.end(); ++entry) {
        for (const auto &car : entry->m_containersAndRuntimes) {
            if (car.second) {
                car.second->stop(true);
                delete car.second;
            }
        }
        entry->m_containersAndRuntimes.clear();
    }
    m_quickLaunchPool.clear();
}

AM_END_NAMESPACE
