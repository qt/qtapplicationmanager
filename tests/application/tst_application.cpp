/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "application.h"
#include "applicationinfo.h"
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

    TestRuntime *create(AbstractContainer *container, Application *app)
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
    auto app = new Application(new ApplicationInfo);

    auto runtimeManager = new TestRuntimeManager(qSL("foo"), qApp);
    auto runtime = runtimeManager->create(nullptr, app);

    app->setCurrentRuntime(runtime);

    QCOMPARE(app->currentRuntime(), runtime);

    delete runtime;

    QCOMPARE(app->currentRuntime(), nullptr);

    delete app;
    delete runtimeManager;
}

QTEST_APPLESS_MAIN(tst_Application)

#include "tst_application.moc"
