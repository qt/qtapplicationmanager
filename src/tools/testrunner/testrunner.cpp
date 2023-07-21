// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "testrunner.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QQmlEngine>
#include <QFileInfo>
#include <QVector>
#include <QScopeGuard>

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


void TestRunner::initialize(const QString &testFile, const QStringList &testRunnerArguments,
                            const QString &sourceFile)
{
    Q_ASSERT(!testRunnerArguments.isEmpty());

    // Convert all the arguments back into a char * array.
    // qtest_qParseArgs copies all data, so we can get rid of the array afterwards
    QVector<char *> argv;
    auto cleanup = qScopeGuard([&argv]() {
        std::for_each(argv.cbegin(), argv.cend(), [](char *arg) { delete [] arg; });
    });
    int argc = int(testRunnerArguments.size());
    argv.resize(argc + 1);
    for (int i = 0; i < argc; ++i)
        argv[i] = qstrdup(testRunnerArguments.at(i).toLocal8Bit());
    argv[argc] = nullptr;

    if (!sourceFile.isEmpty()) {
        if (QFileInfo(sourceFile).fileName() != QFileInfo(testFile).fileName())
            qWarning() << "appman-qmltestrunner: source file" << sourceFile << "does not match test file" << testFile;
        else
            QTest::setMainSourcePath(sourceFile.toLocal8Bit());
    }
    QuickTestResult::setCurrentAppname(argv.at(0));
    QuickTestResult::setProgramName("appman-qmltestrunner");

    // Allocate a QuickTestResult to create QBenchmarkGlobalData, otherwise the benchmark options don't work
    volatile QuickTestResult result;

    // We would call QuickTestResult::parseArgs here, but that would include qml options in the help
    // which we don't support
    QTest::qtest_qParseArgs(argc, argv.data(), false /*no qml options*/);

    qputenv("QT_QTESTLIB_RUNNING", "1");

    // Register the test object and application manager test add-on
    qmlRegisterSingletonType<AmTest>("QtApplicationManager.SystemUI", 2, 0, "AmTest",
                                     [](QQmlEngine *, QJSEngine *) {
        return AmTest::instance();
    });
}

int TestRunner::exec(QQmlEngine *qmlEngine)
{
    QEventLoop eventLoop;

    int typeId = qmlTypeId("QtTest", 1, 2, "QTestRootObject");
    QTestRootObject* inst = qmlEngine->singletonInstance<QTestRootObject*>(typeId);

    inst->setWindowShown(true);

    if (QTest::printAvailableFunctions)
        return 0;

    if (inst->hasTestCase())
        eventLoop.exec();

    QuickTestResult::setProgramName(nullptr);

    return QuickTestResult::exitCode();
}

QT_END_NAMESPACE_AM
