// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <memory>

#include <QtCore>
#include <QtTest>
#include <QStandardPaths>

#include <abstractcontainer.h>
#include <processcontainer.h>
#include <utilities.h>

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

// These tests drive a real 'process' container against a *faked* cgroup-v2 tree (redirected via
// setTestRootPathPrefix(), the same mechanism CGroupStatus uses). They verify the cgroup handling
// added for the 'createControlGroupPerProcess'/'defaultControlGroup' options: the process joins its
// cgroup by writing "0" into cgroup.procs at fork time, the group is created on demand, and the
// (now empty) directory is removed again once the process exits.
class tst_ProcessContainer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cgroupExistingGroup();
    void cgroupCreatedOnTheFly();
    void cgroupRemovalFailureIsReported();

private:
    static bool writeFile(const QString &path, const QByteArray &data);
    static QByteArray readFile(const QString &path);
    std::unique_ptr<QFileSystemWatcher> fakeKernel(const QString &baseDir, QString *nestedDir);

    QString m_sleep;
    QTemporaryDir m_root;
    QString m_cgroupRoot;
    int m_timeout = 5000;
};

bool tst_ProcessContainer::writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

QByteArray tst_ProcessContainer::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return { };
    return f.readAll();
}

// Plays the kernel's role for cgroups that are created on the fly: as soon as a new directory shows
// up below baseDir, it gets the cgroup.procs file the kernel would have created (the container's
// start() waits for it) and its path is reported back via nestedDir.
std::unique_ptr<QFileSystemWatcher> tst_ProcessContainer::fakeKernel(const QString &baseDir,
                                                                    QString *nestedDir)
{
    auto watcher = std::make_unique<QFileSystemWatcher>();
    if (!watcher->addPath(baseDir))
        return { };

    connect(watcher.get(), &QFileSystemWatcher::directoryChanged, this,
            [nestedDir](const QString &dir) {
        const auto subDirs = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : subDirs) {
            const QString procs = fi.absoluteFilePath() + u"/cgroup.procs"_s;
            if (!QFile::exists(procs)) {
                writeFile(procs, QByteArray());
                *nestedDir = fi.absoluteFilePath();
            }
        }
    });
    return watcher;
}

void tst_ProcessContainer::initTestCase()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("Needs setTestRootPathPrefix() from a developer build");
#elif !defined(Q_OS_LINUX)
    QSKIP("cgroups are only available on Linux");
#else
    m_sleep = QStandardPaths::findExecutable(u"sleep"_s);
    if (m_sleep.isEmpty())
        QSKIP("This test needs the 'sleep' executable to launch a real process");

    m_timeout *= timeoutFactor();

    QVERIFY(m_root.isValid());
    m_cgroupRoot = m_root.path() + u"/sys/fs/cgroup"_s;
    QVERIFY(QDir().mkpath(m_cgroupRoot));
    // the cgroup-v2 marker file makes the container enable its cgroup handling
    QVERIFY(writeFile(m_cgroupRoot + u"/cgroup.controllers"_s, "cpu memory\n"));

    // redirect the container's /sys/fs/cgroup access into our fake tree. This must happen before
    // the first container is constructed, as the v2 detection is cached on first use.
    setTestRootPathPrefix(m_root.path() + u"/"_s);
#endif
}

// defaultControlGroup only: the group already exists (as it would in a real system) and the process
// joins it. As this group is shared between all applications, it must survive the process exiting.
void tst_ProcessContainer::cgroupExistingGroup()
{
    const QString group = u"tst-existing.slice"_s;
    const QString groupDir = m_cgroupRoot + u"/"_s + group;
    QVERIFY(QDir().mkpath(groupDir));
    // an existing cgroup already has a (kernel-provided, initially empty) cgroup.procs
    QVERIFY(writeFile(groupDir + u"/cgroup.procs"_s, QByteArray()));

    ProcessContainerManager mgr;
    mgr.setConfiguration({ { u"defaultControlGroup"_s, group } });

    AbstractContainer *c = mgr.create(nullptr, { }, { }, { });
    QVERIFY(c);
    QVERIFY(c->setProgram(m_sleep));

    // the sleep duration only needs to outlive the test: the process is killed via stop() below
    AbstractContainerProcess *p = c->start({ u"60"_s }, { }, { });
    QVERIFY(p);

    QSignalSpy startedSpy(p, &AbstractContainerProcess::started);
    QVERIFY(startedSpy.wait(m_timeout));

    // the child moved itself into the cgroup by writing "0" to cgroup.procs before exec
    QTRY_COMPARE_WITH_TIMEOUT(readFile(groupDir + u"/cgroup.procs"_s), QByteArray("0\n"), m_timeout);

    // Drop the (virtual) cgroup.procs, mimicking the kernel emptying the group. This leaves an
    // empty directory, so an rmdir *would* succeed - which is exactly what must not happen here:
    // without createControlGroupPerProcess the group is shared, so it has to be left alone.
    QVERIFY(QFile::remove(groupDir + u"/cgroup.procs"_s));

    QSignalSpy finishedSpy(p, &AbstractContainerProcess::finished);
    p->stop(Am::ForcedExit);
    QVERIFY(finishedSpy.wait(m_timeout));

    // the container removes cgroups from its own finished handler, which was connected before this
    // test's spy, so a (bogus) removal would already have happened when the spy returns
    QVERIFY(QFileInfo::exists(groupDir));

    delete c;
}

