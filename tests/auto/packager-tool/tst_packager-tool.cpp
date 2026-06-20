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
#include "qmlinprocruntime.h"
#include "runtimefactory.h"
#include "utilities.h"
#include "sudo.h"

#include "../error-checking.h"
#include "../devmode.h"
#include "../toolrunner.h"

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


class PackagerTool : public ToolRunner
{
public:
    PackagerTool(const std::initializer_list<QString> &list)
        : PackagerTool(QStringList(list))
    { }
    PackagerTool(const QStringList &arguments)
        : ToolRunner("appman-packager", s_command, arguments)
    { }

    static void setPackagerPath(const QString &path) { s_command = path; }

private:
    static inline QString s_command;
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

    const QString packagerPath = ToolRunner::findTool(u"appman-packager"_s);
    QVERIFY(!packagerPath.isEmpty());
    PackagerTool::setPackagerPath(packagerPath);

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

void tst_PackagerTool::test()
{
    QTemporaryDir tmp;

    // no valid destination
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), pathTo("test.ampkg") });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("is not a directory"), p.stdErr.constData());
    }
    // no valid info.yaml
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("Cannot open for reading"), p.stdErr.constData());
    }

    // add an info.yaml file
    createInfoYaml(tmp);

    // no icon
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("missing the file referenced by the 'icon' field"), p.stdErr.constData());
    }

    // add an icon
    createIconPng(tmp);

    // missing intent icon
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("missing the file referenced by the 'icon' field for intent 'test-intent'"), p.stdErr.constData());
    }

    // add an icon for the intent
    createIconPng(tmp, u"intent-"_s);

    // no valid code
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("missing the file referenced by the 'code' field"), p.stdErr.constData());
    }

    // add a code file
    createCode(tmp);

    // missing app icon
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("missing the file referenced by the 'icon' field for application 'test-app'"), p.stdErr.constData());
    }

    // add an icon for the app
    createIconPng(tmp, u"app-"_s);

    // invalid destination
    {
        PackagerTool p({ u"create-package"_s, tmp.path(), tmp.path() });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create package file"), p.stdErr.constData());
    }

    // now everything is correct - try again
    {
        PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
        QVERIFY2(p.call(), p.failure.constData());
    }

    // invalid source package
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("no-such-file"), pathTo("test.dev-signed.ampkg"),
                         m_devCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("does not exist"), p.stdErr.constData());
    }
    // invalid destination package
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("test.ampkg"), pathTo("."),
                         m_devCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create package file"), p.stdErr.constData());
    }
    // invalid dev key
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("test.ampkg"), pathTo("test.dev-signed.ampkg"),
                         m_devCertificate, u"--password"_s, u"pass:wrong-password"_s });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create signature"), p.stdErr.constData());
    }
    // store key as dev key
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("test.ampkg"), pathTo("test.dev-signed.ampkg"),
                         m_storeCertificate, u"--password"_s, u"pass:"_s + m_storePassword });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create signature"), p.stdErr.constData());
    }
    // dev sign
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("test.ampkg"), pathTo("test.dev-signed.ampkg"),
                         m_devCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
        QVERIFY2(p.call(), p.failure.constData());
    }

    // invalid store key
    {
        PackagerTool p({ u"store-sign-package"_s, pathTo("test.dev-signed.ampkg"), pathTo("test.store-signed.ampkg"),
                         m_storeCertificate, u"--password"_s, u"pass:wrong-password"_s, u"--hardware-id"_s, m_hardwareId });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create signature"), p.stdErr.constData());
    }
    // dev key as store key
    {
        PackagerTool p({ u"store-sign-package"_s, pathTo("test.dev-signed.ampkg"), pathTo("test.store-signed.ampkg"),
                         m_devCertificate, u"--password"_s, u"pass:"_s + m_devPassword, u"--hardware-id"_s, m_hardwareId });
        QVERIFY(!p.call());
        QVERIFY2(p.stdErr.contains("could not create signature"), p.stdErr.constData());
    }
    // store sign
    {
        PackagerTool p({ u"store-sign-package"_s, pathTo("test.dev-signed.ampkg"), pathTo("test.store-signed.ampkg"),
                         m_storeCertificate, u"--password"_s, u"pass:"_s + m_storePassword, u"--hardware-id"_s, m_hardwareId });
        QVERIFY2(p.call(), p.failure.constData());
    }

    // --crl <file> ... arguments, shared by all verify invocations below
    QStringList crlArgs;
    for (const QString &crl : std::as_const(m_crlFiles))
        crlArgs << u"--crl"_s << crl;

    // dev verify without any CA: the tool requires at least one certificate and bails out with usage
    {
        PackagerTool p({ u"dev-verify-package"_s, pathTo("test.dev-signed.ampkg") });
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Usage:"), p.stdOut.constData());
    }

    QStringList devVerifyBaseArgs { u"dev-verify-package"_s, u"--verbose"_s,
                                    pathTo("test.dev-signed.ampkg") };

    // dev verify without root CA
    {
        PackagerTool p(devVerifyBaseArgs + m_devCaFiles + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // dev verify without dev CA
    {
        PackagerTool p(devVerifyBaseArgs + m_commonCaFiles + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // dev verify with store CA
    {
        PackagerTool p(devVerifyBaseArgs + m_commonCaFiles + m_storeCaFiles + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // dev verify
    {
        PackagerTool p(devVerifyBaseArgs + m_commonCaFiles + m_devCaFiles + crlArgs);
        QVERIFY2(p.call(), p.failure.constData());
    }

    // store verify without any CA: the tool requires at least one certificate and bails out with usage
    {
        PackagerTool p({ u"store-verify-package"_s, pathTo("test.store-signed.ampkg"), m_hardwareId });
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Usage:"), p.stdOut.constData());
    }

    QStringList storeVerifyBaseArgs { u"store-verify-package"_s, u"--verbose"_s,
                                     pathTo("test.store-signed.ampkg") };

    // store verify without root CA
    {
        PackagerTool p(storeVerifyBaseArgs + m_storeCaFiles + QStringList { m_hardwareId } + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // store verify without store CA
    {
        PackagerTool p(storeVerifyBaseArgs + m_commonCaFiles + QStringList { m_hardwareId } + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // store verify with dev CA
    {
        PackagerTool p(storeVerifyBaseArgs + m_commonCaFiles + m_devCaFiles + QStringList { m_hardwareId } + crlArgs);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("Failed to verify signature"), p.stdOut.constData());
    }
    // store verify
    {
        PackagerTool p(storeVerifyBaseArgs + m_commonCaFiles + m_storeCaFiles + QStringList { m_hardwareId } + crlArgs);
        QVERIFY2(p.call(), p.failure.constData());
    }

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
    createInfoYaml(tmp);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createCode(tmp);

    {
        PackagerTool p({ u"create-package"_s, pathTo("expired.ampkg"), tmp.path() });
        QVERIFY2(p.call(), p.failure.constData());
    }

    //TODO: why does openssl allow signing with expired certs at all?
    // expired dev key
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("expired.ampkg"), pathTo("expired.dev-signed.ampkg"),
                         m_devExpiredCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
        QVERIFY2(p.call(), p.failure.constData());
    }

    // dev verify expired
    {
        QStringList args { u"dev-verify-package"_s, u"--verbose"_s, pathTo("expired.dev-signed.ampkg") };
        args += m_commonCaFiles + m_devCaFiles;
        for (const QString &crl : std::as_const(m_crlFiles))
            args << u"--crl"_s << crl;
        PackagerTool p(args);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("expired"), p.stdOut.constData());
    }

    failToInstallPackage(pathTo("expired.dev-signed.ampkg"), u"expired"_s);
}

void tst_PackagerTool::revoked()
{
    QTemporaryDir tmp;
    createInfoYaml(tmp);
    createIconPng(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createCode(tmp);

    {
        PackagerTool p({ u"create-package"_s, pathTo("revoked.ampkg"), tmp.path() });
        QVERIFY2(p.call(), p.failure.constData());
    }
    {
        PackagerTool p({ u"dev-sign-package"_s, pathTo("revoked.ampkg"), pathTo("revoked.dev-signed.ampkg"),
                         m_devRevokedCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
        QVERIFY2(p.call(), p.failure.constData());
    }
    {
        QStringList args { u"dev-verify-package"_s, u"--verbose"_s, pathTo("revoked.dev-signed.ampkg") };
        args += m_commonCaFiles + m_devCaFiles;
        for (const QString &crl : std::as_const(m_crlFiles))
            args << u"--crl"_s << crl;
        PackagerTool p(args);
        QVERIFY(!p.call());
        QVERIFY2(p.stdOut.contains("revoked"), p.stdOut.constData());
    }

    failToInstallPackage(pathTo("revoked.dev-signed.ampkg"), u"revoked"_s);
}

void tst_PackagerTool::developerSignCertBinding_data()
{
    // The "narrow" cert grants exactly:
    //   packageid/test-pkg, applicationid/test-app,
    //   capability/cap-allowed, category/test-category, runtime/qml
    //
    // Each row manipulates info.yaml to either stay within those grants (success)
    // or step outside one of them (error). This is a wiring test: each negative row
    // confirms one info.yaml field flows into the matching require-list and surfaces
    // the right error. The exhaustive matching matrix lives in tst_signature.

    QTest::addColumn<QString>("pkgIdOverride");
    QTest::addColumn<QString>("appIdOverride");
    QTest::addColumn<QStringList>("capabilities");
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<QString>("runtimeOverride");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("ok-baseline")
        << QString { } << QString { } << QStringList { } << QStringList { } << QString { } << QString { };
    QTest::newRow("ok-cap-allowed")
        << QString { } << QString { } << QStringList { u"cap-allowed"_s } << QStringList { } << QString { } << QString { };
    QTest::newRow("ok-category-allowed")
        << QString { } << QString { } << QStringList { } << QStringList { u"test-category"_s } << QString { } << QString { };
    QTest::newRow("pkgid-mismatch")
        << u"rogue-pkg"_s << QString { } << QStringList { } << QStringList { } << QString { } << u"Package ID mismatch"_s;
    QTest::newRow("appid-mismatch")
        << QString { } << u"rogue-app"_s << QStringList { } << QStringList { } << QString { } << u"Application ID mismatch"_s;
    QTest::newRow("capability-mismatch")
        << QString { } << QString { } << QStringList { u"forbidden-cap"_s } << QStringList { } << QString { } << u"Capabilities mismatch"_s;
    QTest::newRow("category-mismatch")
        << QString { } << QString { } << QStringList { } << QStringList { u"wrong-category"_s } << QString { } << u"Categories mismatch"_s;
    QTest::newRow("runtime-inprocess-blocked")
        << QString { } << QString { } << QStringList { } << QStringList { } << u"qml-inprocess"_s << u"Runtimes mismatch"_s;
}

void tst_PackagerTool::developerSignCertBinding()
{
    QFETCH(QString, pkgIdOverride);
    QFETCH(QString, appIdOverride);
    QFETCH(QStringList, capabilities);
    QFETCH(QStringList, categories);
    QFETCH(QString, runtimeOverride);
    QFETCH(QString, errorString);

    QTemporaryDir tmp;

    auto manipulate = [&](QVariantMap &m) {
        if (!pkgIdOverride.isEmpty())
            m[u"id"_s] = pkgIdOverride;
        if (!categories.isEmpty())
            m[u"categories"_s] = categories;
        if (!appIdOverride.isEmpty() || !capabilities.isEmpty() || !runtimeOverride.isEmpty()) {
            QVariantList apps = m[u"applications"_s].toList();
            QVariantMap app = apps[0].toMap();
            if (!appIdOverride.isEmpty())
                app[u"id"_s] = appIdOverride;
            if (!capabilities.isEmpty())
                app[u"capabilities"_s] = capabilities;
            if (!runtimeOverride.isEmpty())
                app[u"runtime"_s] = runtimeOverride;
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

    {
        PackagerTool p({ u"create-package"_s, unsignedPkg, tmp.path() });
        QVERIFY2(p.call(), p.failure.constData());
    }

    PackagerTool sign({ u"dev-sign-package"_s, unsignedPkg, signedPkg,
                        m_devNarrowCertificate, u"--password"_s, u"pass:"_s + m_devPassword });
    const bool ok = sign.call();

    if (errorString.isEmpty()) {
        QVERIFY2(ok, sign.failure.constData());
    } else {
        QVERIFY2(!ok, "Signing should have failed but succeeded");
        QVERIFY2(sign.stdErr.contains(errorString.toLocal8Bit()), sign.stdErr.constData());
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

    PackagerTool p({ u"create-package"_s, pathTo("test.ampkg"), tmp.path() });
    QVERIFY(!p.call());
    QString error = QString::fromLocal8Bit(p.stdErr).trimmed();
    QT_AM_CHECK_ERRORSTRING(error, errorString);
}

/*
    Specify an icon whose name is different from "icon.png".
    Packaging should work fine
 */
void tst_PackagerTool::iconFileName()
{
    QTemporaryDir tmp;

    createInfoYaml(tmp, [](QVariantMap &m) { m[u"icon"_s] = u"foo.bar"_s; });
    createCode(tmp);
    createIconPng(tmp, u"app-"_s);
    createIconPng(tmp, u"intent-"_s);
    createDummyFile(tmp, u"foo.bar"_s, "this-is-a-dummy-icon-file");

    {
        PackagerTool p({ u"create-package"_s, pathTo("test-foobar-icon.ampkg"), tmp.path() });
        QVERIFY2(p.call(), p.failure.constData());
    }

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
