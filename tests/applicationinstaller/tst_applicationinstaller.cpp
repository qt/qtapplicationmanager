/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <functional>

#include "applicationinstaller.h"
#include "applicationmanager.h"
#include "application.h"
#include "sudo.h"
#include "utilities.h"
#include "error.h"
#include "private/package_p.h"
#include "runtimefactory.h"
#include "qmlinprocessruntime.h"
#include "package.h"

#ifdef Q_OS_LINUX
#  include <signal.h>
#  include <linux/loop.h>
#  include <sys/ioctl.h>
#endif

#include "../sudo-cleanup.h"
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
        AllowUnsinged,
        RequireDevSigned,
        RequireStoreSigned
    };

    AllowInstallations(Type t)
        : m_oldUnsigned(ApplicationInstaller::instance()->allowInstallationOfUnsignedPackages())
        , m_oldDevMode(ApplicationInstaller::instance()->developmentMode())
    {
        switch (t) {
        case AllowUnsinged:
            ApplicationInstaller::instance()->setAllowInstallationOfUnsignedPackages(true);
            ApplicationInstaller::instance()->setDevelopmentMode(false);
            break;
        case RequireDevSigned:
            ApplicationInstaller::instance()->setAllowInstallationOfUnsignedPackages(false);
            ApplicationInstaller::instance()->setDevelopmentMode(true);
            break;
        case RequireStoreSigned:
            ApplicationInstaller::instance()->setAllowInstallationOfUnsignedPackages(false);
            ApplicationInstaller::instance()->setDevelopmentMode(false);
            break;
        }
    }
    ~AllowInstallations()
    {
        ApplicationInstaller::instance()->setAllowInstallationOfUnsignedPackages(m_oldUnsigned);
        ApplicationInstaller::instance()->setDevelopmentMode(m_oldDevMode);
    }
private:
    bool m_oldUnsigned;
    bool m_oldDevMode;
};

class tst_ApplicationInstaller : public QObject
{
    Q_OBJECT

public:
    tst_ApplicationInstaller(QObject *parent = nullptr);
    ~tst_ApplicationInstaller();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    //TODO: test AI::cleanupBrokenInstallations() before calling cleanup() the first time!

    void installationLocations();

    void packageActivation();

    void packageInstallation_data();
    void packageInstallation();

    void removeAppOnMissingSDCard();

    void simulateErrorConditions_data();
    void simulateErrorConditions();

    void cancelPackageInstallation_data();
    void cancelPackageInstallation();

    void validateDnsName_data();
    void validateDnsName();

    void compareVersions_data();
    void compareVersions();

public:
    enum PathLocation {
        TemporaryMount = 0,
        Manifests,
        ImageMounts,
        Internal0,
        Documents0,
        Internal1,
        Documents1,
        SDCard0,
        SDCard0Images,
        SDCard1,
        SDCard1Images,

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
        case Manifests:      base = "manifests"; break;
        case TemporaryMount: base = "mnt"; break;
        case ImageMounts:    base = "image-mounts"; break;
        case Internal0:      base = "internal-0"; break;
        case Documents0:     base = "documents-0"; break;
        case Internal1:      base = "internal-1"; break;
        case Documents1:     base = "documents-1"; break;
        case SDCard0:        base = "sdcard0"; break;
        case SDCard0Images:  base = "sdcard0/" + m_hardwareId; break;
        case SDCard1:        base = "sdcard1"; break;
        case SDCard1Images:  base = "sdcard1/" + m_hardwareId; break;
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
            base = workDir.absoluteFilePath(base + '/' + sub);

        if (QDir(base).exists())
            return base + '/';
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
        m_packageActivatedSpy->clear();
        m_packageDeactivatedSpy->clear();
    }

    bool checkPackageActivation(const QString &appId, int waitmsec = 0)
    {
        return internalCheckActivation(m_packageActivatedSpy, appId, waitmsec);
    }

    bool checkPackageDeactivation(const QString &appId, int waitmsec = 0)
    {
        return internalCheckActivation(m_packageDeactivatedSpy, appId, waitmsec);
    }