// createControlGroupPerProcess: the container creates a fresh nested cgroup on the fly. The process
// joins it and the group is removed again on exit.
void tst_ProcessContainer::cgroupCreatedOnTheFly()
{
    const QString base = u"tst-onthefly.slice"_s;
    const QString baseDir = m_cgroupRoot + u"/"_s + base;
    QVERIFY(QDir().mkpath(baseDir)); // must exist up-front so the watcher can watch it

    QString nestedDir;
    auto watcher = fakeKernel(baseDir, &nestedDir);
    QVERIFY(watcher);

    ProcessContainerManager mgr;
    mgr.setConfiguration({ { u"defaultControlGroup"_s, base },
                           { u"createControlGroupPerProcess"_s, true } });

    AbstractContainer *c = mgr.create(nullptr, { }, { }, { });
    QVERIFY(c);
    QVERIFY(c->setProgram(m_sleep));

    AbstractContainerProcess *p = c->start({ u"60"_s }, { }, { });
    QVERIFY(p);

    QSignalSpy startedSpy(p, &AbstractContainerProcess::started);
    QVERIFY(startedSpy.wait(m_timeout));

    // a new nested cgroup was created automatically and the child joined it
    QVERIFY(!nestedDir.isEmpty());
    QVERIFY(QFileInfo::exists(nestedDir));
    QTRY_COMPARE_WITH_TIMEOUT(readFile(nestedDir + u"/cgroup.procs"_s), QByteArray("0\n"), m_timeout);

    // emptying the group lets the finished handler remove the per-process directory - in contrast
    // to the shared group in cgroupExistingGroup(), this one is exclusive to the process
    QVERIFY(QFile::remove(nestedDir + u"/cgroup.procs"_s));

    QSignalSpy finishedSpy(p, &AbstractContainerProcess::finished);
    p->stop(Am::ForcedExit);
    QVERIFY(finishedSpy.wait(m_timeout));

    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(nestedDir), m_timeout);
    // ... while the shared parent group it was nested in is left untouched
    QVERIFY(QFileInfo::exists(baseDir));

    delete c;
}

// A cgroup that cannot be removed (here: because it is not empty) must not fail silently.
void tst_ProcessContainer::cgroupRemovalFailureIsReported()
{
    const QString base = u"tst-removefail.slice"_s;
    const QString baseDir = m_cgroupRoot + u"/"_s + base;
    QVERIFY(QDir().mkpath(baseDir));

    QString nestedDir;
    auto watcher = fakeKernel(baseDir, &nestedDir);
    QVERIFY(watcher);

    ProcessContainerManager mgr;
    mgr.setConfiguration({ { u"defaultControlGroup"_s, base },
                           { u"createControlGroupPerProcess"_s, true } });

    AbstractContainer *c = mgr.create(nullptr, { }, { }, { });
    QVERIFY(c);
    QVERIFY(c->setProgram(m_sleep));

    AbstractContainerProcess *p = c->start({ u"60"_s }, { }, { });
    QVERIFY(p);

    QSignalSpy startedSpy(p, &AbstractContainerProcess::started);
    QVERIFY(startedSpy.wait(m_timeout));
    QVERIFY(!nestedDir.isEmpty());

    // In contrast to the other tests, cgroup.procs is left in place: the directory is not empty,
    // so the rmdir on exit fails and the container has to report that.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Failed to remove cgroup"_s));

    QSignalSpy finishedSpy(p, &AbstractContainerProcess::finished);
    p->stop(Am::ForcedExit);
    QVERIFY(finishedSpy.wait(m_timeout));

    delete c;
}

QTEST_GUILESS_MAIN(tst_ProcessContainer)

#include "tst_processcontainer.moc"
