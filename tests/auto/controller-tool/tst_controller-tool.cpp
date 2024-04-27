// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <initializer_list>
#include <memory>

#include <QtCore>
#include <QtTest>
#include <QDir>
#include <QUuid>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>

#include "applicationmanager.h"
#include "packagemanager.h"
#include "logging.h"
#include "main.h"
#include "exception.h"
#include "utilities.h"
#include "qml-utilities.h"
#include "qtyaml.h"
#include <QtAppManMain/configuration.h>

using namespace Qt::StringLiterals;


QT_USE_NAMESPACE_AM
using namespace QtYaml;

class tst_ControllerTool : public QObject
{
    Q_OBJECT

public:
    tst_ControllerTool();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void usage();
    void instances();
    void applications();
    void packages();
    void installationLocations();
    void installCancel();
    void installRemove();
    void startStop();
    void injectIntent();

private:
    int m_spyTimeout;
    int m_argc = 0;
    char **m_argv = nullptr;
    Main *m_main = nullptr;
    Configuration *m_config = nullptr;
    bool m_mainSetupDone = false;
};


class ControllerTool
{
public:
    ControllerTool(const std::initializer_list<QString> &list)
        : m_arguments(list)
    { }
    ControllerTool(const QStringList &arguments)
        : m_arguments(arguments)
    { }

    static void setControllerPath(const QString &path)
    {
        s_command = path;
    }

    int exitCode = 0;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QByteArray stdOut;
    QStringList stdOutList;
    QByteArray stdErr;
    QStringList stdErrList;
    QByteArray failure;

    bool call()
    {
        return start() && waitForFinished();
    }

    bool start()
    {
        if (m_started)
            return false;

        m_ctrl.reset(new QProcess);
        m_spy.reset(new QSignalSpy(m_ctrl.get(), &QProcess::finished));
        m_ctrl->setProgram(s_command);
        QStringList args = { u"--instance-id"_s, u"controller-test-id"_s };
        args.append(m_arguments);
        m_ctrl->setArguments(args);
        m_ctrl->start();

        if (!m_ctrl->waitForStarted()) {
            failure = "could not start appman-controller";
            return false;
        }
        return m_started = true;
    }

    bool waitForFinished()
    {
        if (!m_started)
            return false;

        if (m_ctrl->state() == QProcess::Running) {
            m_spy->wait(5000 * timeoutFactor());
            if (m_ctrl->state() != QProcess::NotRunning) {
                failure = "appman-controller did not exit";
                return false;
            }
        }

        exitCode = m_ctrl->exitCode();
        exitStatus = m_ctrl->exitStatus();
        stdOut = m_ctrl->readAllStandardOutput();
        stdOutList = QString::fromLocal8Bit(stdOut).split(u'\n', Qt::SkipEmptyParts);
        stdErr = m_ctrl->readAllStandardError();
        stdErrList = QString::fromLocal8Bit(stdErr).split(u'\n', Qt::SkipEmptyParts);

        if (exitStatus == QProcess::CrashExit)
            failure = "appman-controller crashed, signal: " + QByteArray::number(exitCode);
        else if (exitCode != 0)
            failure = "appman-controller returned an error code: " + QByteArray::number(exitCode);

        // enable for debugging
//        if (!failure.isEmpty()) {
//            qWarning() << "STDOUT" << stdOut;
//            qWarning() << "STDERR" << stdErr;
//        }
        m_started = false;
        return failure.isEmpty();
    }
private:
    QStringList m_arguments;
    bool m_started = false;
    std::unique_ptr<QSignalSpy> m_spy;
    std::unique_ptr<QProcess> m_ctrl;
    static QString s_command;
};

QString ControllerTool::s_command;


tst_ControllerTool::tst_ControllerTool()
    : m_spyTimeout(5000 * timeoutFactor())
{ }

