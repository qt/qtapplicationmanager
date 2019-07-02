/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
#include <QDir>
#include <QString>
#include <QStringList>

#include "applicationinstaller.h"
#include "applicationmanager.h"
#include "logging.h"
#include "main.h"
#include <QtAppManMain/defaultconfiguration.h>


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

private:
    void cleanUpInstallationDir();
    void installPackage(const QString &path);
    void removePackage(const QString &id);
    void initMain();
    void destroyMain();
    void copyRecursively(const QString &sourceDir, const QString &destDir);
    int argc;
    char **argv;
    Main *main{nullptr};
    DefaultConfiguration *config{nullptr};
    bool m_verbose = false;
};

tst_Main::tst_Main()
{
    argc = 3;
    argv = new char*[3];
    argv[0] = new char[255];
    sprintf(argv[0], "tst_update-builtin-app");
    argv[1] = new char[255];
    sprintf(argv[1], "--dbus");
    argv[2] = new char[255];
    sprintf(argv[2], "none");
}

tst_Main::~tst_Main()
{
    for (int i = 0; i < argc; ++i)
        delete []argv[i];
    delete []argv;
}

void tst_Main::initTestCase()
{
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    m_verbose = qEnvironmentVariableIsSet("VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (m_verbose ? "on" : "off") << "(changed by (un)setting $VERBOSE_TEST)";
}

void tst_Main::copyRecursively(const QString &sourcePath, const QString &destPath)
{
    QDir sourceDir(sourcePath);
    QDir destDir(destPath);

    QStringList subdirNames = sourceDir.entryList(QDir::NoDotAndDotDot | QDir::AllDirs);
    for (auto subdirName : subdirNames) {
        destDir.mkdir(subdirName);
        copyRecursively(sourceDir.filePath(subdirName), destDir.filePath(subdirName));
    }

    QStringList fileNames = sourceDir.entryList(QDir::Files);
    for (auto fileName : fileNames)
        QFile::copy(sourceDir.filePath(fileName), destDir.filePath(fileName));
}

void tst_Main::cleanUpInstallationDir()
{
    {
        QDir testTmpDir("/tmp/am-test-main");
        if (testTmpDir.exists())
            testTmpDir.removeRecursively();
    }
    QDir tmpDir("/tmp");
    tmpDir.mkdir("am-test-main");
}

void tst_Main::init()
{
    cleanUpInstallationDir();
}

void tst_Main::initMain()
{
    main = new Main(argc, argv);

    QStringList deploymentWarnings;

    QString amConfigPath = QFINDTESTDATA("am-config.yaml");
    auto pathList = QStringList(amConfigPath);

    config = new DefaultConfiguration(pathList, QString());
    config->parse(&deploymentWarnings);

    main->setup(config, deploymentWarnings);
    if (m_verbose)
        Logging::setFilterRules(Logging::filterRules() += qSL("am.installer.debug=true"));

    ApplicationInstaller::instance()->setAllowInstallationOfUnsignedPackages(true);
}

void tst_Main::destroyMain()
{
    if (main) {
        main->shutDown();
        main->exec();
        delete main;
        main = nullptr;
    }
}

void tst_Main::cleanup()
{
    destroyMain();

    delete config;
    config = nullptr;
}

void tst_Main::installPackage(const QString &pkgPath)
{
    auto appInstaller = ApplicationInstaller::instance();

    bool installationFinished = false;

    connect(appInstaller, &ApplicationInstaller::taskRequestingInstallationAcknowledge,
            this, [this, appInstaller](const QString &taskId, const QVariantMap &,
                                       const QVariantMap &, const QVariantMap &) {
            appInstaller->acknowledgePackageInstallation(taskId);
    });

    connect(appInstaller, &ApplicationInstaller::taskFinished,
            this, [this, &installationFinished](const QString &) {
            installationFinished = true;
    });

    appInstaller->startPackageInstallation(qSL("internal-0"), QUrl::fromLocalFile(pkgPath));

    QTRY_COMPARE(installationFinished, true);
}

void tst_Main::removePackage(const QString &id)
{
    auto appInstaller = ApplicationInstaller::instance();

    bool removalFinished = false;

    connect(appInstaller, &ApplicationInstaller::taskFinished,
            this, [this, &removalFinished](const QString &) {
            removalFinished = true;
    });

    appInstaller->removePackage(id, false /* keepDocuments */);

    QTRY_COMPARE(removalFinished, true);
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
    QCOMPARE(appMan->count(), 1);

    auto app = appMan->application(0);
    QCOMPARE(app->name(qSL("en")), qSL("Hello Red"));

    installPackage(qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg"));

    QCOMPARE(appMan->count(), 1);
    // it must still be the same Application instance as before the installation
    QCOMPARE(appMan->application(0), app);
    // but with different contents
    QCOMPARE(app->name(qSL("en")), qSL("Hello Updated Red"));

    removePackage(qSL("hello-world.red"));

    // After removal of the updated version all data in Application should be as before the
    // installation took place.
    QCOMPARE(appMan->count(), 1);
    QCOMPARE(appMan->application(0), app);
    QCOMPARE(app->name(qSL("en")), qSL("Hello Red"));
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
    copyRecursively(QFINDTESTDATA("dir-with-update-already-installed"), "/tmp/am-test-main");

    initMain();

    auto appMan = ApplicationManager::instance();
    QCOMPARE(appMan->count(), 1);

    auto app = appMan->application(0);
    QCOMPARE(app->name(qSL("en")), qSL("Hello Updated Red"));
}

/*
   Install an update for a built-in app and quit Main. A database will be generated.

   Then, on next iteration of Main, Applications will be created from that database.
   Check that ApplicationManager shows only one, updated, built-in app.
 */
void tst_Main::loadDatabaseWithUpdatedBuiltInApp()
{
    QCOMPARE(QFile::exists("/tmp/am-test-main/apps.db"), false);

    initMain();
    installPackage(qL1S(AM_TESTDATA_DIR "packages/hello-world.red.appkg"));
    destroyMain();

    QCOMPARE(QFile::exists("/tmp/am-test-main/apps.db"), true);

    initMain();

    auto appMan = ApplicationManager::instance();
    QCOMPARE(appMan->count(), 1);

    auto app = appMan->application(0);
    QCOMPARE(app->name(qSL("en")), qSL("Hello Updated Red"));
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

    QStringList arguments;
    arguments << "tst_update-builtin-app";
    arguments << "--dbus";
    arguments << "none";
    arguments << mainQml;

    main = new Main(argc, argv);

    config = new DefaultConfiguration(QStringList(QFINDTESTDATA("am-config.yaml")), QString());
    config->parseWithArguments(arguments);

    QString errorMsg;

    try {
        main->setup(config);
    } catch (const std::exception &e) {
        errorMsg.append(e.what());
    }

    QCOMPARE(errorMsg, expectedErrorMsg);

    delete config;
    config = nullptr;
    delete main;
    main = nullptr;
}

QTEST_APPLESS_MAIN(tst_Main)

#include "tst_main.moc"
