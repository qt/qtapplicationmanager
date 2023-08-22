// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include <functional>

#include "packagemanager.h"
#include "packagedatabase.h"
#include "package.h"
#include "applicationinfo.h"
#include "sudo.h"
#include "utilities.h"
#include "error.h"
#include "private/packageutilities_p.h"
#include "runtimefactory.h"
#include "qmlinprocessruntime.h"
#include "packageutilities.h"

#include "../error-checking.h"

QT_USE_NAMESPACE_AM

static bool startedSudoServer = false;
static QString sudoServerError;

static int spyTimeout = 5000; // shorthand for specifying QSignalSpy timeouts

// RAII to reset the global attribute
class AllowInstallations
{
public:
    enum Type {
        AllowUnsigned,
        RequireDevSigned,
        RequireStoreSigned
    };

    AllowInstallations(Type t)
        : m_oldUnsigned(PackageManager::instance()->allowInstallationOfUnsignedPackages())
        , m_oldDevMode(PackageManager::instance()->developmentMode())
    {
        switch (t) {
        case AllowUnsigned:
            PackageManager::instance()->setAllowInstallationOfUnsignedPackages(true);
            PackageManager::instance()->setDevelopmentMode(false);
            break;
        case RequireDevSigned:
            PackageManager::instance()->setAllowInstallationOfUnsignedPackages(false);
            PackageManager::instance()->setDevelopmentMode(true);
            break;
        case RequireStoreSigned:
            PackageManager::instance()->setAllowInstallationOfUnsignedPackages(false);
            PackageManager::instance()->setDevelopmentMode(false);
            break;
        }
    }
    ~AllowInstallations()
    {
        PackageManager::instance()->setAllowInstallationOfUnsignedPackages(m_oldUnsigned);
        PackageManager::instance()->setDevelopmentMode(m_oldDevMode);
    }
private:
    bool m_oldUnsigned;
    bool m_oldDevMode;
};

class tst_PackageManager : public QObject
{
    Q_OBJECT

public:
    tst_PackageManager(QObject *parent = nullptr);
    ~tst_PackageManager();

private slots:
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
        case Internal0:      base = qSL("internal"); break;
        case Documents0:     base = qSL("documents"); break;
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
            base = workDir.absoluteFilePath(base + qL1C('/') + sub);

        if (QDir(base).exists())
            return base + qL1C('/');
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
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    bool verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    if (!verbose)
        QLoggingCategory::setFilterRules(qSL("am.installer.debug=false"));
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";

    spyTimeout *= timeoutFactor();

    QVERIFY(PackageUtilities::checkCorrectLocale());
    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_sudo = SudoClient::instance();
    QVERIFY(m_sudo);
    m_fakeSudo = m_sudo->isFallbackImplementation();

    // create a temporary dir (plus sub-dirs) for everything created by this test run
    QVERIFY(m_workDir.isValid());

    // make sure we have a valid hardware-id
    m_hardwareId = qSL("foobar");

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
    QCOMPARE(iloc.value(qSL("path")).toString(), pathTo(Internal0));
    QVERIFY(iloc.value(qSL("deviceSize")).toLongLong() > 0);
    QVERIFY(iloc.value(qSL("deviceFree")).toLongLong() > 0);
    QVERIFY(iloc.value(qSL("deviceFree")).toLongLong() < iloc.value(qSL("deviceSize")).toLongLong());

    const QVariantMap dloc = m_pm->documentLocation();
    QCOMPARE(dloc.size(), 3);
    QCOMPARE(dloc.value(qSL("path")).toString(), pathTo(Documents0));
    QVERIFY(dloc.value(qSL("deviceSize")).toLongLong() > 0);
    QVERIFY(dloc.value(qSL("deviceFree")).toLongLong() > 0);
    QVERIFY(dloc.value(qSL("deviceFree")).toLongLong() < dloc.value(qSL("deviceSize")).toLongLong());

    m_startedSpy = new QSignalSpy(m_pm, &PackageManager::taskStarted);
    m_requestingInstallationAcknowledgeSpy = new QSignalSpy(m_pm, &PackageManager::taskRequestingInstallationAcknowledge);
    m_blockingUntilInstallationAcknowledgeSpy = new QSignalSpy(m_pm, &PackageManager::taskBlockingUntilInstallationAcknowledge);
    m_progressSpy = new QSignalSpy(m_pm, &PackageManager::taskProgressChanged);
    m_finishedSpy = new QSignalSpy(m_pm, &PackageManager::taskFinished);
    m_failedSpy = new QSignalSpy(m_pm, &PackageManager::taskFailed);

