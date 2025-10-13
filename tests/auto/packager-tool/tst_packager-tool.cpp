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

    PackageManager *m_pm = nullptr;
    QTemporaryDir m_workDir;

    QString m_devPassword;
    QString m_devCertificate;
    QString m_storePassword;
    QString m_storeCertificate;
    QStringList m_commonCaFiles;
    QStringList m_devCaFiles;
    QStringList m_storeCaFiles;
    QString m_hardwareId;
};

void tst_PackagerTool::initTestCase()
{
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    Sudo::fallbackServer();

    spyTimeout *= timeoutFactor();

    QVERIFY(m_workDir.isValid());
    QVERIFY(QDir::root().mkpath(pathTo("internal-0")));
    QVERIFY(QDir::root().mkpath(pathTo("documents-0")));

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


    // crypto stuff - we need to load the root CA and developer CA certificates

    QString devcaFile = u"" AM_TESTDATA_DIR "certificates/devca.crt"_s;
    QString caFile = u"" AM_TESTDATA_DIR "certificates/ca.crt"_s;

    QVERIFY_THROWS_NO_EXCEPTION(m_pm->loadCertificates({ caFile }, { devcaFile }, { }));

    m_commonCaFiles << caFile;
    m_devCaFiles << devcaFile;

    m_devPassword = u"password"_s;
    m_devCertificate = u"" AM_TESTDATA_DIR "certificates/dev1.p12"_s;
    m_storePassword = u"password"_s;
    m_storeCertificate = u"" AM_TESTDATA_DIR "certificates/store.p12"_s;

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

    // invalid store key
    QVERIFY(!packagerCheck(PackagingJob::storeSign(
                               pathTo("test.ampkg"),
                               pathTo("test.store-signed.ampkg"),
                               m_storeCertificate,
                               u"wrong-password"_s,
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(u"could not create signature"), qPrintable(errorString));

    // sign
    QVERIFY2(packagerCheck(PackagingJob::developerSign(
                               pathTo("test.ampkg"),
                               pathTo("test.dev-signed.ampkg"),
                               m_devCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(PackagingJob::storeSign(
                               pathTo("test.ampkg"),
                               pathTo("test.store-signed.ampkg"),
                               m_storeCertificate,
                               m_storePassword,
                               m_hardwareId), errorString), qPrintable(errorString));

    // verify
    QVERIFY2(packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.ampkg"),
                               m_commonCaFiles + m_devCaFiles), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.ampkg"),
                               m_commonCaFiles + m_storeCaFiles,
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
        yaml = QtYaml::yamlFromVariantDocuments({ docs.at(0), map });
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

QTEST_GUILESS_MAIN(tst_PackagerTool)

#include "tst_packager-tool.moc"
