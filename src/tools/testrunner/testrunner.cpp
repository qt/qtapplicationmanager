/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
#include <QQmlEngine>
#include <QRegExp>
#include <QRegularExpression>

#include <qlogging.h>
#include <QtTest/qtestsystem.h>
#include <private/quicktestresult_p.h>
#include <private/qtestlog_p.h>
#include "testrunner_p.h"

#include "utilities.h"

QT_BEGIN_NAMESPACE
namespace QTest {
    extern Q_TESTLIB_EXPORT bool printAvailableFunctions;
    extern Q_TESTLIB_EXPORT void qtest_qParseArgs(int argc, char *argv[], bool qml);
}
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_AM

AmTest::AmTest()
{}

AmTest *AmTest::instance()
{
    static QPointer<AmTest> object = new AmTest;
    if (!object) {
        qWarning("A new appman test object has been created, the behavior may be compromised");
        object = new AmTest;
    }
    return object;
}

int AmTest::timeoutFactor() const
{
    return QT_PREPEND_NAMESPACE_AM(timeoutFactor)();
}

static QtMsgType convertMsgType(AmTest::MsgType type)
{
    QtMsgType ret;

    switch (type) {
    case AmTest::WarningMsg: ret = QtWarningMsg; break;
    case AmTest::CriticalMsg: ret = QtCriticalMsg; break;
    case AmTest::FatalMsg: ret = QtFatalMsg; break;
    case AmTest::InfoMsg: ret = QtInfoMsg; break;
    default: ret = QtDebugMsg;
    }
    return ret;
}

void AmTest::ignoreMessage(MsgType type, const char *msg)
{
    QTestLog::ignoreMessage(convertMsgType(type), msg);
}

void AmTest::ignoreMessage(MsgType type, const QRegExp &expression)
{
#ifndef QT_NO_REGULAREXPRESSION
    QRegularExpression re(expression.pattern());
    if (expression.caseSensitivity() == Qt::CaseInsensitive)
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QTestLog::ignoreMessage(convertMsgType(type), re);
#else
    Q_UNUSED(type);
    Q_UNUSED(expression);
    qWarning() << "Cannot ignore message: regular expressions are not supported";
#endif
}


QTestRootObject::QTestRootObject(QObject *parent)
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

QTestRootObject *QTestRootObject::instance()
{
    static QPointer<QTestRootObject> object = new QTestRootObject;
    if (!object) {
        qWarning("A new test root object has been created, the behavior may be compromised");
        object = new QTestRootObject;
    }
    return object;
}




static QObject *testRootObject(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(jsEngine);
    return QTestRootObject::instance();
}

static QObject *amTest(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(jsEngine);
    return AmTest::instance();
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

    // Allocate a QuickTestResult to create QBenchmarkGlobalData, otherwise the benchmark options don't work
    QuickTestResult result;
    // We would call QuickTestResult::parseArgs here, but that would include qml options in the help
    // which we don't support
    QTest::qtest_qParseArgs(testArgV.size(), testArgV.data(), false /*no qml options*/);
    qputenv("QT_QTESTLIB_RUNNING", "1");

    // Register the test object and application-manager test add-on
    qmlRegisterSingletonType<QTestRootObject>("Qt.test.qtestroot", 1, 0, "QTestRootObject", testRootObject);
    qmlRegisterSingletonType<AmTest>("QtApplicationManager", 2, 0, "AmTest", amTest);

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

    QuickTestResult::setProgramName(nullptr);

    return QuickTestResult::exitCode();
}

QT_END_NAMESPACE_AM
