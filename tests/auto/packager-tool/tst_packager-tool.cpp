// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtTest>
#include <QCoreApplication>

#include "global.h"
#include "applicationmanager.h"
#include "application.h"
#include "qtyaml.h"
#include "exception.h"
#include "packagedatabase.h"
#include "packagemanager.h"
#include "packagingjob.h"
#include "qmlinprocruntime.h"
#include "runtimefactory.h"
#include "utilities.h"
#include "sudo.h"

#include "../error-checking.h"
#include "../devmode.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

static int spyTimeout = 5000; // shorthand for specifying QSignalSpy timeouts

class tst_PackagerTool : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void test();
    void revoked();
    void expired();
    void developerSignCertBinding_data();
    void developerSignCertBinding();
    void brokenMetadata_data();
    void brokenMetadata();
    void iconFileName();

private:
    QString pathTo(const char *file)
    {
        return QDir(m_workDir.path()).absoluteFilePath(QString::fromLatin1(file));
    }

    bool createInfoYaml(QTemporaryDir &tmp, const std::function<void(QVariantMap &)> &manipulate = { });
    bool createIconPng(QTemporaryDir &tmp, const QString &prefix = { });
    bool createCode(QTemporaryDir &tmp);
    void createDummyFile(QTemporaryDir &tmp, const QString &fileName, const char *data);

    void installPackage(const QString &filePath, bool allowUnsigned = false);
    void failToInstallPackage(const QString &filePath, const QString &expectedError);

    PackageManager *m_pm = nullptr;
    QTemporaryDir m_workDir;

    QString m_devPassword;
    QString m_devCertificate;
    QString m_devNarrowCertificate;
    QString m_devExpiredCertificate;
    QString m_devRevokedCertificate;
    QString m_storePassword;
    QString m_storeCertificate;
    QStringList m_commonCaFiles;
    QStringList m_devCaFiles;
    QStringList m_storeCaFiles;
    QStringList m_crlFiles;
    QString m_hardwareId;
};

void tst_PackagerTool::initTestCase()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#endif
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    Sudo::fallbackServer();

    spyTimeout *= timeoutFactor();

    QVERIFY(m_workDir.isValid());
    QVERIFY(QDir::root().mkpath(pathTo("internal-0")));
    QVERIFY(QDir::root().mkpath(pathTo("documents-0")));

    // Route the sudo helper's trusted-file tree (installation-reports/) into the temp dir so
    // installs and removes don't leak files into the user's XDG state location.
#if defined(QT_BUILD_INTERNAL)
    SudoClient::instance()->setTestRootPathPrefix(m_workDir.path() + u'/');
#endif
    SudoClient::instance()->setInstanceId(QString());

    m_hardwareId = u"foobar"_s;

    PackageDatabase *pdb = new PackageDatabase({}, pathTo("internal-0"));
    try {
        m_pm = PackageManager::createInstance(pdb, pathTo("documents-0"));
        m_pm->setHardwareId(m_hardwareId);
        m_pm->enableInstaller();
        m_pm->registerPackages();
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }

    QVERIFY(ApplicationManager::createInstance(true));

    m_commonCaFiles << AM_TESTDATA_DIR u"certificates/root-ca/root-ca.crt"_s;
    m_devCaFiles    << AM_TESTDATA_DIR u"certificates/dev-ca/dev-ca.crt"_s;
    m_storeCaFiles  << AM_TESTDATA_DIR u"certificates/store-ca/store-ca.crt"_s;

    m_crlFiles << AM_TESTDATA_DIR u"certificates/root-ca/root-ca.crl"_s
               << AM_TESTDATA_DIR u"certificates/dev-ca/dev-ca.crl"_s
               << AM_TESTDATA_DIR u"certificates/store-ca/store-ca.crl"_s;

    QList<CaCertificate> caCerts = {
        CaCertificate(m_commonCaFiles[0], CaCertificate::Scope::Common, CaCertificate::Role::Root),
        CaCertificate(m_devCaFiles[0], CaCertificate::Scope::Developer, CaCertificate::Role::Issuer),
        CaCertificate(m_storeCaFiles[0], CaCertificate::Scope::Store, CaCertificate::Role::Issuer)
    };

    QVERIFY_THROWS_NO_EXCEPTION(m_pm->loadCertificates(caCerts, m_crlFiles));
    m_devPassword   = u"password"_s;
    m_storePassword = u"password"_s;

    m_devCertificate        = AM_TESTDATA_DIR u"certificates/dev-certs/dev-1.p12"_s;
    m_devNarrowCertificate  = AM_TESTDATA_DIR u"certificates/dev-certs/dev-narrow.p12"_s;
    m_devRevokedCertificate = AM_TESTDATA_DIR u"certificates/dev-certs/dev-revoked.p12"_s;
    m_devExpiredCertificate = AM_TESTDATA_DIR u"certificates/dev-certs/dev-expired.p12"_s;
    m_storeCertificate      = AM_TESTDATA_DIR u"certificates/store-certs/store.p12"_s;

    RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager(u"qml"_s));
}

