// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore>
#include <QtTest>

#include <functional>

#include "packagemanager.h"
#include "packagedatabase.h"
#include "package.h"
#include "applicationinfo.h"
#include "sudo.h"
#include "logging.h"
#include "utilities.h"
#include "error.h"
#include "private/packageutilities_p.h"
#include "runtimefactory.h"
#include "qmlinprocruntime.h"
#include "packageutilities.h"

#include "../error-checking.h"
#include "../devmode.h"


using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM


static int spyTimeout = 5000; // shorthand for specifying QSignalSpy timeouts

class tst_PackageManager : public QObject
{
    Q_OBJECT

public:
    tst_PackageManager(QObject *parent = nullptr);
    ~tst_PackageManager() override;

    static bool startedSudoServer;
    static QString sudoServerError;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    //TODO: test AI::cleanupBrokenInstallations() before calling cleanup() the first time!

    void packageInstallation_data();
    void packageInstallation();

    void simulateErrorConditions_data();
    void simulateErrorConditions();

    void cancelPackageInstallation_data();
    void cancelPackageInstallation();

    void parallelPackageInstallation();
    void doublePackageInstallation();

    void validateDnsName_data();
    void validateDnsName();

    void compareVersions_data();
    void compareVersions();

public:
    enum PathLocation {
        Internal0,
        Documents0,

        PathLocationCount
    };

private:
    QString pathTo(const QString &sub = QString())
    {
        return pathTo(PathLocationCount, sub);
    }

    QString pathTo(PathLocation pathLocation, const QString &sub = QString())
    {
        QString base;
        switch (pathLocation) {
        case Internal0:      base = u"internal"_s; break;
        case Documents0:     base = u"documents"_s; break;
        default: break;
        }

        QDir workDir(m_workDir.path());

        if (base.isEmpty() && sub.isEmpty())
            base = workDir.absolutePath();
        else if (sub.isEmpty())
            base = workDir.absoluteFilePath(base);
        else if (base.isEmpty())
            base = workDir.absoluteFilePath(sub);
        else
            base = workDir.absoluteFilePath(base + u'/' + sub);

        if (QDir(base).exists())
            return base + u'/';
        else
            return base;
    }

    void clearSignalSpies()
    {
        m_startedSpy->clear();
        m_requestingInstallationAcknowledgeSpy->clear();
        m_blockingUntilInstallationAcknowledgeSpy->clear();
        m_progressSpy->clear();
        m_finishedSpy->clear();
        m_failedSpy->clear();
    }

    static bool isDataTag(const char *tag)
    {
        return !qstrcmp(tag, QTest::currentDataTag());
    }

private:
    SudoClient *m_sudo = nullptr;
    bool m_fakeSudo = false;

    QTemporaryDir m_workDir;
    QString m_hardwareId;
    PackageManager *m_pm = nullptr;
    QSignalSpy *m_startedSpy = nullptr;
    QSignalSpy *m_requestingInstallationAcknowledgeSpy = nullptr;
    QSignalSpy *m_blockingUntilInstallationAcknowledgeSpy = nullptr;
    QSignalSpy *m_progressSpy = nullptr;
    QSignalSpy *m_finishedSpy = nullptr;
    QSignalSpy *m_failedSpy = nullptr;
};

bool tst_PackageManager::startedSudoServer = false;
QString tst_PackageManager::sudoServerError;

tst_PackageManager::tst_PackageManager(QObject *parent)
    : QObject(parent)
{ }

tst_PackageManager::~tst_PackageManager()
{
    if (m_workDir.isValid()) {
        if (m_sudo)
            m_sudo->removeRecursive(m_workDir.path());
        else
            recursiveOperation(m_workDir.path(), safeRemove);
    }

    delete m_failedSpy;
    delete m_finishedSpy;
    delete m_progressSpy;
    delete m_blockingUntilInstallationAcknowledgeSpy;
    delete m_requestingInstallationAcknowledgeSpy;
    delete m_startedSpy;

    delete m_pm;
}

