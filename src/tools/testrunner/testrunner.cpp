/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include "testrunner.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

#include <QtQml/qqmlpropertymap.h>
#include <QtTest/qtestsystem.h>
#include <private/quicktestresult_p.h>

QT_BEGIN_NAMESPACE
namespace QTest {
    extern Q_TESTLIB_EXPORT bool printAvailableFunctions;
}
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_AM

class QTestRootObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool windowShown READ windowShown NOTIFY windowShownChanged)
    Q_PROPERTY(bool hasTestCase READ hasTestCase WRITE setHasTestCase NOTIFY hasTestCaseChanged)
    Q_PROPERTY(QObject *defined READ defined)
public:
    QTestRootObject(QObject *parent = 0)
        : QObject(parent)
        , m_windowShown(false)
        , m_hasTestCase(false)
        , m_hasQuit(false)
        , m_defined(new QQmlPropertyMap(this))
    {
#if defined(QT_OPENGL_ES_2_ANGLE)
        m_defined->insert(QLatin1String("QT_OPENGL_ES_2_ANGLE"), QVariant(true));
#endif
    }

    static QTestRootObject *instance()
    {
        static QPointer<QTestRootObject> object = new QTestRootObject;
        if (!object) {
            qWarning("A new test root object has been created, the behavior may be compromised");
            object = new QTestRootObject;
        }
        return object;
    }

    bool hasQuit() const { return m_hasQuit; }

    bool hasTestCase() const { return m_hasTestCase; }
    void setHasTestCase(bool value) { m_hasTestCase = value; emit hasTestCaseChanged(); }

    bool windowShown() const { return m_windowShown; }
    void setWindowShown(bool value) { m_windowShown = value; emit windowShownChanged(); }
    QQmlPropertyMap *defined() const { return m_defined; }

    void init() { setWindowShown(false); setHasTestCase(false); m_hasQuit = false; }

Q_SIGNALS:
    void windowShownChanged();
    void hasTestCaseChanged();

private Q_SLOTS:
    void quit() { m_hasQuit = true; }

private:
    bool m_windowShown;
    bool m_hasTestCase;
    bool m_hasQuit;
    QQmlPropertyMap *m_defined;
    friend class TestRunner;
};

static QObject *testRootObject(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(jsEngine);
    return QTestRootObject::instance();
}

void TestRunner::initialize(const QStringList &testRunnerArguments)
{
    Q_ASSERT(!testRunnerArguments.isEmpty());

    // Convert all the arguments back into a char * array.
    // These need to be alive as long as the program is running!
    static QVector<char *> testArgV;
    for (const auto &arg : testRunnerArguments)
        testArgV << strdup(arg.toLocal8Bit().constData());
    atexit([]() { std::for_each(testArgV.constBegin(), testArgV.constEnd(), free); });

    QuickTestResult::setCurrentAppname(testArgV.constFirst());
    QuickTestResult::setProgramName(testArgV.constFirst());
    QuickTestResult::parseArgs(testArgV.size(), testArgV.data());
    qputenv("QT_QTESTLIB_RUNNING", "1");

    // Register the test object
    qmlRegisterSingletonType<QTestRootObject>("Qt.test.qtestroot", 1, 0, "QTestRootObject", testRootObject);

    QTestRootObject::instance()->init();
}

int TestRunner::exec(QQmlEngine *engine)
{
    QEventLoop eventLoop;
    QObject::connect(engine, &QQmlEngine::quit,
                     QTestRootObject::instance(), &QTestRootObject::quit);
    QObject::connect(engine, &QQmlEngine::quit,
                     &eventLoop, &QEventLoop::quit);

    QTestRootObject::instance()->setWindowShown(true);

    if (QTest::printAvailableFunctions)
        return 0;

    if (!QTestRootObject::instance()->hasQuit() && QTestRootObject::instance()->hasTestCase())
        eventLoop.exec();

    QuickTestResult::setProgramName(0);

    return QuickTestResult::exitCode();
}

QT_END_NAMESPACE_AM

#include "testrunner.moc"
