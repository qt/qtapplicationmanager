// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>

#include "cgrouptesthelper.h"

#if defined(QT_BUILD_INTERNAL) && defined(Q_OS_LINUX)
#  include <utilities.h>
using namespace QtAM;
#endif

using namespace Qt::StringLiterals;

CGroupTestHelper::CGroupTestHelper(QObject *parent)
    : QObject(parent)
{ }

CGroupTestHelper::~CGroupTestHelper()
{
    cleanup();
}

bool CGroupTestHelper::setup(const QString &baseGroup)
{
#if !defined(QT_BUILD_INTERNAL) || !defined(Q_OS_LINUX)
    Q_UNUSED(baseGroup)
    return false;
#else
    if (!m_root.isValid())
        return false;

    m_cgroupRoot = m_root.path() + u"/sys/fs/cgroup"_s;
    if (!QDir().mkpath(m_cgroupRoot))
        return false;

    // cgroup-v2 marker so the container enables its cgroup handling
    QFile controllers(m_cgroupRoot + u"/cgroup.controllers"_s);
    if (!controllers.open(QIODevice::WriteOnly))
        return false;
    controllers.write("cpu memory\n");
    controllers.close();

    // The parent group must exist up-front so the watcher can watch it for new sub-groups.
    const QString baseDir = m_cgroupRoot + u"/"_s + baseGroup;
    if (!QDir().mkpath(baseDir))
        return false;

    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(baseDir);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &dir) {
        const auto subDirs = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : subDirs) {
            const QString procs = fi.absoluteFilePath() + u"/cgroup.procs"_s;
            if (!QFile::exists(procs)) {
                // Play the kernel: a freshly created cgroup directory comes with a cgroup.procs.
                QFile f(procs);
                if (f.open(QIODevice::WriteOnly)) {
                    f.close();
                    m_lastCreatedGroup = QDir(m_cgroupRoot).relativeFilePath(fi.absoluteFilePath());
                }
            }
        }
    });

    // Redirect the containers' /sys/fs/cgroup access into our fake tree. Must happen before the
    // first container is constructed (its cgroup-v2 detection reads this).
    setTestRootPathPrefix(m_root.path() + u"/"_s);
    return true;
#endif
}

void CGroupTestHelper::cleanup()
{
#if defined(QT_BUILD_INTERNAL) && defined(Q_OS_LINUX)
    setTestRootPathPrefix(QString());
#endif
    delete m_watcher;
    m_watcher = nullptr;
}

QString CGroupTestHelper::lastCreatedGroup() const
{
    return m_lastCreatedGroup;
}

bool CGroupTestHelper::groupExists(const QString &name) const
{
    return !m_cgroupRoot.isEmpty() && QFileInfo::exists(m_cgroupRoot + u"/"_s + name);
}

QString CGroupTestHelper::readProcs(const QString &name) const
{
    QFile f(m_cgroupRoot + u"/"_s + name + u"/cgroup.procs"_s);
    if (!f.open(QIODevice::ReadOnly))
        return { };
    return QString::fromLocal8Bit(f.readAll());
}

bool CGroupTestHelper::removeProcs(const QString &name)
{
    return QFile::remove(m_cgroupRoot + u"/"_s + name + u"/cgroup.procs"_s);
}