    bool internalCheckActivation(QSignalSpy *spy, const QString &appId, int waitmsec)
    {
        auto check = [spy, appId]() -> bool
        {
            for (int i = 0; i < spy->count(); ++i) {
                const QVariantList &paramList = spy->at(i);
                if ((paramList.size() == 2) && (paramList.at(0).toString() == appId))
                    return paramList.at(1).toBool();
            }
            return false;
        };

        forever {
            bool result = check();
            if (!result && waitmsec) {
                QTest::qWait(100);
                waitmsec -= qMin(100, waitmsec);
            } else {
                return result;
            }
        }
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
    QString m_loopbackForSDCard[2];
    QVector<InstallationLocation> m_installationLocations;
    ApplicationInstaller *m_ai = nullptr;
    QSignalSpy *m_startedSpy = nullptr;
    QSignalSpy *m_requestingInstallationAcknowledgeSpy = nullptr;
    QSignalSpy *m_blockingUntilInstallationAcknowledgeSpy = nullptr;
    QSignalSpy *m_progressSpy = nullptr;
    QSignalSpy *m_finishedSpy = nullptr;
    QSignalSpy *m_failedSpy = nullptr;
    QSignalSpy *m_packageActivatedSpy = nullptr;
    QSignalSpy *m_packageDeactivatedSpy = nullptr;
};


tst_ApplicationInstaller::tst_ApplicationInstaller(QObject *parent)
    : QObject(parent)
{ }

tst_ApplicationInstaller::~tst_ApplicationInstaller()
{
    if (m_workDir.isValid()) {
        detachLoopbacksAndUnmount(m_sudo, m_workDir.path());
        if (m_sudo)
            m_sudo->removeRecursive(m_workDir.path());
        else
            recursiveOperation(m_workDir.path(), safeRemove);
    }

    delete m_packageDeactivatedSpy;
    delete m_packageActivatedSpy;
    delete m_failedSpy;
    delete m_finishedSpy;
    delete m_progressSpy;
    delete m_blockingUntilInstallationAcknowledgeSpy;
    delete m_requestingInstallationAcknowledgeSpy;
    delete m_startedSpy;

    delete m_ai;
}

void tst_ApplicationInstaller::initTestCase()
{
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    bool verbose = qEnvironmentVariableIsSet("VERBOSE_TEST");
    if (!verbose)
        QLoggingCategory::setFilterRules("am.installer.debug=false");
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(changed by (un)setting $VERBOSE_TEST)";

    spyTimeout *= timeoutFactor();

    QVERIFY(Package::checkCorrectLocale());
    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_sudo = SudoClient::instance();
    QVERIFY(m_sudo);
    m_fakeSudo = m_sudo->isFallbackImplementation();

    // we need a (dummy) ApplicationManager for the installer, since the installer will
    // notify the manager during installations
    QString errorString;
    QVERIFY2(ApplicationManager::createInstance(nullptr, true, &errorString), qPrintable(errorString));

    // create a temporary dir (plus sub-dirs) for everything created by this test run

    QVERIFY(m_workDir.isValid());

    // make sure we have a valid hardware-id
    m_hardwareId = "foobar";

    for (int i = 0; i < PathLocationCount; ++i)
        QVERIFY(QDir().mkdir(pathTo(PathLocation(i))));

    if (!m_fakeSudo) {
        // we have support for loopback devices, so we create two FAT images that simulate SDCards
        // (a lambda would have been perfect here, but QVERIFY/QCOMPARE do not work inside lambdas).

        for (int i = 0; i < 2; ++i) {
            QFile f(pathTo(QString::fromLatin1("sdcard-image-%1").arg(i)));
            QVERIFY2(f.open(QFile::WriteOnly | QFile::Truncate), qPrintable(f.errorString()));
            QVERIFY2(f.resize(3*1024*1024), qPrintable(f.errorString()));
            f.close();
            QCOMPARE(f.size(), 3*1024*1024);

            m_loopbackForSDCard[i] = m_sudo->attachLoopback(f.fileName());
            QVERIFY2(!m_loopbackForSDCard[i].isEmpty(), qPrintable(m_sudo->lastError()));
            QVERIFY2(m_sudo->mkfs(m_loopbackForSDCard[i], "vfat"), qPrintable(m_sudo->lastError()));
            QVERIFY2(m_sudo->mount(m_loopbackForSDCard[i], pathTo(i == 0 ? SDCard0 : SDCard1), false, "vfat"), qPrintable(m_sudo->lastError()));

            // those paths have been hidden due to the mount, so recreate them
            QVERIFY(QDir().mkdir(pathTo(i == 0 ? SDCard0Images : SDCard1Images)));
        }
    }

    // define some installation locations for testing

    QVariantList iloc = QVariantList {
            QVariantMap {
                { "isDefault", true },
                { "id", "internal-0" },
                { "installationPath", pathTo(Internal0) },
                { "documentPath", pathTo(Documents0) }
            },
            QVariantMap {
                { "id", "internal-1" },
                { "installationPath", pathTo(Internal1) },
                { "documentPath", pathTo(Documents1) },
            }
    };
    if (!m_fakeSudo) {
        iloc += QVariantList {
                QVariantMap {
                    { "id", "removable-0" },
                    { "mountPoint", pathTo(SDCard0) },
                    { "installationPath", pathTo(SDCard0, "@HARDWARE-ID@") },
                    { "documentPath", pathTo(Documents0) },
                },
                QVariantMap {
                    { "id", "removable-1" },
                    { "mountPoint", pathTo(SDCard1) },
                    { "installationPath", pathTo(SDCard1, "@HARDWARE-ID@") },
                    { "documentPath", pathTo(Documents1) },
                }
        };
    }

    m_installationLocations = InstallationLocation::parseInstallationLocations(iloc, m_hardwareId);

    QCOMPARE(m_installationLocations.size(), m_fakeSudo ? 2 : 4);
    if (!m_fakeSudo)
        QCOMPARE(m_installationLocations.at(2).installationPath(), pathTo(SDCard0, m_hardwareId));

    // finally, instantiate the ApplicationInstaller and a bunch of signal-spies for its signals

    QString installerError;
    m_ai = ApplicationInstaller::createInstance(m_installationLocations, pathTo(Manifests), pathTo(ImageMounts), m_hardwareId, &installerError);
    QVERIFY2(m_ai, qPrintable(installerError));

    m_startedSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskStarted);
    m_requestingInstallationAcknowledgeSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskRequestingInstallationAcknowledge);
    m_blockingUntilInstallationAcknowledgeSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskBlockingUntilInstallationAcknowledge);
    m_progressSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskProgressChanged);
    m_finishedSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskFinished);
    m_failedSpy = new QSignalSpy(m_ai, &ApplicationInstaller::taskFailed);
    m_packageActivatedSpy = new QSignalSpy(m_ai, &ApplicationInstaller::packageActivated);
    m_packageDeactivatedSpy = new QSignalSpy(m_ai, &ApplicationInstaller::packageDeactivated);

    // crypto stuff - we need to load the root CA and developer CA certificates

    QFile devcaFile(AM_TESTDATA_DIR "certificates/devca.crt");
    QFile storecaFile(AM_TESTDATA_DIR "certificates/store.crt");
    QFile caFile(AM_TESTDATA_DIR "certificates/ca.crt");
    QVERIFY2(devcaFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));
    QVERIFY2(storecaFile.open(QIODevice::ReadOnly), qPrintable(storecaFile.errorString()));
    QVERIFY2(caFile.open(QIODevice::ReadOnly), qPrintable(caFile.errorString()));

    QList<QByteArray> chainOfTrust;
    chainOfTrust << devcaFile.readAll() << caFile.readAll();
    QVERIFY(!chainOfTrust.at(0).isEmpty());
    QVERIFY(!chainOfTrust.at(1).isEmpty());
    m_ai->setCACertificates(chainOfTrust);

    // we do not require valid store signatures for this test run

    m_ai->setDevelopmentMode(true);

    // make sure we have a valid runtime available. The important part is
    // that we have a runtime called "native" - the functionality does not matter.
    RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager(qSL("native")));
}

