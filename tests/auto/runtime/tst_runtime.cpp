// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

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

private slots:
    void factory();
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

    QVERIFY(rf->registerRuntime(new TestRuntimeManager(u"foo"_s, qApp)));
    QVERIFY(rf->runtimeIds() == QStringList() << u"foo"_s);

    QVERIFY(!rf->create(nullptr, nullptr));

    QByteArray yaml =
            "formatVersion: 1\n"
            "formatType: am-application\n"
            "---\n"
            "id: com.pelagicore.test\n"
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
    r->stop();
    QVERIFY(r->state() == Am::NotRunning);
    QVERIFY(!r->securityToken().isEmpty());
}

QTEST_MAIN(tst_Runtime)

#include "tst_runtime.moc"
