/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include "testrunner.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>

#include <qlogging.h>
#include <QtTest/qtestsystem.h>
#include <private/quicktest_p.h>
#include <private/quicktestresult_p.h>
#include "amtest.h"


QT_BEGIN_NAMESPACE
namespace QTest {
    extern Q_TESTLIB_EXPORT bool printAvailableFunctions;
    extern Q_TESTLIB_EXPORT void qtest_qParseArgs(int argc, char *argv[], bool qml);
}
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_AM

static QObject *amTest(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(jsEngine);
    return AmTest::instance();
}

void TestRunner::initialize(const QString &testFile, const QStringList &testRunnerArguments)
{
    Q_ASSERT(!testRunnerArguments.isEmpty());

    const QString name = QFileInfo(testRunnerArguments.at(0)).fileName() + qSL("::") + QDir().relativeFilePath(testFile);
    static const char *programName = qstrdup(name.toLocal8Bit().constData());
    QuickTestResult::setProgramName(programName);

    // Convert all the arguments back into a char * array.
    // These need to be alive as long as the program is running!
    static QVector<char *> testArgV;
    for (const auto &arg : testRunnerArguments)
        testArgV << qstrdup(arg.toLocal8Bit().constData());

    atexit([]() {
        delete [] programName;
        std::for_each(testArgV.constBegin(), testArgV.constEnd(), [](char *arg) {
            delete [] arg;
        });
    });

    QuickTestResult::setCurrentAppname(testArgV.constFirst());

    // Allocate a QuickTestResult to create QBenchmarkGlobalData, otherwise the benchmark options don't work
    QuickTestResult result;
    // We would call QuickTestResult::parseArgs here, but that would include qml options in the help
    // which we don't support
    QTest::qtest_qParseArgs(testArgV.size(), testArgV.data(), false /*no qml options*/);
    qputenv("QT_QTESTLIB_RUNNING", "1");

    // Register the test object and application manager test add-on
    qmlRegisterSingletonType<AmTest>("QtApplicationManager.SystemUI", 2, 0, "AmTest", amTest);
}

int TestRunner::exec(QQmlEngine *qmlEngine)
{
    QEventLoop eventLoop;

    int typeId = qmlTypeId("QtTest", 1, 2, "QTestRootObject");
    QTestRootObject* inst = qmlEngine->singletonInstance<QTestRootObject*>(typeId);
    inst->init();

    inst->setWindowShown(true);

    if (QTest::printAvailableFunctions)
        return 0;

    if (inst->hasTestCase())
        eventLoop.exec();

    QuickTestResult::setProgramName(nullptr);

    return QuickTestResult::exitCode();
}

QT_END_NAMESPACE_AM