void tst_PackageManager::initTestCase()
{
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    auto verbose = amVerboseTest();
    QLoggingCategory::setFilterRules(u"am.*.debug=%1"_s.arg(verbose ? "true" : "false"));

    spyTimeout *= timeoutFactor();

    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_sudo = SudoClient::instance();
    QVERIFY(m_sudo);
    m_fakeSudo = m_sudo->isFallbackImplementation();

    // create a temporary dir (plus sub-dirs) for everything created by this test run
    QVERIFY(m_workDir.isValid());

    // make sure we have a valid hardware-id
    m_hardwareId = u"foobar"_s;

    for (int i = 0; i < PathLocationCount; ++i)
        QVERIFY(QDir().mkdir(pathTo(PathLocation(i))));

    // finally, instantiate the PackageManager and a bunch of signal-spies for its signals
    try {
        PackageDatabase *pdb = new PackageDatabase(QStringList(), pathTo(Internal0));
        m_pm = PackageManager::createInstance(pdb, pathTo(Documents0));
        m_pm->setHardwareId(m_hardwareId);
        m_pm->enableInstaller();
        m_pm->registerPackages();

        // simulate the ApplicationManager stopping blocked applications
        connect(&m_pm->internalSignals, &PackageManagerInternalSignals::registerApplication,
                this, [this](ApplicationInfo *ai, Package *package) {
            connect(package, &Package::blockedChanged, this, [ai, package](bool blocked) {
                if (package->info()->applications().contains(ai) && blocked)
                    package->applicationStoppedDueToBlock(ai->id());
            });
        });
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }

    const QVariantMap iloc = m_pm->installationLocation();
    QCOMPARE(iloc.size(), 3);
    QCOMPARE(iloc.value(u"path"_s).toString(), pathTo(Internal0));
    QVERIFY(iloc.value(u"deviceSize"_s).toLongLong() > 0);
    QVERIFY(iloc.value(u"deviceFree"_s).toLongLong() > 0);
    QVERIFY(iloc.value(u"deviceFree"_s).toLongLong() < iloc.value(u"deviceSize"_s).toLongLong());

    const QVariantMap dloc = m_pm->documentLocation();
    QCOMPARE(dloc.size(), 3);
    QCOMPARE(dloc.value(u"path"_s).toString(), pathTo(Documents0));
    QVERIFY(dloc.value(u"deviceSize"_s).toLongLong() > 0);
    QVERIFY(dloc.value(u"deviceFree"_s).toLongLong() > 0);
    QVERIFY(dloc.value(u"deviceFree"_s).toLongLong() < dloc.value(u"deviceSize"_s).toLongLong());

    m_startedSpy = new QSignalSpy(m_pm, &PackageManager::taskStarted);
    m_requestingInstallationAcknowledgeSpy = new QSignalSpy(m_pm, &PackageManager::taskRequestingInstallationAcknowledge);
    m_blockingUntilInstallationAcknowledgeSpy = new QSignalSpy(m_pm, &PackageManager::taskBlockingUntilInstallationAcknowledge);
    m_progressSpy = new QSignalSpy(m_pm, &PackageManager::taskProgressChanged);
    m_finishedSpy = new QSignalSpy(m_pm, &PackageManager::taskFinished);
    m_failedSpy = new QSignalSpy(m_pm, &PackageManager::taskFailed);

    QString rootcaFile  = AM_TESTDATA_DIR u"certificates/root-ca/root-ca.crt"_s;
    QString devcaFile   = AM_TESTDATA_DIR u"certificates/dev-ca/dev-ca.crt"_s;
    QString storecaFile = AM_TESTDATA_DIR u"certificates/store-ca/store-ca.crt"_s;

    // no CRLs set on purpose to test verification without CRLs at all
    QVERIFY_THROWS_NO_EXCEPTION(m_pm->loadCertificates({ rootcaFile }, { devcaFile },
                                                       { storecaFile }, { }));

    // we do not require valid store signatures for this test run
    m_pm->setDevelopmentMode(PackageManager::DevelopmentMode::System);

    // make sure we have a valid runtime available. The important part is
    // that we have a runtime called "native" - the functionality does not matter.
    RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager(u"native"_s));
}

void tst_PackageManager::cleanupTestCase()
{
    // the real cleanup happens in ~tst_PackageManager, since we also need
    // to call this cleanup from the crash handler
}

void tst_PackageManager::init()
{
    // start with a fresh App1 dir on each test run

    if (!QDir(pathTo(Internal0)).exists())
        QVERIFY(QDir().mkdir(pathTo(Internal0)));
}