void tst_PackagerTool::cleanupTestCase()
{
    recursiveOperation(pathTo("internal-0"), safeRemove);
    recursiveOperation(pathTo("documents-0"), safeRemove);

    QDir dir(m_workDir.path());
    QStringList fileNames = dir.entryList(QDir::Files);
    for (const auto &fileName : fileNames)
        dir.remove(fileName);
}

// exceptions are nice -- just not for unit testing :)
static bool packagerCheck(PackagingJob *p, QString &errorString)
{
    bool result = false;
    try {
        p->execute();
        errorString.clear();
        result = (p->resultCode() == 0);
        if (!result)
            errorString = p->output();
    } catch (const Exception &e) { \
        errorString = e.errorString();
    }
    delete p;
    return result;
}

void tst_PackagerTool::test()
{
    QTemporaryDir tmp;
    QString errorString;

    // no valid destination
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), pathTo("test.ampkg")), errorString));
    QVERIFY2(errorString.contains(u"is not a directory"), qPrintable(errorString));

    // no valid info.yaml
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"Cannot open for reading"), qPrintable(errorString));

    // add an info.yaml file
    createInfoYaml(tmp);

    // no icon
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"missing the file referenced by the 'icon' field"), qPrintable(errorString));

    // add an icon
    createIconPng(tmp);

    // missing intent icon
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"missing the file referenced by the 'icon' field for intent 'test-intent'"), qPrintable(errorString));

    // add an icon for the intent
    createIconPng(tmp, u"intent-"_s);

    // no valid code
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"missing the file referenced by the 'code' field"), qPrintable(errorString));

    // add a code file
    createCode(tmp);

    // missing app icon
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"missing the file referenced by the 'icon' field for application 'test-app'"), qPrintable(errorString));

    // add an icon for the app
    createIconPng(tmp, u"app-"_s);

    // invalid destination
    QVERIFY(!packagerCheck(PackagingJob::create(tmp.path(), tmp.path()), errorString));
    QVERIFY2(errorString.contains(u"could not create package file"), qPrintable(errorString));

    // now everything is correct - try again
    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), errorString), qPrintable(errorString));

    // invalid source package
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("no-such-file"),
                               pathTo("test.dev-signed.ampkg"),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains(u"does not exist"), qPrintable(errorString));

    // invalid destination package
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("test.ampkg"),
                               pathTo("."),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains(u"could not create package file"), qPrintable(errorString));

    // invalid dev key
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("test.ampkg"),
                               pathTo("test.dev-signed.ampkg"),
                               m_devCertificate,
                               u"wrong-password"_s), errorString));
    QVERIFY2(errorString.contains(u"could not create signature"), qPrintable(errorString));

    // store key as dev key
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("test.ampkg"),
                               pathTo("test.dev-signed.ampkg"),
                               m_storeCertificate,
                               m_storePassword), errorString));
    QVERIFY2(errorString.contains(u"could not create signature"), qPrintable(errorString));

    // dev sign
    QVERIFY2(packagerCheck(PackagingJob::developerSign(
                               pathTo("test.ampkg"),
                               pathTo("test.dev-signed.ampkg"),
                               m_devCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    // invalid store key
    QVERIFY(!packagerCheck(PackagingJob::storeSign(
                               pathTo("test.dev-signed.ampkg"),
                               pathTo("test.store-signed.ampkg"),
                               m_storeCertificate,
                               u"wrong-password"_s,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"could not create signature"), qPrintable(errorString));

    // dev key as store key
    QVERIFY(!packagerCheck(PackagingJob::storeSign(
                               pathTo("test.dev-signed.ampkg"),
                               pathTo("test.store-signed.ampkg"),
                               m_devCertificate,
                               m_devPassword,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"could not create signature"), qPrintable(errorString));

    // store sign
    QVERIFY2(packagerCheck(PackagingJob::storeSign(
                               pathTo("test.dev-signed.ampkg"),
                               pathTo("test.store-signed.ampkg"),
                               m_storeCertificate,
                               m_storePassword,
                               m_hardwareId), errorString), qPrintable(errorString));

    // dev verify without any CA
    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               { }, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // dev verify without root CA
    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               m_devCaFiles, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // dev verify without dev CA
    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               m_commonCaFiles, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // dev verify with store CA
    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               m_commonCaFiles + m_storeCaFiles, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // dev verify
    QVERIFY2(packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               m_commonCaFiles + m_devCaFiles, m_crlFiles), errorString), qPrintable(errorString));

    // store verify without any CA
    QVERIFY(!packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               { }, m_crlFiles,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // store verify without root CA
    QVERIFY(!packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               m_storeCaFiles, m_crlFiles,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // store verify without store CA
    QVERIFY(!packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               m_commonCaFiles, m_crlFiles,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // store verify with dev CA
    QVERIFY(!packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               m_commonCaFiles + m_devCaFiles, m_crlFiles,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"Failed to verify signature"), qPrintable(errorString));

    // store verify
    QVERIFY2(packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               m_commonCaFiles + m_storeCaFiles, m_crlFiles,
                               m_hardwareId), errorString), qPrintable(errorString));

    // now that we have it, see if the package actually installs correctly

    installPackage(pathTo("test.dev-signed.ampkg"));

    QDir checkDir(pathTo("internal-0"));
    QVERIFY(checkDir.cd(u"test-pkg"_s));

    for (const QString &file : { u"info.yaml"_s, u"icon.png"_s, u"test.qml"_s }) {
        QVERIFY(checkDir.exists(file));
        QFile src(QDir(tmp.path()).absoluteFilePath(file));
        QVERIFY(src.open(QFile::ReadOnly));
        QFile dst(checkDir.absoluteFilePath(file));
        QVERIFY(dst.open(QFile::ReadOnly));
        QCOMPARE(src.readAll(), dst.readAll());
    }
}

void tst_PackagerTool::expired()
{
    QTemporaryDir tmp;
    QString errorString;
    createInfoYaml(tmp);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createCode(tmp);

    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("expired.ampkg"), tmp.path()), errorString), qPrintable(errorString));

    //TODO: why does openssl allow signing with expired certs at all?
    // expired dev key
    QVERIFY2(packagerCheck(PackagingJob::developerSign(
                               pathTo("expired.ampkg"),
                               pathTo("expired.dev-signed.ampkg"),
                               m_devExpiredCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    // dev verify expired
    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("expired.dev-signed.ampkg"),
                               m_commonCaFiles + m_devCaFiles, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"expired"), qPrintable(errorString));

    failToInstallPackage(pathTo("expired.dev-signed.ampkg"), u"expired"_s);
}