    // crypto stuff - we need to load the root CA and developer CA certificates

    QFile devcaFile(qL1S(AM_TESTDATA_DIR "certificates/devca.crt"));
    QFile storecaFile(qL1S(AM_TESTDATA_DIR "certificates/store.crt"));
    QFile caFile(qL1S(AM_TESTDATA_DIR "certificates/ca.crt"));
    QVERIFY2(devcaFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));
    QVERIFY2(storecaFile.open(QIODevice::ReadOnly), qPrintable(storecaFile.errorString()));
    QVERIFY2(caFile.open(QIODevice::ReadOnly), qPrintable(caFile.errorString()));

    QList<QByteArray> chainOfTrust;
    chainOfTrust << devcaFile.readAll() << caFile.readAll();
    QVERIFY(!chainOfTrust.at(0).isEmpty());
    QVERIFY(!chainOfTrust.at(1).isEmpty());
    m_pm->setCACertificates(chainOfTrust);

    // we do not require valid store signatures for this test run

    m_pm->setDevelopmentMode(true);

    // make sure we have a valid runtime available. The important part is
    // that we have a runtime called "native" - the functionality does not matter.
    RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager(qSL("native")));
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
        { qSL("extra"), QVariantMap {
            { qSL("array"), QVariantList { 1, 2 } },
            { qSL("foo"), qSL("bar") },
            { qSL("foo2"),qSL("bar2") },
            { qSL("key"), qSL("value") } } },
        { qSL("extraSigned"), QVariantMap {
            { qSL("sfoo"), qSL("sbar") },
            { qSL("sfoo2"), qSL("sbar2") },
            { qSL("signed-key"), qSL("signed-value") },
            { qSL("signed-object"), QVariantMap { { qSL("k1"), qSL("v1") }, { qSL("k2"), qSL("v2") } } }
        } }
    };

    QTest::newRow("normal") \
            << "test.appkg" << "test-update.appkg"
            << false << false << true << true << nomd<< "";
    QTest::newRow("no-dev-signed") \
            << "test.appkg" << ""
            << true << false << false << false << nomd << "cannot install unsigned packages";
    QTest::newRow("dev-signed") \
            << "test-dev-signed.appkg" << "test-update-dev-signed.appkg"
            << true << false << true << true << nomd << "";
    QTest::newRow("no-store-signed") \
            << "test.appkg" << ""
            << false << true << false << false << nomd << "cannot install unsigned packages";
    QTest::newRow("no-store-but-dev-signed") \
            << "test-dev-signed.appkg" << ""
            << false << true << false << false << nomd << "cannot install development packages on consumer devices";
    QTest::newRow("store-signed") \
            << "test-store-signed.appkg" << ""
            << false << true << true << false << nomd << "";
    QTest::newRow("extra-metadata") \
            << "test-extra.appkg" << ""
            << false << false << true << false << extramd << "";
    QTest::newRow("extra-metadata-dev-signed") \
            << "test-extra-dev-signed.appkg" << ""
            << true << false << true << false << extramd << "";
    QTest::newRow("invalid-file-order") \
            << "test-invalid-file-order.appkg" << ""
            << false << false << false << false << nomd << "The package icon (as stated in info.yaml) must be the second file in the package. Expected 'icon.png', got 'test'";
    QTest::newRow("invalid-header-format") \
            << "test-invalid-header-formatversion.appkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid format specification: wrong header: expected type 'am-package-header', version '2' or type 'am-package-header', version '1', but instead got type 'am-package-header', version '0'";
    QTest::newRow("invalid-header-diskspaceused") \
            << "test-invalid-header-diskspaceused.appkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid diskSpaceUsed field (0)";
    QTest::newRow("invalid-header-id") \
            << "test-invalid-header-id.appkg" << ""
            << false << false << false << false << nomd << "metadata has an invalid packageId field (:invalid)";
    QTest::newRow("non-matching-header-id") \
            << "test-non-matching-header-id.appkg" << ""
            << false << false << false << false << nomd << "the package identifiers in --PACKAGE-HEADER--' and info.yaml do not match";
    QTest::newRow("tampered-extra-signed-header") \
            << "test-tampered-extra-signed-header.appkg" << ""
            << false << false << false << false << nomd << "~package digest mismatch.*";
    QTest::newRow("invalid-info.yaml") \
            << "test-invalid-info.appkg" << ""
            << false << false << false << false << nomd << "~.*did not find expected key.*";
    QTest::newRow("invalid-info.yaml-id") \
            << "test-invalid-info-id.appkg" << ""
            << false << false << false << false << nomd << "~.*the identifier \\(:invalid\\) is not a valid package-id: must consist of printable ASCII characters only, except any of .*";
    QTest::newRow("invalid-footer-signature") \
            << "test-invalid-footer-signature.appkg" << ""
            << true << false << false << false << nomd << "could not verify the package's developer signature";
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

    QString installationDir = m_pm->installationLocation().value(qSL("path")).toString();
    QString documentDir = m_pm->documentLocation().value(qSL("path")).toString();

    AllowInstallations allow(storeSigned ? AllowInstallations::RequireStoreSigned
                                         : (devSigned ? AllowInstallations::RequireDevSigned
                                                      : AllowInstallations::AllowUnsigned));

    int lastPass = (updatePackageName.isEmpty() ? 1 : 2);
    // pass 1 is the installation / pass 2 is the update (if needed)
    for (int pass = 1; pass <= lastPass; ++pass) {
        // this makes the results a bit ugly to look at, but it helps with debugging a lot
        if (pass > 1)
            qInfo("Pass %d", pass);

        // install (or update) the package

        QUrl url = QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/")
                                       + (pass == 1 ? packageName : updatePackageName));
        QString taskId = m_pm->startPackageInstallation(url);
        QVERIFY(!taskId.isEmpty());
        m_pm->acknowledgePackageInstallation(taskId);

        // check received signals...

        if (pass == 1 ? !expectedSuccess : !updateExpectedSuccess) {
            // ...in case of expected failure

            QVERIFY(m_failedSpy->wait(spyTimeout));
            QCOMPARE(m_failedSpy->first()[0].toString(), taskId);

            AM_CHECK_ERRORSTRING(m_failedSpy->first()[2].toString(), errorString);
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

            QVERIFY(QFile::exists(installationDir + qSL("/com.pelagicore.test/.installation-report.yaml")));
            QVERIFY(QDir(documentDir + qSL("/com.pelagicore.test")).exists());

            QString fileCheckPath = installationDir + qSL("/com.pelagicore.test");

            // now check the installed files

            QStringList files = QDir(fileCheckPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
#if defined(Q_OS_WIN)
            // files starting with . are not considered hidden on Windows
            files = files.filter(QRegularExpression(qSL("^[^.].*")));
#endif
            files.sort();

            QVERIFY2(files == QStringList({ qSL("icon.png"), qSL("info.yaml"), qSL("test"), QString::fromUtf8("t\xc3\xa4st") }),
                     qPrintable(files.join(qSL(", "))));

            QFile f(fileCheckPath + qSL("/test"));
            QVERIFY(f.open(QFile::ReadOnly));
            QCOMPARE(f.readAll(), QByteArray(pass == 1 ? "test\n" : "test update\n"));
            f.close();

            // check metadata
            QCOMPARE(m_requestingInstallationAcknowledgeSpy->count(), 1);
            QVariantMap extra = m_requestingInstallationAcknowledgeSpy->first()[2].toMap();
            QVariantMap extraSigned = m_requestingInstallationAcknowledgeSpy->first()[3].toMap();
            if (extraMetaData.value(qSL("extra")).toMap() != extra) {
                qDebug() << "Actual: " << extra;
                qDebug() << "Expected: " << extraMetaData.value(qSL("extra")).toMap();
                QVERIFY(extraMetaData == extra);
            }
            if (extraMetaData.value(qSL("extraSigned")).toMap() != extraSigned) {
                qDebug() << "Actual: " << extraSigned;
                qDebug() << "Expected: " << extraMetaData.value(qSL("extraSigned")).toMap();
                QVERIFY(extraMetaData == extraSigned);
            }

            // check if the meta-data was saved to the installation report correctly
            QVERIFY2(m_pm->installedPackageExtraMetaData(qSL("com.pelagicore.test")) == extra,
                     "Extra meta-data was not correctly saved to installation report");
            QVERIFY2(m_pm->installedPackageExtraSignedMetaData(qSL("com.pelagicore.test")) == extraSigned,
                     "Extra signed meta-data was not correctly saved to installation report");
        }
        if (pass == lastPass && expectedSuccess) {
            // remove package again

            clearSignalSpies();
            taskId = m_pm->removePackage(qSL("com.pelagicore.test"), false);
            QVERIFY(!taskId.isEmpty());

            // check signals
            QVERIFY(m_finishedSpy->wait(spyTimeout));
            QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        }
        clearSignalSpies();
    }
    // check that all files are gone

    for (PathLocation pl: { Internal0, Documents0 }) {
        QStringList entries = QDir(pathTo(pl)).entryList({ qSL("com.pelagicore.test*") });
        QVERIFY2(entries.isEmpty(), qPrintable(pathTo(pl) + qSL(": ") + entries.join(qSL(", "))));
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
             << false << "~could not create installation directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(pathTo(Internal0).toLocal8Bit(), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(pathTo(Internal0).toLocal8Bit(), 0777) == 0; } } };

     QTest::newRow("documents-dir-read-only") \
             << false << "~could not create the document directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(pathTo(Documents0).toLocal8Bit(), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(pathTo(Documents0).toLocal8Bit(), 0777) == 0; } } };
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

        taskId = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));
        QVERIFY(!taskId.isEmpty());
        m_pm->acknowledgePackageInstallation(taskId);
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        clearSignalSpies();
    }

    const auto beforeStart = functions.values("before-start");
    for (const auto &f : beforeStart)
        QVERIFY(f());

    taskId = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));

    const auto afterStart = functions.values("after-start");
    for (const auto &f : afterStart)
        QVERIFY(f());

    m_pm->acknowledgePackageInstallation(taskId);

    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
    AM_CHECK_ERRORSTRING(m_failedSpy->first()[2].toString(), errorString);
    clearSignalSpies();

    const auto afterFailed = functions.values("after-failed");
    for (const auto &f : afterFailed)
        QVERIFY(f());

    if (testUpdate) {
        taskId = m_pm->removePackage(qSL("com.pelagicore.test"), false);

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

    QString taskId = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));
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

        taskId = m_pm->removePackage(qSL("com.pelagicore.test"), false);
        QVERIFY(!taskId.isEmpty());
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    }
    clearSignalSpies();
}