void tst_PackageManager::cleanup()
{
    // this helps with reducing the amount of cleanup work required
    // at the end of each test

    try {
        m_pm->cleanupBrokenInstallations();
    } catch (const Exception &e) {
        QFAIL(e.what());
    }

    clearSignalSpies();
    recursiveOperation(pathTo(Internal0), safeRemove);
}

void tst_PackageManager::packageInstallation_data()
{
    QTest::addColumn<QString>("packageName");
    QTest::addColumn<QString>("updatePackageName");
    QTest::addColumn<bool>("devSigned");
    QTest::addColumn<bool>("storeSigned");
    QTest::addColumn<bool>("expectedSuccess");
    QTest::addColumn<bool>("updateExpectedSuccess");
    QTest::addColumn<QVariantMap>("extraMetaData");
    QTest::addColumn<QString>("errorString"); // start with ~ to create a RegExp

    QVariantMap nomd { }; // no meta-data
    QVariantMap extramd = QVariantMap {
        { u"extra"_s, QVariantMap {
            { u"array"_s, QVariantList { 1, 2 } },
            { u"foo"_s, u"bar"_s },
            { u"foo2"_s,u"bar2"_s },
            { u"key"_s, u"value"_s } } },
        { u"extraSigned"_s, QVariantMap {
            { u"sfoo"_s, u"sbar"_s },
            { u"sfoo2"_s, u"sbar2"_s },
            { u"signed-key"_s, u"signed-value"_s },
            { u"signed-object"_s, QVariantMap { { u"k1"_s, u"v1"_s }, { u"k2"_s, u"v2"_s } } }
        } }
    };

    QTest::newRow("normal") \
            << "test.ampkg" << "test-update.ampkg"
            << false << false << true << true << nomd<< "";
    QTest::newRow("no-dev-signed") \
            << "test.ampkg" << ""
            << true << false << false << false << nomd << "cannot install unsigned packages";
    QTest::newRow("dev-signed") \
            << "test-dev-signed.ampkg" << "test-update-dev-signed.ampkg"
            << true << false << true << true << nomd << "";
    QTest::newRow("no-store-signed") \
            << "test.ampkg" << ""
            << false << true << false << false << nomd << "packages without store signatures cannot be installed unless development mode is enabled";
    QTest::newRow("no-store-but-dev-signed") \
            << "test-dev-signed.ampkg" << ""
            << false << true << false << false << nomd << "packages without store signatures cannot be installed unless development mode is enabled";
    QTest::newRow("store-signed-only") \
            << "test-store-signed.ampkg" << ""
            << false << true << false << false << nomd << "cannot install packages with only a store signature";
    QTest::newRow("extra-metadata") \
            << "test-extra.ampkg" << ""
            << false << false << true << false << extramd << "";
    QTest::newRow("extra-metadata-dev-signed") \
            << "test-extra-dev-signed.ampkg" << ""
            << true << false << true << false << extramd << "";
    QTest::newRow("invalid-file-order") \
            << "test-invalid-file-order.ampkg" << ""
            << false << false << false << false << nomd << "The package icon (as stated in info.yaml) must be the second file in the package. Expected 'icon.png', got 'test'";
    QTest::newRow("invalid-header-format") \
            << "test-invalid-header-formatversion.ampkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid format specification: wrong header: expected type 'am-package-header', version '2' or type 'am-package-header', version '1', but instead got type 'am-package-header', version '0'";
    QTest::newRow("invalid-header-diskspaceused") \
            << "test-invalid-header-diskspaceused.ampkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid diskSpaceUsed field (0)";
    QTest::newRow("invalid-header-id") \
            << "test-invalid-header-id.ampkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid packageId field (:invalid)";
    QTest::newRow("non-matching-header-id") \
            << "test-non-matching-header-id.ampkg" << ""
            << false << false << false << false << nomd << "the package identifiers in --PACKAGE-HEADER--' and info.yaml do not match";
    QTest::newRow("tampered-extra-signed-header") \
            << "test-tampered-extra-signed-header.ampkg" << ""
            << false << false << false << false << nomd << "~package digest mismatch.*";
    QTest::newRow("invalid-info.yaml") \
            << "test-invalid-info.ampkg" << ""
            << false << false << false << false << nomd << "~.*did not find expected key.*";
    QTest::newRow("invalid-info.yaml-id") \
            << "test-invalid-info-id.ampkg" << ""
            << false << false << false << false << nomd << "~.*the identifier \\(:invalid\\) is not a valid package-id: must consist of printable ASCII characters only, except any of .*";
    QTest::newRow("invalid-footer-signature") \
            << "test-invalid-footer-signature.ampkg" << ""
            << true << false << false << false << nomd << "~could not verify the package's developer signature:.*";
    QTest::newRow("no-icon") \
            << "test-no-icon.ampkg" << ""
            << false << false << true << false << nomd << "";
    QTest::newRow("icon-in-subdir") \
            << "test-icon-in-subdir.ampkg" << ""
            << false << false << false << false << nomd << "the icon must be located in the package's root directory";
    QTest::newRow("non-existent-icon") \
            << "test-non-existent-icon.ampkg" << ""
            << false << false << false << false << nomd << "~.*must be the second file in the package.*";
}

