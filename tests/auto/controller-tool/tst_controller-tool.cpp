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


AM_QML_REGISTER_TYPES(QtApplicationManager_SystemUI)
AM_QML_REGISTER_TYPES(QtApplicationManager)

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
    QDir m_tmpDir;
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
        QStringList args = { qSL("--instance-id"), qSL("controller-test-id") };
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
    , m_tmpDir(qSL("/tmp/am-test-controller-tool"))
{ }

void tst_ControllerTool::initTestCase()
{
#if !defined(Q_OS_LINUX)
    QSKIP("This test is only supported on Linux");
#endif

    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    auto verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";

    m_argc = 2;
    m_argv = new char * [m_argc + 1];
    m_argv[0] = qstrdup("tst_controller-tool");
    m_argv[1] = qstrdup("--no-cache");
    m_argv[m_argc] = nullptr;

    m_main = new Main(m_argc, m_argv);  // QCoreApplication saves a reference to argc!

    QStringList possibleLocations;
    possibleLocations.append(QLibraryInfo::path(QLibraryInfo::BinariesPath));
    possibleLocations.append(QCoreApplication::applicationDirPath() + qSL("/../../../bin"));

    QString controllerPath;
    const QString controllerName = qSL("/appman-controller");
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

    if (m_tmpDir.exists())
        m_tmpDir.removeRecursively();
    QVERIFY(m_tmpDir.mkpath(qSL(".")));
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

    if (m_tmpDir.exists())
        m_tmpDir.removeRecursively();
}

void tst_ControllerTool::usage()
{
    ControllerTool ctrl({ });
    QVERIFY(!ctrl.call());
    QVERIFY(ctrl.stdOut.trimmed().startsWith("Usage:"));
}

void tst_ControllerTool::instances()
{
    ControllerTool ctrl({ qSL("list-instances") });
    QVERIFY2(ctrl.call(), ctrl.failure);
    QCOMPARE(ctrl.stdOutList, QStringList({ qSL("\"controller-test-id\"") }));
}

void tst_ControllerTool::applications()
{
    {
        ControllerTool ctrl({ qSL("list-applications") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ qSL("green1"), qSL("green2") }));
    }
    {
        ControllerTool ctrl({ qSL("show-application"), qSL("green1") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QVariantMap expected({
            { qSL("applicationId"), qSL("green1") },
            { qSL("capabilities"), QVariantList { } },
            { qSL("categories"), QVariantList { } },
            { qSL("codeFilePath"), QFINDTESTDATA("builtin-apps/hello-world.green/main.qml") },
            { qSL("icon"), QUrl::fromLocalFile(QFINDTESTDATA("builtin-apps/hello-world.green/icon.png")).toString() },
            { qSL("isBlocked"), false },
            { qSL("isRemovable"), false },
            { qSL("isRunning"), false },
            { qSL("isShuttingDown"), false },
            { qSL("isStartingUp"), false },
            { qSL("isUpdating"), false },
            { qSL("lastExitCode"), 0 },
            { qSL("lastExitStatus"), 0 },
            { qSL("name"), qSL("Hello Green") },
            { qSL("runtimeName"), qSL("qml") },
            { qSL("runtimeParameters"), QVariantMap { } },
            { qSL("updateProgress"), 0 },
            { qSL("version"), qSL("1.2.3") },
        });
        QCOMPARE(vm, expected);
    }
}

void tst_ControllerTool::packages()
{
    {
        ControllerTool ctrl({ qSL("list-packages") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ qSL("hello-world.green") }));
    }
    {
        ControllerTool ctrl({ qSL("show-package"), qSL("hello-world.green") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QVariantMap expected({
            { qSL("description"), qSL("Green description") },
            { qSL("icon"), QUrl::fromLocalFile(QFINDTESTDATA("builtin-apps/hello-world.green/icon.png")).toString() },
            { qSL("isBlocked"), false },
            { qSL("isRemovable"), false },
            { qSL("isUpdating"), false },
            { qSL("name"), qSL("Hello Green") },
            { qSL("packageId"), qSL("hello-world.green") },
            { qSL("updateProgress"), 0 },
            { qSL("version"), qSL("1.2.3") },
        });
        QCOMPARE(vm, expected);
    }
}

void tst_ControllerTool::installationLocations()
{
    {
        ControllerTool ctrl({ qSL("list-installation-locations") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList, QStringList({ qSL("internal-0") }));
    }
    {
        ControllerTool ctrl({ qSL("show-installation-location"), qSL("internal-0") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        const auto docs = YamlParser::parseAllDocuments(ctrl.stdOut);
        QCOMPARE(docs.size(), 1);
        const auto vm = docs[0].toMap();

        QCOMPARE(vm.value(qSL("path")), qSL("/tmp/am-test-controller-tool/apps"));
        QVERIFY(vm.value(qSL("deviceSize")).toULongLong() > 0);
        QVERIFY(vm.value(qSL("deviceFree")).toULongLong() > 0);
    }
}

void tst_ControllerTool::installCancel()
{
    ControllerTool install({ qSL("install-package"),
                            qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg") });
    QVERIFY2(install.start(), install.failure);

    QTRY_VERIFY(PackageManager::instance()->isPackageInstallationActive(qSL("hello-world.red")));
    QString taskId;

    {
        ControllerTool ctrl({ qSL("list-installation-tasks") });
        QVERIFY2(ctrl.call(), ctrl.failure);
        QCOMPARE(ctrl.stdOutList.size(), 1);
        taskId = ctrl.stdOutList.at(0);
        QVERIFY(!QUuid::fromString(taskId).isNull());
    }
    {
        ControllerTool ctrl({ qSL("cancel-installation-task"), taskId });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    {
        ControllerTool ctrl({ qSL("list-installation-tasks") });
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
        ControllerTool ctrl({ qSL("install-package"), qSL("-a"),
                             qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg") });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    {
        ControllerTool ctrl({ qSL("remove-package"), qSL("hello-world.red") });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
}

void tst_ControllerTool::startStop()
{
    const auto app = ApplicationManager::instance()->application(qSL("green1"));
    QVERIFY(app);
    {
        ControllerTool ctrl({ qSL("start-application"), app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::Running);
    {
        ControllerTool ctrl({ qSL("stop-application"), app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::NotRunning);
    {
        ControllerTool ctrl({ qSL("debug-application"), qSL("FOO=BAR"), app->id() });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::Running);
    {
        ControllerTool ctrl({ qSL("stop-all-applications") });
        QVERIFY2(ctrl.call(), ctrl.failure);
    }
    QTRY_VERIFY(app->runState() == Am::NotRunning);
}

void tst_ControllerTool::injectIntent()
{
    auto oldDevMode = PackageManager::instance()->developmentMode();
    PackageManager::instance()->setDevelopmentMode(true);

    ControllerTool ctrl({ qSL("inject-intent-request"), qSL("--requesting-application-id"),
        qSL(":sysui:"), qSL("--application-id"), qSL(":sysui:"), qSL("inject-intent"), qSL("{ }"), });
    QVERIFY2(ctrl.call(), ctrl.failure);

    const auto json = QJsonDocument::fromJson(ctrl.stdOut);
    const auto vm = json.toVariant().toMap();

    QVariantMap expected({
        { qSL("status"), qSL("ok") },
    });
    QCOMPARE(vm, expected);

    PackageManager::instance()->setDevelopmentMode(oldDevMode);
}


QTEST_APPLESS_MAIN(tst_ControllerTool)

#include "tst_controller-tool.moc"
