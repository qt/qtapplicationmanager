// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <algorithm>
#include <memory>

#include <QtTest>
#include <QQmlEngine>

#include "application.h"
#include "package.h"
#include "abstractruntime.h"
#include "runtimefactory.h"
#include "exception.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Runtime : public QObject
{
    Q_OBJECT

public:
    tst_Runtime();

private Q_SLOTS:
    void factory();
    void aliases();
};

class TestRuntimeManager;

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

public Q_SLOTS:
    bool start() override
    {
        m_state = Am::Running;
        return true;
    }

    void stop(Am::ExitStatus exitStatus) override
    {
        Q_UNUSED(exitStatus)
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

    static QString defaultIdentifier() { return u"foo"_s; }

    bool inProcess() const override
    {
        return !AbstractRuntimeManager::inProcess();
    }

    TestRuntime *create(AbstractContainer *container, Application *app) override
    {
        return new TestRuntime(container, app, this);
    }
};


tst_Runtime::tst_Runtime()
{ }

void tst_Runtime::factory()
{
    std::unique_ptr<RuntimeFactory> rf { RuntimeFactory::instance() };

    QVERIFY(rf);
    QVERIFY(rf.get() == RuntimeFactory::instance());
    QVERIFY(rf->runtimeIds().isEmpty());

    QVERIFY(rf->registerRuntime(new TestRuntimeManager(u"foo"_s, QCoreApplication::instance())));
    QVERIFY(rf->runtimeIds() == QStringList() << u"foo"_s);

    QVERIFY(!rf->create(nullptr, nullptr));

    QByteArray yaml =
            "formatVersion: 1\n"
            "formatType: am-application\n"
            "---\n"
            "id: test-pkg\n"
            "name: { en_US: 'Test' }\n"
            "icon: icon.png\n"
            "code: test.foo\n"
            "runtime: foo\n";

    QTemporaryFile temp;
    QVERIFY(temp.open());
    QCOMPARE(temp.write(yaml), yaml.size());
    temp.close();

    std::unique_ptr<Application> a;
    std::unique_ptr<PackageInfo> pi;
    std::unique_ptr<Package> p;
    try {
        pi.reset(PackageInfo::fromManifest(temp.fileName()));
        QVERIFY(pi);
        p.reset(new Package(pi.get()));
        a.reset(new Application(pi->applications().constFirst(), p.get()));
    } catch (const Exception &e) {
        QVERIFY2(false, qPrintable(e.errorString()));
    }
    QVERIFY(a);

    std::unique_ptr<AbstractRuntime> r { rf->create(nullptr, a.get()) };
    QVERIFY(r);
    QVERIFY(r->application() == a.get());
    QVERIFY(r->manager()->inProcess());
    QVERIFY(r->state() == Am::NotRunning);
    QVERIFY(r->applicationProcessId() == 0);
    {
        std::unique_ptr<QQmlEngine> engine(new QQmlEngine());
        QVERIFY(!r->inProcessQmlEngine());
        r->setInProcessQmlEngine(engine.get());
        QVERIFY(r->inProcessQmlEngine() == engine.get());
        r->setInProcessQmlEngine(nullptr);
    }
    QVERIFY(r->start());
    QVERIFY(r->state() == Am::Running);
    QVERIFY(r->applicationProcessId() == 1);
    r->stop(Am::NormalExit);
    QVERIFY(r->state() == Am::NotRunning);
    QVERIFY(!r->securityToken().isEmpty());
}

void tst_Runtime::aliases()
{
    std::unique_ptr<RuntimeFactory> rf { RuntimeFactory::instance() };
    QVERIFY(rf);

    QVERIFY(rf->registerRuntime(new TestRuntimeManager(u"foo"_s, QCoreApplication::instance())));

    QVERIFY(!rf->registerRuntimeAlias({ }, u"foo"_s));       // empty alias
    QVERIFY(!rf->registerRuntimeAlias(u"foo"_s, u"foo"_s));  // collides with a runtime id
    QVERIFY(!rf->registerRuntimeAlias(u"bar"_s, u"baz"_s));  // aliased runtime is not registered
    QVERIFY(rf->registerRuntimeAlias(u"bar"_s, u"foo"_s));
    QVERIFY(!rf->registerRuntimeAlias(u"bar"_s, u"foo"_s));  // duplicate alias

    // a runtime id cannot collide with an alias
    TestRuntimeManager barManager(u"bar"_s, nullptr);
    QVERIFY(!rf->registerRuntime(&barManager));

    QCOMPARE(rf->resolveRuntimeId(u"bar"_s), u"foo"_s);
    QCOMPARE(rf->resolveRuntimeId(u"foo"_s), u"foo"_s);
    QCOMPARE(rf->resolveRuntimeId(u"baz"_s), u"baz"_s);

    QVERIFY(rf->manager(u"foo"_s));
    QCOMPARE(rf->manager(u"bar"_s), rf->manager(u"foo"_s));

    QStringList ids = rf->runtimeIds();
    std::sort(ids.begin(), ids.end());
    QCOMPARE(ids, (QStringList { u"bar"_s, u"foo"_s }));

    // configurations are keyed on canonical runtime ids only - alias keys are ignored
    rf->setConfiguration({ { u"bar"_s, QVariantMap { { u"x"_s, 1 } } },
                           { u"foo"_s, QVariantMap { { u"x"_s, 2 } } } });
    QCOMPARE(rf->manager(u"bar"_s)->configuration().value(u"x"_s).toInt(), 2);

    rf->setConfiguration({ { u"bar"_s, QVariantMap { { u"x"_s, 1 } } } });
    QVERIFY(rf->manager(u"foo"_s)->configuration().isEmpty());
}

QTEST_GUILESS_MAIN(tst_Runtime)

#include "tst_runtime.moc"