void tst_PackagerTool::revoked()
{
    QTemporaryDir tmp;
    QString errorString;
    createInfoYaml(tmp);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createCode(tmp);

    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("revoked.ampkg"), tmp.path()), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(PackagingJob::developerSign(
                               pathTo("revoked.ampkg"),
                               pathTo("revoked.dev-signed.ampkg"),
                               m_devRevokedCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    QVERIFY(!packagerCheck(PackagingJob::developerVerify(
                               pathTo("revoked.dev-signed.ampkg"),
                               m_commonCaFiles + m_devCaFiles, m_crlFiles), errorString));
    QVERIFY2(errorString.contains(u"revoked"), qPrintable(errorString));

    failToInstallPackage(pathTo("revoked.dev-signed.ampkg"), u"revoked"_s);
}

void tst_PackagerTool::developerSignCertBinding_data()
{
    // The "narrow" cert grants exactly:
    //   packageid/test-pkg, applicationid/test-app,
    //   capability/cap-allowed, category/test-category
    //
    // Each row manipulates info.yaml to either stay within those grants (success)
    // or step outside one of them (error). This is a wiring test: each negative row
    // confirms one info.yaml field flows into the matching require-list and surfaces
    // the right error. The exhaustive matching matrix lives in tst_signature.

    QTest::addColumn<QString>("pkgIdOverride");
    QTest::addColumn<QString>("appIdOverride");
    QTest::addColumn<QStringList>("capabilities");
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("ok-baseline")
        << QString { } << QString { } << QStringList { } << QStringList { } << QString { };
    QTest::newRow("ok-cap-allowed")
        << QString { } << QString { } << QStringList { u"cap-allowed"_s } << QStringList { } << QString { };
    QTest::newRow("ok-category-allowed")
        << QString { } << QString { } << QStringList { } << QStringList { u"test-category"_s } << QString { };
    QTest::newRow("pkgid-mismatch")
        << u"rogue-pkg"_s << QString { } << QStringList { } << QStringList { } << u"Package ID mismatch"_s;
    QTest::newRow("appid-mismatch")
        << QString { } << u"rogue-app"_s << QStringList { } << QStringList { } << u"Application ID mismatch"_s;
    QTest::newRow("capability-mismatch")
        << QString { } << QString { } << QStringList { u"forbidden-cap"_s } << QStringList { } << u"Capabilities mismatch"_s;
    QTest::newRow("category-mismatch")
        << QString { } << QString { } << QStringList { } << QStringList { u"wrong-category"_s } << u"Categories mismatch"_s;
}

void tst_PackagerTool::developerSignCertBinding()
{
    QFETCH(QString, pkgIdOverride);
    QFETCH(QString, appIdOverride);
    QFETCH(QStringList, capabilities);
    QFETCH(QStringList, categories);
    QFETCH(QString, errorString);

    QTemporaryDir tmp;
    QString error;

    auto manipulate = [&](QVariantMap &m) {
        if (!pkgIdOverride.isEmpty())
            m[u"id"_s] = pkgIdOverride;
        if (!categories.isEmpty())
            m[u"categories"_s] = categories;
        if (!appIdOverride.isEmpty() || !capabilities.isEmpty()) {
            QVariantList apps = m[u"applications"_s].toList();
            QVariantMap app = apps[0].toMap();
            if (!appIdOverride.isEmpty())
                app[u"id"_s] = appIdOverride;
            if (!capabilities.isEmpty())
                app[u"capabilities"_s] = capabilities;
            apps[0] = app;
            m[u"applications"_s] = apps;
        }
    };

    createInfoYaml(tmp, manipulate);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createCode(tmp);

    const QByteArray rowName = QTest::currentDataTag();
    const QString unsignedPkg = pathTo(("cb-" + rowName + ".ampkg").constData());
    const QString signedPkg = pathTo(("cb-" + rowName + ".dev-signed.ampkg").constData());

    QVERIFY2(packagerCheck(PackagingJob::create(unsignedPkg, tmp.path()), error), qPrintable(error));

    const bool ok = packagerCheck(PackagingJob::developerSign(
                                      unsignedPkg, signedPkg,
                                      m_devNarrowCertificate, m_devPassword), error);

    if (errorString.isEmpty()) {
        QVERIFY2(ok, qPrintable(error));
    } else {
        QVERIFY2(!ok, "Signing should have failed but succeeded");
        QVERIFY2(error.contains(errorString), qPrintable(error));
    }
}

void tst_PackagerTool::brokenMetadata_data()
{
    QTest::addColumn<QString>("yamlList");
    QTest::addColumn<QString>("yamlField");
    QTest::addColumn<QVariant>("yamlValue");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("missing-pkg-id")      << ""             << "id"      << QVariant() << "~.*Required fields are missing: id";
    QTest::newRow("missing-app-id")      << "applications" << "id"      << QVariant() << "~.*Required fields are missing: id";
    QTest::newRow("missing-app-runtime") << "applications" << "runtime" << QVariant() << "~.*Required fields are missing: runtime";
    QTest::newRow("missing-app-code")    << "applications" << "code"    << QVariant() << "~.*Required fields are missing: code";
    QTest::newRow("missing-intent-id")   << "intents"      << "id"      << QVariant() << "~.*Required fields are missing: id";
}

void tst_PackagerTool::brokenMetadata()
{
    QFETCH(QString, yamlList);
    QFETCH(QString, yamlField);
    QFETCH(QVariant, yamlValue);
    QFETCH(QString, errorString);

    QTemporaryDir tmp;

    createCode(tmp);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createInfoYaml(tmp, [=](QVariantMap &m) {
        auto &mref = yamlList.isEmpty() ? m : get<QVariantMap>(get<QVariantList>(m[yamlList])[0]);

        if (!yamlValue.isValid())
            mref.remove(yamlField);
        else
            mref[yamlField] = yamlValue;
    });

    // check if packaging actually fails with the expected error

    QString error;
    QVERIFY2(!packagerCheck(PackagingJob::create(pathTo("test.ampkg"), tmp.path()), error), qPrintable(error));
    QT_AM_CHECK_ERRORSTRING(error, errorString);
}

/*
    Specify an icon whose name is different from "icon.png".
    Packaging should work fine
 */
void tst_PackagerTool::iconFileName()
{
    QTemporaryDir tmp;
    QString errorString;

    createInfoYaml(tmp, [](QVariantMap &m) { m[u"icon"_s] = u"foo.bar"_s; });
    createCode(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createDummyFile(tmp, u"foo.bar"_s, "this-is-a-dummy-icon-file");

    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("test-foobar-icon.ampkg"), tmp.path()), errorString),
            qPrintable(errorString));

    // see if the package installs correctly

    installPackage(pathTo("test-foobar-icon.ampkg"), true);

    QDir checkDir(pathTo("internal-0"));
    QVERIFY(checkDir.cd(u"test-pkg"_s));

    for (const QString &file : { u"info.yaml"_s, u"foo.bar"_s, u"test.qml"_s }) {
        QVERIFY(checkDir.exists(file));
        QFile src(QDir(tmp.path()).absoluteFilePath(file));
        QVERIFY(src.open(QFile::ReadOnly));
        QFile dst(checkDir.absoluteFilePath(file));
        QVERIFY(dst.open(QFile::ReadOnly));
        QCOMPARE(src.readAll(), dst.readAll());
    }
}


