// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>
#include <QDir>
#include <QString>

#include "packagemanager.h"
#include "package.h"
#include "applicationmanager.h"
#include "logging.h"
#include "main.h"
#include "intentserver.h"
#include "intent.h"
#include "startuptimer.h"
#include "utilities.h"
#include <QtAppManMain/configuration.h>


QT_USE_NAMESPACE_AM

class tst_Main : public QObject
{
    Q_OBJECT

public:
    tst_Main();
    ~tst_Main();

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void installAndRemoveUpdateForBuiltIn();
    void updateForBuiltInAlreadyInstalled();
    void loadDatabaseWithUpdatedBuiltInApp();
    void mainQmlFile_data();
    void mainQmlFile();
    void startupTimer();

private:
    void cleanUpInstallationDir();
    void installPackage(const QString &path);
    void removePackage(const QString &id);
    void initMain(const QString &mainQml = { });
    void destroyMain();
    void copyRecursively(const QString &sourceDir, const QString &destDir);
    int argc = 0;
    char **argv = nullptr;
    Main *main = nullptr;
    bool mainSetupDone = false;
    Configuration *config = nullptr;
    bool m_verbose = false;
    int m_spyTimeout;
};

tst_Main::tst_Main()
    : m_spyTimeout(5000 * timeoutFactor())
{ }

tst_Main::~tst_Main()
{ }

void tst_Main::initTestCase()
{
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    m_verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (m_verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";
}

void tst_Main::copyRecursively(const QString &sourcePath, const QString &destPath)
{
    QDir sourceDir(sourcePath);
    QDir destDir(destPath);

    QStringList subdirNames = sourceDir.entryList(QDir::NoDotAndDotDot | QDir::AllDirs);
    for (const auto &subdirName : subdirNames) {
        destDir.mkdir(subdirName);
        copyRecursively(sourceDir.filePath(subdirName), destDir.filePath(subdirName));
    }

    QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Hidden);
    for (const auto &fileName : fileNames)
        QFile::copy(sourceDir.filePath(fileName), destDir.filePath(fileName));
}

void tst_Main::cleanUpInstallationDir()
{
    {
        QDir testTmpDir(qSL("/tmp/am-test-main"));
        if (testTmpDir.exists())
            testTmpDir.removeRecursively();
    }
    QDir tmpDir(qSL("/tmp"));
    tmpDir.mkdir(qSL("am-test-main"));
}

void tst_Main::init()
{
    cleanUpInstallationDir();
}

void tst_Main::initMain(const QString &mainQml)
{
    argc = mainQml.isNull() ? 4 : 5;
    argv = new char*[argc + 1];
    argv[0] = qstrdup("tst_Main");
    argv[1] = qstrdup("--dbus");
    argv[2] = qstrdup("none");
    argv[3] = qstrdup("--no-cache");
    if (!mainQml.isNull())
        argv[4] = qstrdup(mainQml.toLocal8Bit());
    argv[argc] = nullptr;

    main = new Main(argc, argv);
    config = new Configuration({ QFINDTESTDATA("am-config.yaml") }, QString());
    config->parseWithArguments(QCoreApplication::arguments());
    if (m_verbose)
        config->setForceVerbose(true);

    main->setup(config);
    mainSetupDone = true;

    PackageManager::instance()->setAllowInstallationOfUnsignedPackages(true);
}

void tst_Main::destroyMain()
{
    if (main) {
        if (mainSetupDone) {
            main->shutDown();
            main->exec();
        }
        delete main;
        main = nullptr;
        mainSetupDone = false;
    }
    if (config) {
        delete config;
        config = nullptr;
    }
    if (argc && argv) {
        for (int i = 0; i < argc; ++i)
            delete [] argv[i];
        delete [] argv;
        argc = 0;
        argv = nullptr;
    }
}

void tst_Main::cleanup()
{
    destroyMain();
}

