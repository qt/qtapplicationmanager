/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QtTest>
#include <QQmlEngine>

#include "application.h"
#include "abstractruntime.h"
#include "runtimefactory.h"

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
    explicit TestRuntime(AbstractContainer *container, const Application *app, AbstractRuntimeManager *manager)
        : AbstractRuntime(container, app, manager)
        , m_running(false)
    { }

    State state() const
    {
        return m_running ? Active : Inactive;
    }

    qint64 applicationProcessId() const
    {
        return m_running ? 1 : 0;
    }

public slots:
    bool start()
    {
        m_running = true;
        return true;
    }

    void stop(bool forceKill)
    {
        Q_UNUSED(forceKill);
        m_running = false;
    }

private:
    bool m_running;

};

class TestRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT

public:
    TestRuntimeManager(const QString &id, QObject *parent)
        : AbstractRuntimeManager(id, parent)
    { }

    static QString defaultIdentifier() { return "foo"; }

    bool inProcess() const
    {
        return !AbstractRuntimeManager::inProcess();
    }

    TestRuntime *create(AbstractContainer *container, const Application *app)
    {
        return new TestRuntime(container, app, this);
    }
};


tst_Runtime::tst_Runtime()
{ }

void tst_Runtime::factory()
{
    RuntimeFactory *rf = RuntimeFactory::instance();

    QVERIFY(rf);
    QVERIFY(rf == RuntimeFactory::instance());
    QVERIFY(rf->runtimeIds().isEmpty());

    QVERIFY(rf->registerRuntime<TestRuntimeManager>());
    QVERIFY(rf->runtimeIds() == QStringList() << "foo");

    QVERIFY(!rf->create(0, 0));

    QVariantMap map;
    map.insert("id", "com.foo.test");
    map.insert("codeFilePath", "test.foo");
    map.insert("runtimeName", "foo");
    map.insert("displayIcon", "icon.png");
    map.insert("displayName", QVariantMap { {"en", QString::fromLatin1("Foo") } });
    QString error;
    Application *a = Application::fromVariantMap(map, &error);
    QVERIFY2(a, qPrintable(error));

    AbstractRuntime *r = rf->create(0, a);
    QVERIFY(r);
    QVERIFY(r->application() == a);
    QVERIFY(r->manager()->inProcess());
    QVERIFY(r->state() == AbstractRuntime::Inactive);
    QVERIFY(r->applicationProcessId() == 0);
    {
        QScopedPointer<QQmlEngine> engine(new QQmlEngine());
        QVERIFY(!r->inProcessQmlEngine());
        r->setInProcessQmlEngine(engine.data());
        QVERIFY(r->inProcessQmlEngine() == engine.data());
        r->setInProcessQmlEngine(0);
    }
    QVERIFY(r->start());
    QVERIFY(r->state() == AbstractRuntime::Active);
    QVERIFY(r->applicationProcessId() == 1);
    r->stop();
    QVERIFY(r->state() == AbstractRuntime::Inactive);
    QVERIFY(!r->securityToken().isEmpty());

    delete r;
    delete rf;
}

QTEST_MAIN(tst_Runtime)

#include "tst_runtime.moc"