bool tst_PackagerTool::createInfoYaml(QTemporaryDir &tmp, const std::function<void(QVariantMap &)> &manipulate)
{
    QByteArray yaml =
            "formatType: am-package\n"
            "formatVersion: 1\n"
            "---\n"
            "id: test-pkg\n"
            "name: { en_US: 'test' }\n"
            "icon: icon.png\n"
            "applications:\n"
            "- id: test-app\n"
            "  icon: app-icon.png\n"
            "  runtime: qml\n"
            "  code: test.qml\n"
            "intents:\n"
            "- id: test-intent\n"
            "  icon: intent-icon.png\n";

    if (manipulate) {
        QVector<QVariant> docs;
        try {
            docs = YamlParser::parseAllDocuments(yaml);
        } catch (...) {
        }

        QVariantMap map = docs.at(1).toMap();
        manipulate(map);
        yaml = YamlEmitter::fromVariantDocuments({ docs.at(0), map });
    }

    QFile infoYaml(QDir(tmp.path()).absoluteFilePath(u"info.yaml"_s));
    return infoYaml.open(QFile::WriteOnly) && infoYaml.write(yaml) == yaml.size();
}

bool tst_PackagerTool::createIconPng(QTemporaryDir &tmp, const QString &prefix)
{
    QFile iconPng(QDir(tmp.path()).absoluteFilePath(prefix + u"icon.png"_s));
    return iconPng.open(QFile::WriteOnly) && iconPng.write("\x89PNG") == 4;
}

