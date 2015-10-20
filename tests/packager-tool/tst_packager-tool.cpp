/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
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
#include "utilities.h"
#include "packager.h"
#include "applicationinstaller.h"


class tst_PackagerTool : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void test();
    void brokenMetadata_data();
    void brokenMetadata();

private:
    QString pathTo(const QString &file)
    {
        return QDir(m_workDir.path()).absoluteFilePath(file);
    }

    bool createInfoYaml(TemporaryDir &tmp, const QString &changeField = QString(), const QVariant &toValue = QVariant());
    bool createIconPng(TemporaryDir &tmp);
    bool createCode(TemporaryDir &tmp);


    ApplicationInstaller *m_ai = 0;
    TemporaryDir m_workDir;

    QString m_devPassword;
    QString m_devCertificate;
    QString m_storePassword;
    QString m_storeCertificate;
    QStringList m_caFiles;
};

void tst_PackagerTool::initTestCase()
{
    QVERIFY(m_workDir.isValid());

    QVERIFY(QDir::root().mkpath(pathTo("manifests")));
    QVERIFY(QDir::root().mkpath(pathTo("image-mounts")));
    QVERIFY(QDir::root().mkpath(pathTo("internal-0")));
    QVERIFY(QDir::root().mkpath(pathTo("documents-0")));

    QVariantMap internalLocation {
        { "id", "internal-0" },
        { "installationPath", pathTo("internal-0") },
        { "documentPath", pathTo("documents-0") },
    };
    QList<InstallationLocation> locations = InstallationLocation::parseInstallationLocations({ internalLocation });

    QString errorString;
    m_ai = ApplicationInstaller::createInstance(locations, pathTo("manifests"), pathTo("image-mounts"), &errorString);
    QVERIFY2(m_ai, qPrintable(errorString));

    QVERIFY2(ApplicationManager::createInstance(0, &errorString), qPrintable(errorString));


    // crypto stuff - we need to load the root CA and developer CA certificates

    QFile devcaFile(AM_TESTDATA_DIR "certificates/devca.crt");
    QFile caFile(AM_TESTDATA_DIR "certificates/ca.crt");
    QVERIFY2(devcaFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));
    QVERIFY2(caFile.open(QIODevice::ReadOnly), qPrintable(devcaFile.errorString()));

    QList<QByteArray> chainOfTrust;
    chainOfTrust << devcaFile.readAll() << caFile.readAll();
    QVERIFY(!chainOfTrust.at(0).isEmpty());
    QVERIFY(!chainOfTrust.at(1).isEmpty());
    m_ai->setCACertificates(chainOfTrust);

    m_caFiles << devcaFile.fileName() << caFile.fileName();

    m_devPassword = "password";
    m_devCertificate = AM_TESTDATA_DIR "certificates/dev1.p12";
    m_storePassword = "password";
    m_storeCertificate = AM_TESTDATA_DIR "certificates/store.p12";
}


// exceptions are nice -- just not for unit testing :)
static bool packagerCheck(Packager *p, QString &errorString)
{
    try {
        p->execute();
        errorString.clear();
        return (p->resultCode() == 0);
    } catch (const Exception &e) { \
        errorString = e.errorString();
        return false;
    }
}