void tst_ApplicationInstaller::cleanupTestCase()
{
    if (m_ai && m_ai->isPackageActivated("com.pelagicore.test")) {
        QVERIFY(m_ai->deactivatePackage("com.pelagicore.test"));
        QTRY_VERIFY(checkPackageDeactivation("com.pelagicore.test"));
    }
    // the real cleanup happens in ~tst_ApplicationInstaller, since we also need
    // to call this cleanup from the crash handler
}

void tst_ApplicationInstaller::init()
{
    // start with a fresh App1 dir on each test run

    if (!QDir(pathTo(Internal0)).exists())
        QVERIFY(QDir().mkdir(pathTo(Internal0)));
}

void tst_ApplicationInstaller::cleanup()
{
    // this helps with reducing the amount of cleanup work required
    // at the end of each test

    try {
        m_ai->cleanupBrokenInstallations();
    } catch (const Exception &e) {
        QFAIL(e.what());
    }

    clearSignalSpies();
    recursiveOperation(pathTo(Internal0), safeRemove);
}

void tst_ApplicationInstaller::installationLocations()
{
    QCOMPARE(InstallationLocation().type(), InstallationLocation::Invalid);

    for (int i = InstallationLocation::Removable; i >= InstallationLocation::Invalid; --i) {
        QString s = InstallationLocation::typeToString((InstallationLocation::Type) i);
        QVERIFY(!s.isEmpty());
        QVERIFY(InstallationLocation::typeFromString(s) == i);
    }

    const QVector<InstallationLocation> loclist = m_ai->installationLocations();

    QCOMPARE(loclist.size(), m_installationLocations.size());
    for (const InstallationLocation &loc : loclist) {
        QVERIFY(m_installationLocations.contains(loc));

        QCOMPARE(loc.id(), InstallationLocation::typeToString(loc.type()) + "-" + QString::number(loc.index()));
    }

    QVector<InstallationLocation> locationList = InstallationLocation::parseInstallationLocations(QVariantList {
        QVariantMap {
            { "id", "internal-0" },
            { "installationPath", QDir::tempPath() },
            { "documentPath", QDir::tempPath() },
            { "isDefault", true }
        },
    }, m_hardwareId);
    QCOMPARE(locationList.size(), 1);
    InstallationLocation &tmp = locationList.first();
    QVariantMap map = tmp.toVariantMap();
    QVERIFY(!map.isEmpty());

    QCOMPARE(map.value("id").toString(), tmp.id());
    QCOMPARE(map.value("type").toString(), InstallationLocation::typeToString(tmp.type()));
    QCOMPARE(map.value("index").toInt(), tmp.index());
    QCOMPARE(map.value("installationPath").toString(), tmp.installationPath());
    QCOMPARE(map.value("documentPath").toString(), tmp.documentPath());
    QCOMPARE(map.value("isDefault").toBool(), tmp.isDefault());
    QCOMPARE(map.value("isRemovable").toBool(), tmp.isRemovable());
    QCOMPARE(map.value("isMounted").toBool(), tmp.isMounted());
    QVERIFY(map.value("installationDeviceSize").toLongLong() > 0);
    QVERIFY(map.value("installationDeviceFree").toLongLong() > 0);
    QVERIFY(map.value("documentDeviceSize").toLongLong() > 0);
    QVERIFY(map.value("documentDeviceFree").toLongLong() > 0);
}