bool tst_PackagerTool::createCode(QTemporaryDir &tmp)
{
    QFile code(QDir(tmp.path()).absoluteFilePath(u"test.qml"_s));
    return code.open(QFile::WriteOnly) && code.write("// test") == 7LL;
}

void tst_PackagerTool::createDummyFile(QTemporaryDir &tmp, const QString &fileName, const char *data)
{
    QFile code(QDir(tmp.path()).absoluteFilePath(fileName));
    QVERIFY(code.open(QFile::WriteOnly));

    auto written = code.write(data);

    QCOMPARE(written, static_cast<qint64>(strlen(data)));
}

void tst_PackagerTool::installPackage(const QString &filePath, bool allowUnsigned)
{
    QSignalSpy finishedSpy(m_pm, &PackageManager::taskFinished);

    DevMode devMode(PackageManager::DevelopmentMode::System, allowUnsigned);

    QString taskId = m_pm->startPackageInstallation(filePath);
    m_pm->acknowledgePackageInstallation(taskId);

    QVERIFY(finishedSpy.wait(2 * spyTimeout));
    QCOMPARE(finishedSpy.first()[0].toString(), taskId);
}

void tst_PackagerTool::failToInstallPackage(const QString &filePath, const QString &expectedError)
{
    QSignalSpy failedSpy(m_pm, &PackageManager::taskFailed);

    DevMode devMode(PackageManager::DevelopmentMode::System);

    QString taskId = m_pm->startPackageInstallation(filePath);
    m_pm->acknowledgePackageInstallation(taskId);

    QVERIFY(failedSpy.wait(2 * spyTimeout));
    QCOMPARE(failedSpy.first()[0].toString(), taskId);
    QVERIFY(failedSpy.first()[2].toString().contains(expectedError));
}

QTEST_GUILESS_MAIN(tst_PackagerTool)

#include "tst_packager-tool.moc"