// this test function is a bit of a kitchen sink, but the basic boiler plate
// code of testing the results of an installation is the biggest part and it
// is always the same.
void tst_PackageManager::packageInstallation()
{
    QFETCH(QString, packageName);
    QFETCH(QString, updatePackageName);
    QFETCH(bool, devSigned);
    QFETCH(bool, storeSigned);
    QFETCH(bool, expectedSuccess);
    QFETCH(bool, updateExpectedSuccess);
    QFETCH(QVariantMap, extraMetaData);
    QFETCH(QString, errorString);

    QString installationDir = m_pm->installationLocation().value(u"path"_s).toString();
    QString documentDir = m_pm->documentLocation().value(u"path"_s).toString();

    DevMode devMode(storeSigned ? PackageManager::DevelopmentMode::Disabled
                                : PackageManager::DevelopmentMode::System,
                    !storeSigned && !devSigned);

    QDir dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString instReport = dataDir.absoluteFilePath(u"installation-reports/test-pkg.yaml"_s);

    int lastPass = (updatePackageName.isEmpty() ? 1 : 2);
    // pass 1 is the installation / pass 2 is the update (if needed)
    for (int pass = 1; pass <= lastPass; ++pass) {
        // this makes the results a bit ugly to look at, but it helps with debugging a lot
        if (pass > 1)
            qCDebug(LogSystem) << "Pass" << pass;

        // install (or update) the package

        QString url = AM_TESTDATA_DIR u"packages/" + (pass == 1 ? packageName : updatePackageName);

        QString taskId = m_pm->startPackageInstallation(url);
        QVERIFY(!taskId.isEmpty());
        m_pm->acknowledgePackageInstallation(taskId);

        // check received signals...

        if (pass == 1 ? !expectedSuccess : !updateExpectedSuccess) {
            // ...in case of expected failure

            QVERIFY(m_failedSpy->wait(spyTimeout));
            QCOMPARE(m_failedSpy->first()[0].toString(), taskId);

            QT_AM_CHECK_ERRORSTRING(m_failedSpy->first()[2].toString(), errorString);
        } else {
            // ...in case of expected success

            QVERIFY(m_finishedSpy->wait(spyTimeout));
            QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
            QVERIFY(!m_progressSpy->isEmpty());
            QCOMPARE(m_progressSpy->last()[0].toString(), taskId);
            QCOMPARE(m_progressSpy->last()[1].toDouble(), double(1));

            // check files

            //QDirIterator it(m_workDir.path(), QDirIterator::Subdirectories);
            //while (it.hasNext()) { qDebug() << it.next(); }

            QVERIFY(QFile::exists(instReport));
            QVERIFY(QDir(documentDir + u"/test-pkg"_s).exists());

            QString fileCheckPath = installationDir + u"/test-pkg"_s;

            // now check the installed files

            QStringList files = QDir(fileCheckPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
#if defined(Q_OS_WIN)
            // files starting with . are not considered hidden on Windows
            files = files.filter(QRegularExpression(u"^[^.].*"_s));
#elif defined(Q_OS_MACOS)
            // starting with Qt7 file names will be reported as-is in macOS decomposed form
            std::for_each(files.begin(), files.end(), [](QString &s) { s = s.normalized(QString::NormalizationForm_C); });
#endif
            files.sort();

            QStringList expectedFiles = { u"icon.png"_s, u"info.yaml"_s, u"test"_s,
                                         QString::fromUtf8("t\xc3\xa4st") };
            if (QByteArray(QTest::currentDataTag()).contains("no-icon"))
                expectedFiles.removeOne(u"icon.png"_s);

            QVERIFY2(files == expectedFiles,
                     qPrintable(files.join(u", "_s) + u" != "_s + expectedFiles.join(u", "_s)));

            QFile f(fileCheckPath + u"/test"_s);
            QVERIFY(f.open(QFile::ReadOnly));
            QCOMPARE(f.readAll(), QByteArray(pass == 1 ? "test\n" : "test update\n"));
            f.close();

            // check metadata
            QCOMPARE(m_requestingInstallationAcknowledgeSpy->count(), 1);
            QVariantMap extra = m_requestingInstallationAcknowledgeSpy->first()[2].toMap();
            QVariantMap extraSigned = m_requestingInstallationAcknowledgeSpy->first()[3].toMap();
            if (extraMetaData.value(u"extra"_s).toMap() != extra) {
                qDebug() << "Actual: " << extra;
                qDebug() << "Expected: " << extraMetaData.value(u"extra"_s).toMap();
                QVERIFY(extraMetaData == extra);
            }
            if (extraMetaData.value(u"extraSigned"_s).toMap() != extraSigned) {
                qDebug() << "Actual: " << extraSigned;
                qDebug() << "Expected: " << extraMetaData.value(u"extraSigned"_s).toMap();
                QVERIFY(extraMetaData == extraSigned);
            }

            // check if the meta-data was saved to the installation report correctly
            QVERIFY2(m_pm->installedPackageExtraMetaData(u"test-pkg"_s) == extra,
                     "Extra meta-data was not correctly saved to installation report");
            QVERIFY2(m_pm->installedPackageExtraSignedMetaData(u"test-pkg"_s) == extraSigned,
                     "Extra signed meta-data was not correctly saved to installation report");
        }
        if (pass == lastPass && expectedSuccess) {
            // remove package again

            clearSignalSpies();
            taskId = m_pm->removePackage(u"test-pkg"_s, false);
            QVERIFY(!taskId.isEmpty());

            // check signals
            QVERIFY(m_finishedSpy->wait(spyTimeout));
            QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        }
        clearSignalSpies();
    }
    // check that all files are gone

    for (PathLocation pl: { Internal0, Documents0 }) {
        QStringList entries = QDir(pathTo(pl)).entryList({ u"test-pkg*"_s });
        QVERIFY2(entries.isEmpty(), qPrintable(pathTo(pl) + u": "_s + entries.join(u", "_s)));
    }
}


Q_DECLARE_METATYPE(std::function<bool()>)
typedef QMultiMap<QByteArray, std::function<bool()>> FunctionMap;
Q_DECLARE_METATYPE(FunctionMap)

void tst_PackageManager::simulateErrorConditions_data()
{
     QTest::addColumn<bool>("testUpdate");
     QTest::addColumn<QString>("errorString");
     QTest::addColumn<FunctionMap>("functions");

#ifdef Q_OS_LINUX
     QTest::newRow("applications-dir-read-only") \
             << false << "~could not create a temporary extraction directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(qPrintable(pathTo(Internal0)), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(qPrintable(pathTo(Internal0)), 0777) == 0; } } };

     QTest::newRow("documents-dir-read-only") \
             << false << "~could not create the document directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(qPrintable(pathTo(Documents0)), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(qPrintable(pathTo(Documents0)), 0777) == 0; } } };
#endif
}

void tst_PackageManager::simulateErrorConditions()
{
#ifndef Q_OS_LINUX
    QSKIP("Only tested on Linux");
#endif

    QFETCH(bool, testUpdate);
    QFETCH(QString, errorString);
    QFETCH(FunctionMap, functions);

    QString taskId;

    if (testUpdate) {
        // the check will run when updating a package, so we need to install it first

        taskId = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);
        QVERIFY(!taskId.isEmpty());
        m_pm->acknowledgePackageInstallation(taskId);
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        clearSignalSpies();
    }

    const auto beforeStart = functions.values("before-start");
    for (const auto &f : beforeStart)
        QVERIFY(f());

    taskId = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);

    const auto afterStart = functions.values("after-start");
    for (const auto &f : afterStart)
        QVERIFY(f());

    m_pm->acknowledgePackageInstallation(taskId);

    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
    QT_AM_CHECK_ERRORSTRING(m_failedSpy->first()[2].toString(), errorString);
    clearSignalSpies();

    const auto afterFailed = functions.values("after-failed");
    for (const auto &f : afterFailed)
        QVERIFY(f());

    if (testUpdate) {
        taskId = m_pm->removePackage(u"test-pkg"_s, false);

        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    }
}