void tst_PackageManager::parallelPackageInstallation()
{
    QString task1Id = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));
    QVERIFY(!task1Id.isEmpty());
    QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
    QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), task1Id);

    QString task2Id = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/bigtest-dev-signed.appkg")));
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
    QString task1Id = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));
    QVERIFY(!task1Id.isEmpty());
    QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
    QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), task1Id);

    QString task2Id = m_pm->startPackageInstallation(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test-dev-signed.appkg")));
    QVERIFY(!task2Id.isEmpty());
    m_pm->acknowledgePackageInstallation(task2Id);
    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), task2Id);
    QCOMPARE(m_failedSpy->first()[2].toString(), qL1S("Cannot install the same package com.pelagicore.test multiple times in parallel"));

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
    QTest::newRow("invalid-char3") << "com.pelagi@core.test" << 3 << false;
    QTest::newRow("unicode-char") << QString::fromUtf8("c\xc3\xb6m.pelagicore.test") << 3 << false;
    QTest::newRow("upper-case") << "com.Pelagicore.test" << 3 << false;
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
    QTest::newRow("12") << "9a" << "10" << -1;
    QTest::newRow("13") << "9-a" << "10" << -1;
    QTest::newRow("14") << "13.403.51-alpha2+gi" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("15") << "13.403.51-alpha1+git" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("16") << "13.403.51-alpha2+git" << "13.403.51-beta1+git" << -1;
    QTest::newRow("17") << "13.403.51-alpha2+git" << "13.403.52" << -1;
    QTest::newRow("18") << "13.403.51-alpha2+git" << "13.403.52-alpha2+git" << -1;
    QTest::newRow("19") << "13.403.51-alpha2+git" << "13.404" << -1;
    QTest::newRow("20") << "13.402" << "13.403.51-alpha2+git" << -1;
    QTest::newRow("21") << "12.403.51-alpha2+git" << "13.403.51-alpha2+git" << -1;
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

static tst_PackageManager *tstPackageManager = nullptr;

int main(int argc, char **argv)
{
    PackageUtilities::ensureCorrectLocale();

    try {
        Sudo::forkServer(Sudo::DropPrivilegesPermanently);
        startedSudoServer = true;
    } catch (...) { }

    QCoreApplication a(argc, argv);
    tstPackageManager = new tst_PackageManager(&a);

    return QTest::qExec(tstPackageManager, argc, argv);
}

#include "tst_applicationinstaller.moc"