void tst_Main::installPackage(const QString &pkgPath)
{
    auto packageManager = PackageManager::instance();

    connect(packageManager, &PackageManager::taskRequestingInstallationAcknowledge,
            this, [packageManager](const QString &taskId, Package *,
                                   const QVariantMap &, const QVariantMap &) {
            packageManager->acknowledgePackageInstallation(taskId);
    });

    QSignalSpy finishedSpy(packageManager, &PackageManager::taskFinished);
    packageManager->startPackageInstallation(QUrl::fromLocalFile(pkgPath));
    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1, m_spyTimeout);
}

void tst_Main::removePackage(const QString &id)
{
    auto packageManager = PackageManager::instance();

    QSignalSpy finishedSpy(packageManager, &PackageManager::taskFinished);
    packageManager->removePackage(id, false /* keepDocuments */);
    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1, m_spyTimeout);
}

/*
  Install an application with the same id of an existing, builtin, one.
  Then remove it.
  At all stages ApplicationManager should contain the same Application
  object. All that changes is the contents of that Application:
  from original, to updated, and then back to original.
 */
void tst_Main::installAndRemoveUpdateForBuiltIn()
{
    initMain();

    auto appMan = ApplicationManager::instance();
    QCOMPARE(appMan->count(), 2);
    auto intents = IntentServer::instance();
    QCOMPARE(intents->count(), 2);

    auto app1 = appMan->application(0);
    QCOMPARE(app1->names().value(qSL("en")), qSL("Hello Red"));
    QCOMPARE(app1->id(), qSL("red1"));
    auto app2 = appMan->application(1);
    QCOMPARE(app2->names().value(qSL("en")), qSL("Hello Red"));
    QCOMPARE(app2->id(), qSL("red2"));

    auto intent1 = intents->applicationIntent(qSL("red.intent1"), qSL("red1"));
    QVERIFY(intent1);
    QCOMPARE(intent1->intentId(), qSL("red.intent1"));
    QCOMPARE(intent1->applicationId(), qSL("red1"));
    QCOMPARE(intent1->packageId(), qSL("hello-world.red"));
    QVERIFY(intent1->categories().contains(qSL("one")));
    QVERIFY(intent1->categories().contains(qSL("launcher")));

    auto intent2 = intents->applicationIntent(qSL("red.intent2"), qSL("red2"));
    QVERIFY(intent2);
    QCOMPARE(intent2->intentId(), qSL("red.intent2"));
    QCOMPARE(intent2->applicationId(), qSL("red2"));
    QCOMPARE(intent2->packageId(), qSL("hello-world.red"));
    QVERIFY(intent2->categories().contains(qSL("two")));
    QVERIFY(intent2->categories().contains(qSL("launcher")));

    installPackage(qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg"));

    QCOMPARE(appMan->count(), 1);
    QCOMPARE(intents->count(), 0);

    // it must still be a different Application instance as before the installation, but quite
    // often we get the same pointer back because it's a delete/new back-to-back
    app1 = appMan->application(0);

    // but with different contents
    QCOMPARE(app1->names().value(qSL("en")), qSL("Hello Updated Red"));
    intent1 = intents->applicationIntent(qSL("red.intent1"), qSL("red1"));
    QVERIFY(!intent1);

    removePackage(qSL("hello-world.red"));

    // After removal of the updated version all data in Application should be as before the
    // installation took place.
    QCOMPARE(appMan->count(), 2);
    QCOMPARE(intents->count(), 2);

    app1 = appMan->application(0);
    QCOMPARE(app1->names().value(qSL("en")), qSL("Hello Red"));
    intent1 = intents->applicationIntent(qSL("red.intent1"), qSL("red1"));
    QVERIFY(intent1);
    QCOMPARE(intent1->intentId(), qSL("red.intent1"));
    intent2 = intents->applicationIntent(qSL("red.intent2"), qSL("red2"));
    QVERIFY(intent2);
    QCOMPARE(intent2->intentId(), qSL("red.intent2"));
}

/*
   Situation: there's already an update installed for a built-in application and there's no database
   file yet (or a database rebuild has been requested, in which case any existing database file is
   ignored).

   Check that ApplicationManager ends up with only one ApplicationObject as both entries
   (the built-in and the installed one) share the same applicaion id. And check also that
   this ApplicationObject is showing data from the installed update.
 */
void tst_Main::updateForBuiltInAlreadyInstalled()
{
    copyRecursively(QFINDTESTDATA("dir-with-update-already-installed"), qSL("/tmp/am-test-main"));

    initMain();

    auto appMan = ApplicationManager::instance();
    QCOMPARE(appMan->count(), 1);

    auto app = appMan->application(0);
    QCOMPARE(app->names().value(qSL("en")), qSL("Hello Updated Red"));
}

/*
   Install an update for a built-in app and quit Main. A database will be generated.

   Then, on next iteration of Main, Applications will be created from that database.
   Check that ApplicationManager shows only one, updated, built-in app.
 */
void tst_Main::loadDatabaseWithUpdatedBuiltInApp()
{
    initMain();
    installPackage(qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg"));
    destroyMain();

    initMain();

    auto appMan = ApplicationManager::instance();
    QCOMPARE(appMan->count(), 1);

    auto app = appMan->application(0);
    QCOMPARE(app->names().value(qSL("en")), qSL("Hello Updated Red"));
}

void tst_Main::mainQmlFile_data()
{
    QTest::addColumn<QString>("mainQml");
    QTest::addColumn<QString>("expectedErrorMsg");

    QTest::newRow("none") << "" << "No main QML file specified";

    QTest::newRow("invalid") << "foo/bar.qml" << "Invalid main QML file specified: foo/bar.qml";
    QTest::newRow("invalid-dir") << "." << "Invalid main QML file specified: .";
    QTest::newRow("invalid-qrc1") << ":/bar.qml" << "Invalid main QML file specified: :/bar.qml";
    // "://bar.qml" yields an absolute file path of ":" and QFile::exists() would always return true
    QTest::newRow("invalid-qrc2") << "://bar.qml" << "Invalid main QML file specified: ://bar.qml";
    QTest::newRow("invalid-qrc-dir") << ":/foo" << "Invalid main QML file specified: :/foo";

    QTest::newRow("valid-native") << QFINDTESTDATA("dummy.qml") << "";
    QTest::newRow("valid-qrc-file") << ":/foo/dummy.qml" << "";
    QTest::newRow("valid-qrc-url") << "qrc:///foo/dummy.qml" << "";

    // Passes unchecked:
    QTest::newRow("https") << "https://www.qt.io/foo/bar.qml" << "";
    QTest::newRow("assets") << "assets:///foo/bar.qml" << "";
}

void tst_Main::mainQmlFile()
{
    QFETCH(QString, mainQml);
    QFETCH(QString, expectedErrorMsg);

    try {
        initMain(mainQml);
        QVERIFY2(expectedErrorMsg.isEmpty(), "Exception was expected, but none was thrown");
    } catch (const std::exception &e) {
        QCOMPARE(QString::fromLocal8Bit(e.what()), expectedErrorMsg);
    }
}

void tst_Main::startupTimer()
{
#if defined(Q_OS_QNX)
    QSKIP("StartupTimer not yet supported on QNX");
#endif
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    QVERIFY(tmp.open());
    tmp.close();

    QString fn = tmp.fileName();

    auto cleanup = qScopeGuard([fn]{ QFile::remove(fn); });

    delete StartupTimer::instance();
    qputenv("AM_STARTUP_TIMER", fn.toLocal8Bit());
    StartupTimer::instance();

    initMain();

    StartupTimer::instance()->createReport(qSL("TEST"));

    QFile f(fn);
    QVERIFY(f.open(QIODevice::ReadOnly));
    auto report = f.readAll().replace('\r', QByteArray { });

    QVERIFY(report.startsWith("\n== STARTUP TIMING REPORT: TEST =="));
    QVERIFY(report.contains("after QML engine instantiation"));
}

QTEST_APPLESS_MAIN(tst_Main)

#include "tst_main.moc"
