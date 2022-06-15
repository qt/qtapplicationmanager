// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "application.h"
#include "applicationinfo.h"
#include "packageinfo.h"
#include "package.h"
#include "abstractruntime.h"

QT_USE_NAMESPACE_AM

class TestRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit TestRuntime(AbstractContainer *container, Application *app, AbstractRuntimeManager *manager)
        : AbstractRuntime(container, app, manager)
    { }

    void setSlowAnimations(bool) override {}

    qint64 applicationProcessId() const override
    {
        return m_state == Am::Running ? 1 : 0;
    }

public slots:
    bool start() override
    {
        m_state = Am::Running;
        return true;
    }

    void stop(bool forceKill) override
    {
        Q_UNUSED(forceKill);
        m_state = Am::NotRunning;
    }
};

class TestRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT

public:
    TestRuntimeManager(const QString &id, QObject *parent)
        : AbstractRuntimeManager(id, parent)
    { }

    static QString defaultIdentifier() { return qSL("foo"); }

    bool inProcess() const override
    {
        return !AbstractRuntimeManager::inProcess();
    }

    TestRuntime *create(AbstractContainer *container, Application *app) override
    {
        return new TestRuntime(container, app, this);
    }
};

class tst_Application : public QObject
{
    Q_OBJECT

public:
    tst_Application(){}

private slots:
    void runtimeDestroyed();
};

// Checks that when the runtime of an application is destroyed
// the application no longer holds a reference to it
void tst_Application::runtimeDestroyed()
{
    auto pi = PackageInfo::fromManifest(qL1S(":/info.yaml"));
    auto pkg = new Package(pi);
    auto ai = new ApplicationInfo(pi);
    auto app = new Application(ai, pkg);

    auto runtimeManager = new TestRuntimeManager(qSL("foo"), qApp);
    auto runtime = runtimeManager->create(nullptr, app);

    app->setCurrentRuntime(runtime);

    QCOMPARE(app->currentRuntime(), runtime);

    delete runtime;

    QCOMPARE(app->currentRuntime(), nullptr);

    delete app;
    delete runtimeManager;
    delete ai;
    delete pkg;
    delete pi;
}

QTEST_APPLESS_MAIN(tst_Application)

#include "tst_application.moc"
