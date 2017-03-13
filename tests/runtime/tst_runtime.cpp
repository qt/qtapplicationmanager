/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <QQmlEngine>

#include "application.h"
#include "yamlapplicationscanner.h"
#include "abstractruntime.h"
#include "runtimefactory.h"

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
    explicit TestRuntime(AbstractContainer *container, const Application *app, AbstractRuntimeManager *manager)
        : AbstractRuntime(container, app, manager)
    { }

    qint64 applicationProcessId() const
    {
        return m_state == AbstractRuntime::Active ? 1 : 0;
    }

public slots:
    bool start()
    {
        m_state = AbstractRuntime::Active;
        return true;
    }

    void stop(bool forceKill)
    {
        Q_UNUSED(forceKill);
        m_state = AbstractRuntime::Inactive;
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

    QVERIFY(rf->registerRuntime(new TestRuntimeManager(qSL("foo"), qApp)));
    QVERIFY(rf->runtimeIds() == QStringList() << qSL("foo"));

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

    Application *a = nullptr;
    try {
        a = YamlApplicationScanner().scan(temp.fileName());
    } catch (const Exception &e) {
        QVERIFY2(false, qPrintable(e.errorString()));
    }
    QVERIFY(a);

    AbstractRuntime *r = rf->create(nullptr, a);
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
        r->setInProcessQmlEngine(nullptr);
    }
    QVERIFY(r->start());
    QVERIFY(r->state() == AbstractRuntime::Active);
    QVERIFY(r->applicationProcessId() == 1);
    r->stop();
    QVERIFY(r->state() == AbstractRuntime::Inactive);
    QVERIFY(!r->securityToken().isEmpty());

    delete r;
    delete rf;
    delete a;
}

QTEST_MAIN(tst_Runtime)

#include "tst_runtime.moc"