void tst_PackageManager::cancelPackageInstallation_data()
{
    QTest::addColumn<bool>("expectedResult");

    // please note that the data tag names are used in the actual test function below!

    QTest::newRow("before-started-signal") << true;
    QTest::newRow("after-started-signal") << true;
    QTest::newRow("after-blocking-until-installation-acknowledge-signal") << true;
    QTest::newRow("after-finished-signal") << false;
}

void tst_PackageManager::cancelPackageInstallation()
{
    QFETCH(bool, expectedResult);

    QString taskId = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);
    QVERIFY(!taskId.isEmpty());

    if (isDataTag("before-started-signal")) {
        QCOMPARE(m_pm->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-started-signal")) {
        QVERIFY(m_startedSpy->wait(spyTimeout));
        QCOMPARE(m_startedSpy->first()[0].toString(), taskId);
        QCOMPARE(m_pm->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-blocking-until-installation-acknowledge-signal")) {
        QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
        QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), taskId);
        QCOMPARE(m_pm->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-finished-signal")) {
        m_pm->acknowledgePackageInstallation(taskId);
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        QCOMPARE(m_pm->cancelTask(taskId), expectedResult);
    }

    if (expectedResult) {
        if (!m_startedSpy->isEmpty()) {
            QVERIFY(m_failedSpy->wait(spyTimeout));
            QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
            QCOMPARE(m_failedSpy->first()[1].toInt(), int(Error::Canceled));
        }
    } else {
        clearSignalSpies();

        taskId = m_pm->removePackage(u"test-pkg"_s, false);
        QVERIFY(!taskId.isEmpty());
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    }
    clearSignalSpies();
}

