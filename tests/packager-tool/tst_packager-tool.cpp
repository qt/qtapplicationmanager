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

#include <QtTest>
#include <QCoreApplication>

#include "global.h"
#include "installationlocation.h"
#include "applicationmanager.h"
#include "application.h"
#include "qtyaml.h"
#include "exception.h"
#include "packagingjob.h"
#include "applicationinstaller.h"
#include "qmlinprocessruntime.h"
#include "runtimefactory.h"
#include "utilities.h"

#include "../error-checking.h"

QT_USE_NAMESPACE_AM

static int spyTimeout = 5000; // shorthand for specifying QSignalSpy timeouts

class tst_PackagerTool : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void test();
    void brokenMetadata_data();
    void brokenMetadata();
    void iconFileName();

private:
    QString pathTo(const char *file)
    {
        return QDir(m_workDir.path()).absoluteFilePath(QLatin1String(file));
    }

    bool createInfoYaml(QTemporaryDir &tmp, const QString &changeField = QString(), const QVariant &toValue = QVariant());
    bool createIconPng(QTemporaryDir &tmp);
    bool createCode(QTemporaryDir &tmp);
    void createDummyFile(QTemporaryDir &tmp, const QString &fileName, const char *data);

    void installPackage(const QString &filePath);

    ApplicationInstaller *m_ai = nullptr;
    QTemporaryDir m_workDir;

    QString m_devPassword;
    QString m_devCertificate;
    QString m_storePassword;
    QString m_storeCertificate;
    QStringList m_caFiles;
    QString m_hardwareId;
};

void tst_PackagerTool::initTestCase()
{
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    spyTimeout *= timeoutFactor();

    QVERIFY(m_workDir.isValid());

    QVERIFY(QDir::root().mkpath(pathTo("manifests")));
    QVERIFY(QDir::root().mkpath(pathTo("image-mounts")));
    QVERIFY(QDir::root().mkpath(pathTo("internal-0")));
    QVERIFY(QDir::root().mkpath(pathTo("documents-0")));

    m_hardwareId = "foobar";

    QVariantMap internalLocation {
        { "id", "internal-0" },
        { "installationPath", pathTo("internal-0") },
        { "documentPath", pathTo("documents-0") },
    };
    QVector<InstallationLocation> locations = InstallationLocation::parseInstallationLocations({ internalLocation }, m_hardwareId);

    QString errorString;
    m_ai = ApplicationInstaller::createInstance(locations, pathTo("manifests"), pathTo("image-mounts"), m_hardwareId, &errorString);
    QVERIFY2(m_ai, qPrintable(errorString));

    QVERIFY(ApplicationManager::createInstance(true));


    // crypto stuff - we need to load the root CA and developer CA certificates

    QFile devcaFile(qL1S(AM_TESTDATA_DIR "certificates/devca.crt"));
    QFile caFile(qL1S(AM_TESTDATA_DIR "certificates/ca.crt"));
    QVERIFY2(devcaFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));
    QVERIFY2(caFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));

    QList<QByteArray> chainOfTrust;
    chainOfTrust << devcaFile.readAll() << caFile.readAll();
    QVERIFY(!chainOfTrust.at(0).isEmpty());
    QVERIFY(!chainOfTrust.at(1).isEmpty());
    m_ai->setCACertificates(chainOfTrust);

    m_caFiles << devcaFile.fileName() << caFile.fileName();

    m_devPassword = qSL("password");
    m_devCertificate = qL1S(AM_TESTDATA_DIR "certificates/dev1.p12");
    m_storePassword = qSL("password");
    m_storeCertificate = qL1S(AM_TESTDATA_DIR "certificates/store.p12");

    RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager(qSL("qml")));
}