void tst_PackagerTool::test()
{
    TemporaryDir tmp;
    QString errorString;

    // no valid destination
    QVERIFY(!packagerCheck(Packager::create(pathTo("test.appkg"), pathTo("test.appkg")), errorString));
    QVERIFY2(errorString.contains("not a directory"), qPrintable(errorString));

    // no valid info.yaml
    QVERIFY(!packagerCheck(Packager::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains("could not open file for reading"), qPrintable(errorString));

    // add an info.yaml file
    createInfoYaml(tmp);

    // no icon
    QVERIFY(!packagerCheck(Packager::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains("missing the 'icon.png' file"), qPrintable(errorString));

    // add an icon
    createIconPng(tmp);

    // no valid code
    QVERIFY(!packagerCheck(Packager::create(pathTo("test.appkg"), tmp.path()), errorString));
    QVERIFY2(errorString.contains("missing the file referenced by the 'code' field"), qPrintable(errorString));

    // add a code file
    createCode(tmp);

    // invalid destination
    QVERIFY(!packagerCheck(Packager::create(tmp.path(), tmp.path()), errorString));
    QVERIFY2(errorString.contains("could not create package file"), qPrintable(errorString));

    // now everything is correct - try again
    QVERIFY2(packagerCheck(Packager::create(pathTo("test.appkg"), tmp.path()), errorString), qPrintable(errorString));

    // invalid source package
    QVERIFY(!packagerCheck(Packager::developerSign(
                               pathTo("no-such-file"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains("does not exist"), qPrintable(errorString));

    // invalid destination package
    QVERIFY(!packagerCheck(Packager::developerSign(
                               pathTo("test.appkg"),
                               pathTo("."),
                               m_devCertificate,
                               m_devPassword), errorString));
    QVERIFY2(errorString.contains("could not create package file"), qPrintable(errorString));


    // invalid dev key
    QVERIFY(!packagerCheck(Packager::developerSign(
                               pathTo("test.appkg"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               "wrong-password"), errorString));
    QVERIFY2(errorString.contains("could not create signature"), qPrintable(errorString));

    // invalid store key
    QVERIFY(!packagerCheck(Packager::storeSign(
                               pathTo("test.appkg"),
                               pathTo("test.store-signed.appkg"),
                               m_storeCertificate,
                               "wrong-password",
                               hardwareId()), errorString));
    QVERIFY2(errorString.contains("could not create signature"), qPrintable(errorString));

    // sign
    QVERIFY2(packagerCheck(Packager::developerSign(
                               pathTo("test.appkg"),
                               pathTo("test.dev-signed.appkg"),
                               m_devCertificate,
                               m_devPassword), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(Packager::storeSign(
                               pathTo("test.appkg"),
                               pathTo("test.store-signed.appkg"),
                               m_storeCertificate,
                               m_storePassword,
                               hardwareId()), errorString), qPrintable(errorString));

    // verify
    QVERIFY2(packagerCheck(Packager::developerVerify(
                               pathTo("test.dev-signed.appkg"),
                               m_caFiles), errorString), qPrintable(errorString));

    QVERIFY2(packagerCheck(Packager::storeVerify(
                               pathTo("test.store-signed.appkg"),
                               m_caFiles,
                               hardwareId()), errorString), qPrintable(errorString));

    // now that we have it, see if the package actually installs correctly

    QSignalSpy finishedSpy(m_ai, &ApplicationInstaller::taskFinished);

    m_ai->setDevelopmentMode(true); // allow packages without store signature

    QString taskId = m_ai->startPackageInstallation("internal-0", QUrl::fromLocalFile(pathTo("test.dev-signed.appkg")));
    m_ai->acknowledgePackageInstallation(taskId);

    QVERIFY(finishedSpy.wait());
    QCOMPARE(finishedSpy.first()[0].toString(), taskId);

    m_ai->setDevelopmentMode(false);

    QDir checkDir(pathTo("internal-0"));
    QVERIFY(checkDir.cd("com.pelagicore.test"));

    for (const QString &file : { "info.yaml", "icon.png", "test.qml" }) {
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

    QTest::newRow("missing-name")       << "name"    << QVariant("") << "the 'name' field must not be empty";
    QTest::newRow("missing-runtime")    << "runtime" << QVariant("") << "the 'runtimeName' field must not be empty";
    QTest::newRow("missing-identifier") << "id"      << QVariant("") << "the identifier () is not a valid reverse-DNS name: the minimum amount of parts (subdomains) is 3 (found 1)";
    QTest::newRow("missing-code")       << "code"    << QVariant("") << "the 'code' field must not be empty";
}

void tst_PackagerTool::brokenMetadata()
{
    QFETCH(QString, yamlField);
    QFETCH(QVariant, yamlValue);
    QFETCH(QString, errorString);

    TemporaryDir tmp;

    createCode(tmp);
    createIconPng(tmp);
    createInfoYaml(tmp, yamlField, yamlValue);

    // check if packaging actually fails with the expected error

    QString error;
    QVERIFY(!packagerCheck(Packager::create(pathTo("test.appkg"), tmp.path()), error));
    QCOMPARE(error, errorString);
}

bool tst_PackagerTool::createInfoYaml(TemporaryDir &tmp, const QString &changeField, const QVariant &toValue)
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

    QFile infoYaml(QDir(tmp.path()).absoluteFilePath("info.yaml"));
    return infoYaml.open(QFile::WriteOnly) && infoYaml.write(yaml) == yaml.size();
}

bool tst_PackagerTool::createIconPng(TemporaryDir &tmp)
{
    QFile iconPng(QDir(tmp.path()).absoluteFilePath("icon.png"));
    return iconPng.open(QFile::WriteOnly) && iconPng.write("\x89PNG") == 4;
}

bool tst_PackagerTool::createCode(TemporaryDir &tmp)
{
    QFile code(QDir(tmp.path()).absoluteFilePath("test.qml"));
    return code.open(QFile::WriteOnly) && code.write("// test") == 7LL;
}

QTEST_GUILESS_MAIN(tst_PackagerTool)

#include "tst_packager-tool.moc"