void tst_PackageManager::parallelPackageInstallation()
{
    QString task1Id = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);
    QVERIFY(!task1Id.isEmpty());
    QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
    QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), task1Id);

    QString task2Id = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/other-test-dev-signed.ampkg"_s);
    QVERIFY(!task2Id.isEmpty());
    m_pm->acknowledgePackageInstallation(task2Id);
    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), task2Id);

    clearSignalSpies();
    m_pm->acknowledgePackageInstallation(task1Id);
    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), task1Id);

    clearSignalSpies();
}

void tst_PackageManager::doublePackageInstallation()
{
    QString task1Id = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);
    QVERIFY(!task1Id.isEmpty());
    QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
    QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), task1Id);

    QString task2Id = m_pm->startPackageInstallation(AM_TESTDATA_DIR u"packages/test-dev-signed.ampkg"_s);
    QVERIFY(!task2Id.isEmpty());
    m_pm->acknowledgePackageInstallation(task2Id);
    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), task2Id);
    QCOMPARE(m_failedSpy->first()[2].toString(), u"Cannot install the same package test-pkg multiple times in parallel");

    clearSignalSpies();
    m_pm->cancelTask(task1Id);
    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), task1Id);

    clearSignalSpies();
}

void tst_PackageManager::validateDnsName_data()
{
    QTest::addColumn<QString>("dnsName");
    QTest::addColumn<int>("minParts");
    QTest::addColumn<bool>("valid");

    // passes
    QTest::newRow("normal") << "com.pelagicore.test" << 3 << true;
    QTest::newRow("shortest") << "c.p.t" << 3 << true;
    QTest::newRow("valid-chars") << "1-2.c-d.3.z" << 3 << true;
    QTest::newRow("longest-part") << "com.012345678901234567890123456789012345678901234567890123456789012.test" << 3 << true;
    QTest::newRow("longest-name") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.test" << 3 << true;
    QTest::newRow("max-part-cnt") << "a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.0.12" << 3 << true;
    QTest::newRow("one-part-only") << "c" << 1 << true;

    // failures
    QTest::newRow("too-few-parts") << "com.pelagicore" << 3 << false;
    QTest::newRow("empty-part") << "com..test" << 3 << false;
    QTest::newRow("empty") << "" << 3 << false;
    QTest::newRow("dot-only") << "." << 3 << false;
    QTest::newRow("invalid-char1") << "com.pelagi_core.test" << 3 << false;
    QTest::newRow("invalid-char2") << "com.pelagi#core.test" << 3 << false;
    QTest::newRow("invalid-char3") << "com.pelagi$core.test" << 3 << false;
    QTest::newRow("invalid-char4") << "com.pelagi@core.test" << 3 << false;
    QTest::newRow("unicode-char") << QString::fromUtf8("c\xc3\xb6m.pelagicore.test") << 3 << false;
    QTest::newRow("upper-case") << "test-pkg" << 3 << false;
    QTest::newRow("dash-at-start") << "com.-pelagicore.test" << 3 << false;
    QTest::newRow("dash-at-end") << "com.pelagicore-.test" << 3 << false;
    QTest::newRow("part-too-long") << "com.x012345678901234567890123456789012345678901234567890123456789012.test" << 3 << false;
}