void tst_ApplicationInstaller::packageActivation()
{
#ifndef Q_OS_LINUX
    QSKIP("Cannot run SD-Card tests on non-Linux systems");
#else
    if (m_fakeSudo)
        QSKIP("Cannot run SD-Card tests without root privileges");
#endif


    QVERIFY(!m_ai->activatePackage(QString()));
    QVERIFY(!m_ai->activatePackage("does.not.exist"));
    QVERIFY(!m_ai->deactivatePackage(QString()));
    QVERIFY(!m_ai->deactivatePackage("does.not.exist"));

    // install package to sdcard

    QString taskId = m_ai->startPackageInstallation("removable-0", QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/test-dev-signed.appkg"));
    QVERIFY(!taskId.isEmpty());
    m_ai->acknowledgePackageInstallation(taskId);

    // check received signals

    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    clearSignalSpies();

    // activate the package

    QString name = "com.pelagicore.test";

    QVERIFY(m_ai->doesPackageNeedActivation(name));
    QVERIFY(!m_ai->isPackageActivated(name));
    QVERIFY(m_ai->activatePackage(name));

    QVERIFY(checkPackageActivation(name, 3000));
    QVERIFY(m_ai->isPackageActivated(name));
    QVERIFY(!m_ai->activatePackage(name));
    clearSignalSpies();

    // check actual file-system mount

    QString mountPoint = QDir(pathTo(ImageMounts, name)).canonicalPath();
    QVERIFY(mountedDirectories().value(mountPoint).startsWith("/dev/loop"));
    QFile testFile(mountPoint + "/test");
    QVERIFY(testFile.exists());
    QVERIFY(!testFile.open(QFile::ReadWrite));

    // deactivate the package again

    QVERIFY(m_ai->deactivatePackage(name));
    QVERIFY(checkPackageDeactivation(name, 3000));
    QVERIFY(!m_ai->isPackageActivated(name));
    QVERIFY(!m_ai->deactivatePackage(name));
    clearSignalSpies();

    // check that the actual file-system mount is gone

    QVERIFY2(!mountedDirectories().contains(mountPoint),
             qPrintable(mountedDirectories().value(mountPoint)));
    QVERIFY(!testFile.exists());

    // remove package again

    taskId = m_ai->removePackage(name, false);
    QVERIFY(!taskId.isEmpty());
    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
}


void tst_ApplicationInstaller::packageInstallation_data()
{
    QTest::addColumn<QString>("packageName");
    QTest::addColumn<QString>("installationLocationId");
    QTest::addColumn<QString>("updatePackageName");
    QTest::addColumn<QString>("updateInstallationLocationId");
    QTest::addColumn<bool>("devSigned");
    QTest::addColumn<bool>("storeSigned");
    QTest::addColumn<bool>("expectedSuccess");
    QTest::addColumn<bool>("updateExpectedSuccess");
    QTest::addColumn<QVariantMap>("extraMetaData");
    QTest::addColumn<QString>("errorString"); // start with ~ to create a RegExp

    QVariantMap nomd { }; // no meta-data
    QVariantMap extramd = QVariantMap {
        { "extra", QVariantMap {
            { "array", QVariantList { 1, 2 } },
            { "foo", "bar" },
            { "foo2","bar2" },
            { "key", "value" } } },
        { "extraSigned", QVariantMap {
            { "sfoo", "sbar" },
            { "sfoo2", "sbar2" },
            { "signed-key", "signed-value" },
            { "signed-object", QVariantMap { { "k1", "v1" }, { "k2", "v2" } } }
        } }
    };

    QTest::newRow("normal") \
            << "test.appkg" << "internal-0" << "test-update.appkg" << "internal-0"
            << false << false << true << true << nomd<< "";
    QTest::newRow("no-dev-signed") \
            << "test.appkg" << "internal-0" << "" << ""
            << true << false << false << false << nomd << "cannot install unsigned packages";
    QTest::newRow("dev-signed") \
            << "test-dev-signed.appkg" << "internal-0" << "test-update-dev-signed.appkg" << "internal-0"
            << true << false << true << true << nomd << "";
    QTest::newRow("no-store-signed") \
            << "test.appkg" << "internal-0" << "" << ""
            << false << true << false << false << nomd << "cannot install unsigned packages";
    QTest::newRow("no-store-but-dev-signed") \
            << "test-dev-signed.appkg" << "internal-0" << "" << ""
            << false << true << false << false << nomd << "cannot install development packages on consumer devices";
    QTest::newRow("store-signed") \
            << "test-store-signed.appkg" << "internal-0" << "" << ""
            << false << true << true << false << nomd << "";
    QTest::newRow("extra-metadata") \
            << "test-extra.appkg" << "internal-0" << "" << ""
            << false << false << true << false << extramd << "";
    QTest::newRow("extra-metadata-dev-signed") \
            << "test-extra-dev-signed.appkg" << "internal-0" << "" << ""
            << true << false << true << false << extramd << "";
    QTest::newRow("update-to-different-location") \
            << "test.appkg" << "internal-0" << "test-update.appkg" << "internal-1"
            << false << false << true << false << nomd << "the application com.pelagicore.test cannot be installed to internal-1, since it is already installed to internal-0";
    QTest::newRow("invalid-location") \
            << "test.appkg" << "internal-42" << "" << ""
            << false << false << false << false << nomd << "invalid installation location";
    QTest::newRow("invalid-file-order") \
            << "test-invalid-file-order.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "The application icon (as stated in info.yaml) must be the second file in the package. Expected 'icon.png', got 'test'";
    QTest::newRow("invalid-header-format") \
            << "test-invalid-header-formatversion.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "metadata has an invalid format specification: wrong formatVersion header: expected 1, got 2";
    QTest::newRow("invalid-header-diskspaceused") \
            << "test-invalid-header-diskspaceused.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "metadata has an invalid diskSpaceUsed field (0)";
    QTest::newRow("invalid-header-id") \
            << "test-invalid-header-id.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "metadata has an invalid applicationId field (:invalid)";
    QTest::newRow("non-matching-header-id") \
            << "test-non-matching-header-id.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "the application identifiers in --PACKAGE-HEADER--' and info.yaml do not match";
    QTest::newRow("tampered-extra-signed-header") \
            << "test-tampered-extra-signed-header.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "~package digest mismatch.*";
    QTest::newRow("invalid-info.yaml") \
            << "test-invalid-info.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "~.*YAML parse error at line \\d+, column \\d+: did not find expected key";
    QTest::newRow("invalid-info.yaml-id") \
            << "test-invalid-info-id.appkg" << "internal-0" << "" << ""
            << false << false << false << false << nomd << "~.*the identifier \\(:invalid\\) is not a valid application-id: must consist of printable ASCII characters only, except any of .*";
    QTest::newRow("invalid-footer-signature") \
            << "test-invalid-footer-signature.appkg" << "internal-0" << "" << ""
            << true << false << false << false << nomd << "could not verify the package's developer signature";

    if (!m_fakeSudo) {
        QTest::newRow("sdcard") \
                << "test.appkg" << "removable-0" << "test-update.appkg" << "removable-0"
                << false << false << true << true << nomd << "";
        QTest::newRow("sdcard-dev-signed") \
                << "test-dev-signed.appkg" << "removable-0" << "test-update-dev-signed.appkg" << "removable-0"
                << true << false << true << true << nomd << "";
        QTest::newRow("sdcard-no-space") \
                << "bigtest-dev-signed.appkg" << "removable-0" << "" << ""
                << true << false << false << false << nomd << "~not enough storage space left on removable-0: [0-9.]+ MB available, but [0-9.]+ MB needed";
    }
}

// this test function is a bit of a kitchen sink, but the basic boiler plate
// code of testing the results of an installation is the biggest part and it
// is always the same.
void tst_ApplicationInstaller::packageInstallation()
{
    QFETCH(QString, packageName);
    QFETCH(QString, installationLocationId);
    QFETCH(QString, updatePackageName);
    QFETCH(QString, updateInstallationLocationId);
    QFETCH(bool, devSigned);
    QFETCH(bool, storeSigned);
    QFETCH(bool, expectedSuccess);
    QFETCH(bool, updateExpectedSuccess);
    QFETCH(QVariantMap, extraMetaData);
    QFETCH(QString, errorString);

#if !defined(Q_OS_LINUX)
    if (installationLocationId.startsWith("removable-"))
        QSKIP("no removable installation locations on this platform");
#else
    if (m_fakeSudo)
        QSKIP("removable installation locations cannot be tested without root privileges");
#endif

    AllowInstallations allow(storeSigned ? AllowInstallations::RequireStoreSigned
                                         : (devSigned ? AllowInstallations::RequireDevSigned
                                                      : AllowInstallations::AllowUnsinged));

    int lastPass = (updatePackageName.isEmpty() ? 1 : 2);
    // pass 1 is the installation / pass 2 is the update (if needed)
    for (int pass = 1; pass <= lastPass; ++pass) {
        const InstallationLocation &il = m_ai->installationLocationFromId(pass == 1 ? installationLocationId : updateInstallationLocationId);

        // this makes the results a bit ugly to look at, but it helps with debugging a lot
        if (pass > 1)
            qInfo("Pass %d", pass);

        // install (or update) the package

        QUrl url = QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/" + (pass == 1 ? packageName : updatePackageName));
        QString taskId = m_ai->startPackageInstallation(il.id(), url);
        QVERIFY(!taskId.isEmpty());
        m_ai->acknowledgePackageInstallation(taskId);

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

            QVERIFY(QFile::exists(pathTo(Manifests, "com.pelagicore.test/installation-report.yaml")));
            QVERIFY(QDir(pathTo(il.documentPath() + "/com.pelagicore.test")).exists());

            // if we installed to an SDCard, we need to mount the generate image first
            // in order to check its contents

            QString loopbackDevice;
            QString fileCheckPath;
            if (il.isRemovable()) {
                QString imgPath = il.installationPath() + "/com.pelagicore.test.appimg";

                QVERIFY(QFile::exists(imgPath));

                loopbackDevice = m_sudo->attachLoopback(imgPath, true);
                QVERIFY2(!loopbackDevice.isEmpty(), qPrintable(m_sudo->lastError()));
                QVERIFY2(m_sudo->mount(loopbackDevice, pathTo(TemporaryMount), true), qPrintable(m_sudo->lastError()));
                fileCheckPath = pathTo(TemporaryMount);
            } else {
                fileCheckPath = il.installationPath() + "/com.pelagicore.test";
            }

            // now check the installed files

            QStringList files = QDir(fileCheckPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
            files.removeAll("lost+found"); // no way to get rid of this completely
            files.sort();
            QVERIFY2(files == QStringList({ "icon.png", "info.yaml", "test", QString::fromUtf8("t\xc3\xa4st") }), qPrintable(files.join(", ")));

            QFile f(fileCheckPath + "/test");
            QVERIFY(f.open(QFile::ReadOnly));
            QCOMPARE(f.readAll(), QByteArray(pass == 1 ? "test\n" : "test update\n"));
            f.close();

            // remove loopback and unmount in case of an SDCard installation

            if (il.isRemovable()) {
                QVERIFY2(m_sudo->unmount(pathTo(TemporaryMount)), qPrintable(m_sudo->lastError()));
                QVERIFY2(m_sudo->detachLoopback(loopbackDevice), qPrintable(m_sudo->lastError()));
            }

            // check metadata
            QCOMPARE(m_requestingInstallationAcknowledgeSpy->count(), 1);
            QVariantMap extra = m_requestingInstallationAcknowledgeSpy->first()[2].toMap();
            QVariantMap extraSigned = m_requestingInstallationAcknowledgeSpy->first()[3].toMap();
            if (extraMetaData.value("extra").toMap() != extra) {
                qDebug() << "Actual: " << extra;
                qDebug() << "Expected: " << extraMetaData.value("extra").toMap();
                QVERIFY(extraMetaData == extra);
            }
            if (extraMetaData.value("extraSigned").toMap() != extraSigned) {
                qDebug() << "Actual: " << extraSigned;
                qDebug() << "Expected: " << extraMetaData.value("extraSigned").toMap();
                QVERIFY(extraMetaData == extraSigned);
            }

            // check if the meta-data was saved to the installation report correctly
            QVERIFY2(m_ai->installedApplicationExtraMetaData("com.pelagicore.test") == extra,
                     "Extra meta-data was not correctly saved to installation report");
            QVERIFY2(m_ai->installedApplicationExtraSignedMetaData("com.pelagicore.test") == extraSigned,
                     "Extra signed meta-data was not correctly saved to installation report");
        }
        if (pass == lastPass && expectedSuccess) {
            // remove package again

            clearSignalSpies();
            taskId = m_ai->removePackage("com.pelagicore.test", false);
            QVERIFY(!taskId.isEmpty());

            // check signals

            QVERIFY(m_finishedSpy->wait(spyTimeout));
            QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        }
        clearSignalSpies();
    }
    // check that all files are gone

    for (PathLocation pl: { Manifests, Internal0, Internal1, Documents0, Documents1, SDCard0Images, SDCard1Images }) {
        QStringList entries = QDir(pathTo(pl)).entryList({ "com.pelagicore.test*" });
        QVERIFY2(entries.isEmpty(), qPrintable(pathTo(pl) + ": " + entries.join(", ")));
    }
}


void tst_ApplicationInstaller::removeAppOnMissingSDCard()
{
#ifndef Q_OS_LINUX
    QSKIP("Cannot run SD-Card tests on non-Linux systems");
#else
    if (m_fakeSudo)
        QSKIP("Cannot run SD-Card tests without root privileges");
#endif

    // install package to sdcard

    QString taskId = m_ai->startPackageInstallation("removable-0", QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/test-dev-signed.appkg"));
    QVERIFY(!taskId.isEmpty());
    m_ai->acknowledgePackageInstallation(taskId);

    // check received signals

    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    clearSignalSpies();

    // check files

    QVERIFY(QFile::exists(pathTo(Manifests, "com.pelagicore.test/installation-report.yaml")));
    QVERIFY(QDir(pathTo(Documents0, "com.pelagicore.test")).exists());
    QVERIFY(QFile::exists(pathTo(SDCard0Images, "com.pelagicore.test.appimg")));

    // simulate removal of SD-Card

    QVERIFY2(m_sudo->unmount(pathTo(SDCard0), true), qPrintable(m_sudo->lastError()));

    // try to remove the package

    taskId = m_ai->removePackage("com.pelagicore.test", false);
    QVERIFY(!taskId.isEmpty());
    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
    QCOMPARE(m_failedSpy->first()[2].toString(), QString::fromLatin1("cannot delete application com.pelagicore.test without the removable medium it was installed on"));
    clearSignalSpies();

    QVERIFY(QFile::exists(pathTo(Manifests, "com.pelagicore.test/installation-report.yaml")));
    QVERIFY(QDir(pathTo(Documents0, "com.pelagicore.test")).exists());

    // now force remove it

    taskId = m_ai->removePackage("com.pelagicore.test", false, true /*force*/);
    QVERIFY(!taskId.isEmpty());
    QVERIFY(m_finishedSpy->wait(spyTimeout));
    QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    clearSignalSpies();

    // check if files that are not on SD-Card are gone

    QVERIFY(!QDir(pathTo(Manifests, "com.pelagicore.test")).exists());
    QVERIFY(!QDir(pathTo(Documents0, "com.pelagicore.test")).exists());

    // "re-insert" the SD-Card

    QVERIFY(m_sudo->mount(m_loopbackForSDCard[0], pathTo(SDCard0), false, "vfat"));
    QVERIFY(QFile::exists(pathTo(SDCard0Images, "com.pelagicore.test.appimg")));
    QVERIFY(QFile::remove(pathTo(SDCard0Images, "com.pelagicore.test.appimg")));

    //TODO: now that AI::cleanupBrokenInstallations() works, call it here and check the results
}


Q_DECLARE_METATYPE(std::function<bool()>)
typedef QMultiMap<QString, std::function<bool()>> FunctionMap;
Q_DECLARE_METATYPE(FunctionMap)

void tst_ApplicationInstaller::simulateErrorConditions_data()
{
     QTest::addColumn<QString>("installationLocation");
     QTest::addColumn<bool>("testUpdate");
     QTest::addColumn<QString>("errorString");
     QTest::addColumn<FunctionMap>("functions");

#ifdef Q_OS_LINUX
     QTest::newRow("manifests-dir-read-only") \
             << "internal-0" << false << "~could not create manifest sub-directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(pathTo(Manifests).toLocal8Bit(), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(pathTo(Manifests).toLocal8Bit(), 0777) == 0; } } };

     QTest::newRow("applications-dir-read-only") \
             << "internal-0" << false << "~could not create installation directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(pathTo(Internal0).toLocal8Bit(), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(pathTo(Internal0).toLocal8Bit(), 0777) == 0; } } };

     QTest::newRow("documents-dir-read-only") \
             << "internal-0" << false << "~could not create the document directory .*" \
             << FunctionMap { { "before-start", [this]() { return chmod(pathTo(Documents0).toLocal8Bit(), 0000) == 0; } },
                              { "after-failed", [this]() { return chmod(pathTo(Documents0).toLocal8Bit(), 0777) == 0; } } };

     QTest::newRow("image-still-activated") \
             << "removable-0" << true << "~existing application image .* is still mounted" \
             << FunctionMap { { "before-start", [this]() { return m_ai->activatePackage("com.pelagicore.test")
                                                                      && checkPackageActivation("com.pelagicore.test", 3000); } },
                              { "after-failed", [this]() { return m_ai->deactivatePackage("com.pelagicore.test")
                                                                      && checkPackageDeactivation("com.pelagicore.test", 3000); } } };

     QTest::newRow("sdcard-not-mounted") \
             << "removable-0" << false << "installation medium removable-0 is not mounted" \
             << FunctionMap { { "before-start", [this]() { return m_sudo->unmount(pathTo(SDCard0), true); } },
                              { "after-failed", [this]() { return m_sudo->mount(m_loopbackForSDCard[0], pathTo(SDCard0), false, "vfat"); } } };

     QTest::newRow("sdcard-unmounted-while-installing") \
             << "removable-0" << false << "removable medium removable-0 is not mounted" \
             << FunctionMap { { "after-start", [this]() { return m_requestingInstallationAcknowledgeSpy->wait(spyTimeout)
                                                                     && m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout)
                                                                     && m_sudo->unmount(pathTo(SDCard0), true); } },
                              { "after-failed", [this]() { return m_sudo->mount(m_loopbackForSDCard[0], pathTo(SDCard0), false, "vfat"); } } };

#endif
}

void tst_ApplicationInstaller::simulateErrorConditions()
{
#ifndef Q_OS_LINUX
    QSKIP("Cannot run SD-Card tests on non-Linux systems");
#else
    if (m_fakeSudo)
        QSKIP("Cannot run SD-Card tests without root privileges");
#endif

    QFETCH(QString, installationLocation);
    QFETCH(bool, testUpdate);
    QFETCH(QString, errorString);
    QFETCH(FunctionMap, functions);

    QString taskId;

    if (testUpdate) {
        // the check will run when updating a package, so we need to install it first

        taskId = m_ai->startPackageInstallation(installationLocation, QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/test-dev-signed.appkg"));
        QVERIFY(!taskId.isEmpty());
        m_ai->acknowledgePackageInstallation(taskId);
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        clearSignalSpies();
    }

    foreach (const auto &f, functions.values("before-start"))
        QVERIFY(f());

    taskId = m_ai->startPackageInstallation(installationLocation, QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/test-dev-signed.appkg"));

    foreach (const auto &f, functions.values("after-start"))
        QVERIFY(f());

    m_ai->acknowledgePackageInstallation(taskId);

    QVERIFY(m_failedSpy->wait(spyTimeout));
    QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
    AM_CHECK_ERRORSTRING(m_failedSpy->first()[2].toString(), errorString);
    clearSignalSpies();

    foreach (const auto &f, functions.values("after-failed"))
        QVERIFY(f());

    if (testUpdate) {
        taskId = m_ai->removePackage("com.pelagicore.test", false);

        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    }
}


void tst_ApplicationInstaller::cancelPackageInstallation_data()
{
    QTest::addColumn<bool>("expectedResult");

    // please note that the data tag names are used in the actual test function below!

    QTest::newRow("before-started-signal") << true;
    QTest::newRow("after-started-signal") << true;
    QTest::newRow("after-blocking-until-installation-acknowledge-signal") << true;
    QTest::newRow("after-finished-signal") << false;
}

void tst_ApplicationInstaller::cancelPackageInstallation()
{
    QFETCH(bool, expectedResult);

    QString taskId = m_ai->startPackageInstallation("internal-0", QUrl::fromLocalFile(AM_TESTDATA_DIR "packages/test-dev-signed.appkg"));
    QVERIFY(!taskId.isEmpty());

    if (isDataTag("before-started-signal")) {
        QCOMPARE(m_ai->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-started-signal")) {
        QVERIFY(m_startedSpy->wait(spyTimeout));
        QCOMPARE(m_startedSpy->first()[0].toString(), taskId);
        QCOMPARE(m_ai->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-blocking-until-installation-acknowledge-signal")) {
        QVERIFY(m_blockingUntilInstallationAcknowledgeSpy->wait(spyTimeout));
        QCOMPARE(m_blockingUntilInstallationAcknowledgeSpy->first()[0].toString(), taskId);
        QCOMPARE(m_ai->cancelTask(taskId), expectedResult);
    } else if (isDataTag("after-finished-signal")) {
        m_ai->acknowledgePackageInstallation(taskId);
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
        QCOMPARE(m_ai->cancelTask(taskId), expectedResult);
    }

    if (expectedResult) {
        if (!m_startedSpy->isEmpty()) {
            QVERIFY(m_failedSpy->wait(spyTimeout));
            QCOMPARE(m_failedSpy->first()[0].toString(), taskId);
            QCOMPARE(m_failedSpy->first()[1].toInt(), int(Error::Canceled));
        }
    } else {
        clearSignalSpies();

        taskId = m_ai->removePackage("com.pelagicore.test", false);
        QVERIFY(!taskId.isEmpty());
        QVERIFY(m_finishedSpy->wait(spyTimeout));
        QCOMPARE(m_finishedSpy->first()[0].toString(), taskId);
    }
}


static tst_ApplicationInstaller *tstApplicationInstaller = nullptr;

int main(int argc, char **argv)
{
    Package::ensureCorrectLocale();

    try {
        Sudo::forkServer(Sudo::DropPrivilegesPermanently);
        startedSudoServer = true;
    } catch (...) { }

    QCoreApplication a(argc, argv);
    tstApplicationInstaller = new tst_ApplicationInstaller(&a);

#ifdef Q_OS_LINUX
    auto crashHandler = [](int sigNum) -> void {
        // we are doing very unsafe things from a within a signal handler, but
        // we've crashed anyway at this point and the alternative is that we are
        // leaking mounts and attached loopback devices.

        tstApplicationInstaller->~tst_ApplicationInstaller();

        if (sigNum != -1)
            exit(1);
    };

    signal(SIGABRT, crashHandler);
    signal(SIGSEGV, crashHandler);
    signal(SIGINT, crashHandler);
#endif // Q_OS_LINUX

    return QTest::qExec(tstApplicationInstaller, argc, argv);
}

void tst_ApplicationInstaller::validateDnsName_data()
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

void tst_ApplicationInstaller::validateDnsName()
{
    QFETCH(QString, dnsName);
    QFETCH(int, minParts);
    QFETCH(bool, valid);

    QString errorString;
    bool result = m_ai->validateDnsName(dnsName, minParts);

    QVERIFY2(valid == result, qPrintable(errorString));
}

void tst_ApplicationInstaller::compareVersions_data()
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

void tst_ApplicationInstaller::compareVersions()
{
    QFETCH(QString, version1);
    QFETCH(QString, version2);
    QFETCH(int, result);

    int cmp = m_ai->compareVersions(version1, version2);
    QCOMPARE(cmp, result);

    if (result) {
        cmp = m_ai->compareVersions(version2, version1);
        QCOMPARE(cmp, -result);
    }
}

#include "tst_applicationinstaller.moc"
