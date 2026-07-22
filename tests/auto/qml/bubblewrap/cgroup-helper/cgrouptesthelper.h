// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CGROUPTESTHELPER_H
#define CGROUPTESTHELPER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtQml/QQmlEngine>

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)

// A tiny, test-only helper that lives in the System-UI (appman) process. It fakes a cgroup-v2 tree
// in a temporary directory and redirects the containers' /sys/fs/cgroup access to it via
// setTestRootPathPrefix(). For the "created on the fly" case it also plays the kernel's role: a
// QFileSystemWatcher creates cgroup.procs inside a freshly created cgroup directory, which the
// container's (test-only) delayed start waits for before the child joins the group.
class CGroupTestHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit CGroupTestHelper(QObject *parent = nullptr);
    ~CGroupTestHelper() override;

    // Set up the fake cgroup tree and start watching baseGroup for on-the-fly sub-groups.
    // Returns false when this isn't a developer build on Linux (the test should then skip).
    Q_INVOKABLE bool setup(const QString &baseGroup);
    Q_INVOKABLE void cleanup();

    // The cgroup (relative to /sys/fs/cgroup) most recently created by the watched container.
    Q_INVOKABLE QString lastCreatedGroup() const;
    Q_INVOKABLE bool groupExists(const QString &name) const;
    Q_INVOKABLE QString readProcs(const QString &name) const;
    Q_INVOKABLE bool removeProcs(const QString &name);

private:
    QString m_cgroupRoot;   // <tmp>/sys/fs/cgroup
    QString m_lastCreatedGroup;
    QTemporaryDir m_root;
    QFileSystemWatcher *m_watcher = nullptr;
};

#endif // CGROUPTESTHELPER_H