void tst_PackageManager::validateDnsName()
{
    QFETCH(QString, dnsName);
    QFETCH(int, minParts);
    QFETCH(bool, valid);

    QString errorString;
    bool result = m_pm->validateDnsName(dnsName, minParts);

    QVERIFY2(valid == result, qPrintable(errorString));
}

void tst_PackageManager::compareVersions_data()
{
    QTest::addColumn<QString>("version1");
    QTest::addColumn<QString>("version2");
    QTest::addColumn<int>("result");


    QTest::newRow("1") << "" << "" << 0;
    QTest::newRow("2") << "0" << "0" << 0;
    QTest::newRow("3") << "foo" << "foo" << 0;
    QTest::newRow("4") << "1foo" << "1foo" << 0;
    QTest::newRow("5") << "foo1" << "foo1" << 0;
    QTest::newRow("6") << "13.403.51-alpha2+git" << "13.403.51-alpha2+git" << 0;
    QTest::newRow("7") << "1" << "2" << -1;
    QTest::newRow("8") << "2" << "1" << 1;
    QTest::newRow("9") << "1.0" << "2.0" << -1;
    QTest::newRow("10") << "1.99" << "2.0" << -1;
    QTest::newRow("11") << "1.9" << "11" << -1;
    QTest::newRow("12") << "9" << "10" << -1;
    QTest::newRow("13") << "9a" << "10" << -1;
    QTest::newRow("14") << "9-a" << "10" << -1;
    QTest::newRow("15") << "13.403.51-alpha2+gi" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("16") << "13.403.51-alpha1+git" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("17") << "13.403.51-alpha2+git" << "13.403.51-beta1+git" << -1;
    QTest::newRow("18") << "13.403.51-alpha2+git" << "13.403.52" << -1;
    QTest::newRow("19") << "13.403.51-alpha2+git" << "13.403.52-alpha2+git" << -1;
    QTest::newRow("20") << "13.403.51-alpha2+git" << "13.404" << -1;
    QTest::newRow("21") << "13.402" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("22") << "12.403.51-alpha2+git" << "13.403.51-alpha2+git" << -1;
}

void tst_PackageManager::compareVersions()
{
    QFETCH(QString, version1);
    QFETCH(QString, version2);
    QFETCH(int, result);

    int cmp = m_pm->compareVersions(version1, version2);
    QCOMPARE(cmp, result);

    if (result) {
        cmp = m_pm->compareVersions(version2, version1);
        QCOMPARE(cmp, -result);
    }
}

QT_AM_VERBOSE_TEST_MAIN(tst_PackageManager, \
    try { \
        Sudo::forkServer(Sudo::DropPrivilegesPermanently); \
        tst_PackageManager::startedSudoServer = true; \
    } catch (const Exception &e) { \
        tst_PackageManager::sudoServerError = e.errorString(); \
    } \
    QTEST_QAPP_SETUP(QCoreApplication))

#include "tst_packagemanager.moc"