void tst_PackagerTool::cleanup()
{
    recursiveOperation(pathTo("manifests"), safeRemove);
    recursiveOperation(pathTo("image-mounts"), safeRemove);
    recursiveOperation(pathTo("internal-0"), safeRemove);
    recursiveOperation(pathTo("documents-0"), safeRemove);

    QDir dir(m_workDir.path());
    QStringList fileNames = dir.entryList(QDir::Files);
    for (auto fileName : fileNames)
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
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.appkg"), pathTo("test.appkg")), errorString));
    QVERIFY2(errorString.contains(qL1S("not a directory")), qPrintable(errorString));

    // no valid info.yaml
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(qL1S("could not open file for reading")), qPrintable(errorString));

    // add an info.yaml file
    createInfoYaml(tmp);

    // no icon
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(qL1S("missing the file referenced by the 'icon' field")), qPrintable(errorString));

    // add an icon
    createIconPng(tmp);

    // no valid code
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains(qL1S("missing the file referenced by the 'code' field")), qPrintable(errorString));

    // add a code file
    createCode(tmp);

    // invalid destination
    QVERIFY(!packagerCheck(PackagingJob::create(tmp.path(), tmp.path()), errorString));
    QVERIFY2(errorString.contains(qL1S("could not create package file")), qPrintable(errorString));

    // now everything is correct - try again
    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("test.appkg"), tmp.path()), errorString), qPrintable(errorString));

    // invalid source package
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("no-such-file"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains(qL1S("does not exist")), qPrintable(errorString));

    // invalid destination package
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("test.appkg"),
                               pathTo("."),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains(qL1S("could not create package file")), qPrintable(errorString));


    // invalid dev key
    QVERIFY(!packagerCheck(PackagingJob::developerSign(
                               pathTo("test.appkg"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               qSL("wrong-password")), errorString));
    QVERIFY2(errorString.contains(qL1S("could not create signature")), qPrintable(errorString));

    // invalid store key
    QVERIFY(!packagerCheck(PackagingJob::storeSign(
                               pathTo("test.appkg"),
                               pathTo("test.store-signed.appkg"),
                               m_storeCertificate,
                               qSL("wrong-password"),
                               m_hardwareId), errorString));
    QVERIFY2(errorString.contains(qL1S("could not create signature")), qPrintable(errorString));

    // sign
    QVERIFY2(packagerCheck(PackagingJob::developerSign(
                               pathTo("test.appkg"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(PackagingJob::storeSign(
                               pathTo("test.appkg"),
                               pathTo("test.store-signed.appkg"),
                               m_storeCertificate,
                               m_storePassword,
                               m_hardwareId), errorString), qPrintable(errorString));

    // verify
    QVERIFY2(packagerCheck(PackagingJob::developerVerify(
                               pathTo("test.dev-signed.appkg"),
                               m_caFiles), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(PackagingJob::storeVerify(
                               pathTo("test.store-signed.appkg"),
                               m_caFiles,
                               m_hardwareId), errorString), qPrintable(errorString));

    // now that we have it, see if the package actually installs correctly

    installPackage(pathTo("test.dev-signed.appkg"));

    QDir checkDir(pathTo("internal-0"));
    QVERIFY(checkDir.cd(qSL("com.pelagicore.test")));

    for (const QString &file : { qSL("info.yaml"), qSL("icon.png"), qSL("test.qml") }) {
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
    QTest::addColumn<QString>("yamlField");
    QTest::addColumn<QVariant>("yamlValue");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("missing-name")       << "name"    << QVariant("") << "~.*the 'name' field must not be empty";
    QTest::newRow("missing-runtime")    << "runtime" << QVariant("") << "~.*the 'runtimeName' field must not be empty";
    QTest::newRow("missing-identifier") << "id"      << QVariant("") << "~.*the identifier \\(\\) is not a valid application-id: must not be empty";
    QTest::newRow("missing-code")       << "code"    << QVariant("") << "~.*the 'code' field must not be empty";
}

void tst_PackagerTool::brokenMetadata()
{
    QFETCH(QString, yamlField);
    QFETCH(QVariant, yamlValue);
    QFETCH(QString, errorString);

    QTemporaryDir tmp;

    createCode(tmp);
    createIconPng(tmp);
    createInfoYaml(tmp, yamlField, yamlValue);

    // check if packaging actually fails with the expected error

    QString error;
    QVERIFY(!packagerCheck(PackagingJob::create(pathTo("test.appkg"), tmp.path()), error));
    AM_CHECK_ERRORSTRING(error, errorString);
}

/*
    Specify an icon whose name is different from "icon.png".
    Packaging should work fine
 */
void tst_PackagerTool::iconFileName()
{
    QTemporaryDir tmp;
    QString errorString;

    createInfoYaml(tmp, "icon", "foo.bar");
    createCode(tmp);
    createDummyFile(tmp, "foo.bar", "this-is-a-dummy-icon-file");

    QVERIFY2(packagerCheck(PackagingJob::create(pathTo("test-foobar-icon.appkg"), tmp.path()), errorString),
            qPrintable(errorString));

    // see if the package installs correctly

    m_ai->setAllowInstallationOfUnsignedPackages(true);
    installPackage(pathTo("test-foobar-icon.appkg"));
    m_ai->setAllowInstallationOfUnsignedPackages(false);

    QDir checkDir(pathTo("internal-0"));
    QVERIFY(checkDir.cd(qSL("com.pelagicore.test")));

    for (const QString &file : { qSL("info.yaml"), qSL("foo.bar"), qSL("test.qml") }) {
        QVERIFY(checkDir.exists(file));
        QFile src(QDir(tmp.path()).absoluteFilePath(file));
        QVERIFY(src.open(QFile::ReadOnly));
        QFile dst(checkDir.absoluteFilePath(file));
        QVERIFY(dst.open(QFile::ReadOnly));
        QCOMPARE(src.readAll(), dst.readAll());
    }
}


bool tst_PackagerTool::createInfoYaml(QTemporaryDir &tmp, const QString &changeField, const QVariant &toValue)
{
    QByteArray yaml =
            "formatVersion: 1\n"
            "formatType: am-application\n"
            "---\n"
            "id: com.pelagicore.test\n"
            "name: { en_US: 'test' }\n"
            "icon: icon.png\n"
            "code: test.qml\n"
            "runtime: qml\n";

    if (!changeField.isEmpty()) {
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(yaml);
        QVariantMap map = docs.at(1).toMap();
        map[changeField] = toValue;
        yaml = QtYaml::yamlFromVariantDocuments({ docs.at(0), map });
    }

    QFile infoYaml(QDir(tmp.path()).absoluteFilePath(qSL("info.yaml")));
    return infoYaml.open(QFile::WriteOnly) && infoYaml.write(yaml) == yaml.size();
}

bool tst_PackagerTool::createIconPng(QTemporaryDir &tmp)
{
    QFile iconPng(QDir(tmp.path()).absoluteFilePath(qSL("icon.png")));
    return iconPng.open(QFile::WriteOnly) && iconPng.write("\x89PNG") == 4;
}

bool tst_PackagerTool::createCode(QTemporaryDir &tmp)
{
    QFile code(QDir(tmp.path()).absoluteFilePath(qSL("test.qml")));
    return code.open(QFile::WriteOnly) && code.write("// test") == 7LL;
}

void tst_PackagerTool::createDummyFile(QTemporaryDir &tmp, const QString &fileName, const char *data)
{
    QFile code(QDir(tmp.path()).absoluteFilePath(fileName));
    QVERIFY(code.open(QFile::WriteOnly));

    auto written = code.write(data);

    QCOMPARE(written, (qint64)strlen(data));
}

void tst_PackagerTool::installPackage(const QString &filePath)
{
    QSignalSpy finishedSpy(m_ai, &ApplicationInstaller::taskFinished);

    m_ai->setDevelopmentMode(true); // allow packages without store signature

    QString taskId = m_ai->startPackageInstallation(qSL("internal-0"),
            QUrl::fromLocalFile(filePath));
    m_ai->acknowledgePackageInstallation(taskId);

    QVERIFY(finishedSpy.wait(2 * spyTimeout));
    QCOMPARE(finishedSpy.first()[0].toString(), taskId);

    m_ai->setDevelopmentMode(false);
}

QTEST_GUILESS_MAIN(tst_PackagerTool)

#include "tst_packager-tool.moc"