void tst_ControllerTool::initTestCase()
{
#if !defined(Q_OS_LINUX)
    QSKIP("This test is only supported on Linux");
#endif

    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    auto verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";

    m_argc = 2;
    m_argv = new char * [size_t(m_argc) + 1];
    m_argv[0] = qstrdup("tst_controller-tool");
    m_argv[1] = qstrdup("--no-cache");
    m_argv[m_argc] = nullptr;

    m_main = new Main(m_argc, m_argv);  // QCoreApplication saves a reference to argc!

    QStringList possibleLocations;
    possibleLocations.append(QCoreApplication::applicationDirPath() + u"/../../../bin"_s);
    possibleLocations.append(QLibraryInfo::path(QLibraryInfo::BinariesPath));

    QString controllerPath;
    const QString controllerName = u"/appman-controller"_s;
    for (const QString &possibleLocation : possibleLocations) {
        QFileInfo fi(possibleLocation + controllerName);

        if (fi.exists() && fi.isExecutable()) {
            controllerPath = fi.absoluteFilePath();
            break;
        }
    }
    QVERIFY(!controllerPath.isEmpty());
    ControllerTool::setControllerPath(controllerPath);

    m_config = new Configuration({ QFINDTESTDATA("am-config.yaml") }, QString());
    m_config->parseWithArguments(QCoreApplication::arguments());
    if (verbose)
        m_config->setForceVerbose(true);

    try {
        m_main->setup(m_config);
        m_mainSetupDone = true;
        m_main->loadQml(false);
        m_main->showWindow(false);
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
    PackageManager::instance()->setAllowInstallationOfUnsignedPackages(true);
}

void tst_ControllerTool::cleanupTestCase()
{
    if (m_main) {
        if (m_mainSetupDone) {
            m_main->shutDown();
            m_main->exec();
        }
        delete m_main;
    }
    if (m_config)
        delete m_config;

    if (m_argc && m_argv) {
        for (int i = 0; i < m_argc; ++i)
            delete [] m_argv[i];
        delete [] m_argv;
    }
}

void tst_ControllerTool::usage()
{
    ControllerTool ctrl({ });
    QVERIFY(!ctrl.call());
    QVERIFY(ctrl.stdOut.trimmed().startsWith("Usage:"));
}

void tst_ControllerTool::instances()
{
    ControllerTool ctrl({ u"list-instances"_s });
    QVERIFY2(ctrl.call(), ctrl.failure);
    QVERIFY(ctrl.stdErrList.isEmpty());
    QCOMPARE(ctrl.stdOutList, QStringList({ u"controller-test-id-0"_s }));
}

void tst_ControllerTool::applications()
{
    {
        ControllerTool ctrl({ u"list-applications"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ u"green1"_s, u"green2"_s }));
    }
    {
        ControllerTool ctrl({ u"show-application"_s, u"green1"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QVariantMap expected({
            { u"applicationId"_s, u"green1"_s },
            { u"capabilities"_s, QVariantList { } },
            { u"categories"_s, QVariantList { } },
            { u"codeFilePath"_s, QFINDTESTDATA("builtin-apps/hello-world.green/main.qml") },
            { u"icon"_s, QUrl::fromLocalFile(QFINDTESTDATA("builtin-apps/hello-world.green/icon.png")).toString() },
            { u"isBlocked"_s, false },
            { u"isRemovable"_s, false },
            { u"isRunning"_s, false },
            { u"isShuttingDown"_s, false },
            { u"isStartingUp"_s, false },
            { u"isUpdating"_s, false },
            { u"lastExitCode"_s, 0 },
            { u"lastExitStatus"_s, 0 },
            { u"name"_s, u"Hello Green"_s },
            { u"runtimeName"_s, u"qml"_s },
            { u"runtimeParameters"_s, QVariantMap { } },
            { u"updateProgress"_s, 0 },
            { u"version"_s, u"1.2.3"_s },
        });
        QCOMPARE(vm, expected);
    }
}

void tst_ControllerTool::packages()
{
    {
        ControllerTool ctrl({ u"list-packages"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ u"hello-world.green"_s }));
    }
    {
        ControllerTool ctrl({ u"show-package"_s, u"hello-world.green"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QVariantMap expected({
            { u"description"_s, u"Green description"_s },
            { u"icon"_s, QUrl::fromLocalFile(QFINDTESTDATA("builtin-apps/hello-world.green/icon.png")).toString() },
            { u"isBlocked"_s, false },
            { u"isRemovable"_s, false },
            { u"isUpdating"_s, false },
            { u"name"_s, u"Hello Green"_s },
            { u"packageId"_s, u"hello-world.green"_s },
            { u"updateProgress"_s, 0 },
            { u"version"_s, u"1.2.3"_s },
        });
        QCOMPARE(vm, expected);
    }
}

void tst_ControllerTool::installationLocations()
{
    {
        ControllerTool ctrl({ u"list-installation-locations"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ u"internal-0"_s }));
    }
    {
        ControllerTool ctrl({ u"show-installation-location"_s, u"internal-0"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QCOMPARE(vm.value(u"path"_s), u"/tmp/am-test-controller-tool/apps"_s);
        QVERIFY(vm.value(u"deviceSize"_s).toULongLong() > 0);
        QVERIFY(vm.value(u"deviceFree"_s).toULongLong() > 0);
    }
}

void tst_ControllerTool::installCancel()
{
    ControllerTool install({ u"install-package"_s,
                            QString::fromLatin1(AM_TESTDATA_DIR "packages/hello-world.red.appkg") });
    QVERIFY2(install.start(), install.failure);

    QTRY_VERIFY(PackageManager::instance()->isPackageInstallationActive(u"hello-world.red"_s));
    QString taskId;

    {
        ControllerTool ctrl({ u"list-installation-tasks"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList.size(), 1);
        taskId = ctrl.stdOutList.at(0);
        QVERIFY(!QUuid::fromString(taskId).isNull());
    }
    {
        ControllerTool ctrl({ u"cancel-installation-task"_s, taskId });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    {
        ControllerTool ctrl({ u"list-installation-tasks"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList.size(), 0);
    }

    QVERIFY(!install.waitForFinished());
    QVERIFY(install.exitCode != 0);
    QVERIFY(install.stdErr.contains("canceled"));
}

void tst_ControllerTool::installRemove()
{
    {
        ControllerTool ctrl({ u"install-package"_s, u"-a"_s,
                             QString::fromLatin1(AM_TESTDATA_DIR "packages/hello-world.red.appkg") });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    {
        ControllerTool ctrl({ u"remove-package"_s, u"hello-world.red"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
}

void tst_ControllerTool::startStop()
{
    const auto app = ApplicationManager::instance()->application(u"green1"_s);
    QVERIFY(app);
    {
        ControllerTool ctrl({ u"start-application"_s, app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::Running);
    {
        ControllerTool ctrl({ u"stop-application"_s, app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::NotRunning);
    {
        // debug-application does not work in single-process mode
        bool sp = ApplicationManager::instance()->isSingleProcess();
        ControllerTool ctrl(sp ? QStringList { u"start-application"_s, app->id() }
                               : QStringList { u"debug-application"_s, u"FOO=BAR"_s, app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::Running);
    {
        ControllerTool ctrl({ u"stop-all-applications"_s });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::NotRunning);
}

void tst_ControllerTool::injectIntent()
{
    auto oldDevMode = PackageManager::instance()->developmentMode();
    PackageManager::instance()->setDevelopmentMode(true);

    ControllerTool ctrl({ u"inject-intent-request"_s, u"--requesting-application-id"_s,
        u":sysui:"_s, u"--application-id"_s, u":sysui:"_s, u"inject-intent"_s, u"{ }"_s, });
    QVERIFY2(ctrl.call(), ctrl.failure);

    const auto json = QJsonDocument::fromJson(ctrl.stdOut);
    const auto vm = json.toVariant().toMap();

    QVariantMap expected({
        { u"status"_s, u"ok"_s },
    });
    QCOMPARE(vm, expected);

    PackageManager::instance()->setDevelopmentMode(oldDevMode);
}


QTEST_APPLESS_MAIN(tst_ControllerTool)

#include "tst_controller-tool.moc"
