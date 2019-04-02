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

#include "global.h"
#include "application.h"
#include "applicationinfo.h"
#include "packageinfo.h"
#include "yamlpackagescanner.h"
#include "exception.h"

QT_USE_NAMESPACE_AM

class tst_ApplicationInfo : public QObject
{
    Q_OBJECT

public:
    tst_ApplicationInfo();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void application_data();
    void application();
    void validApplicationId_data();
    void validApplicationId();
    void validIcon_data();
    void validIcon();

private:
    QVector<PackageInfo *> m_pkgs;
};

tst_ApplicationInfo::tst_ApplicationInfo()
{ }


void tst_ApplicationInfo::initTestCase()
{
    YamlPackageScanner scanner;
    QDir baseDir(qL1S(AM_TESTDATA_DIR "manifests"));
    const QStringList pkgDirNames = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &pkgDirName : pkgDirNames) {
        QDir dir = baseDir.absoluteFilePath(pkgDirName);
        try {
            PackageInfo *pi = scanner.scan(dir.absoluteFilePath(qSL("info.yaml")));
            QVERIFY(pi);
            QCOMPARE(pkgDirName, pi->id());
            m_pkgs << pi;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }

    QCOMPARE(m_pkgs.size(), 2);
}

void tst_ApplicationInfo::cleanupTestCase()
{
    qDeleteAll(m_pkgs);
}

void tst_ApplicationInfo::application_data()
{
    QTest::addColumn<QString>("id");

    QTest::newRow("normal") << "com.pelagicore.test";
    QTest::newRow("json") << "com.pelagicore.json-legacy";
}

void tst_ApplicationInfo::application()
{
    QFETCH(QString, id);

    QString name = QString::fromLatin1(AM_TESTDATA_DIR "manifests/%1/info.yaml").arg(id);

    YamlPackageScanner scanner;
    PackageInfo *pkg;
    try {
        pkg = scanner.scan(name);
        QVERIFY(pkg);
    } catch (const std::exception &e) {
        QFAIL(e.what());
    }

    QCOMPARE(pkg->id(), id);
    QCOMPARE(QFileInfo(pkg->icon()).fileName(), qSL("icon.png"));
    QCOMPARE(pkg->names().size(), 2);
    QCOMPARE(pkg->names().value(qSL("en")), qSL("english"));
    QCOMPARE(pkg->names().value(qSL("de")), qSL("deutsch"));
    QCOMPARE(pkg->name(qSL("en")), qSL("english"));
    QCOMPARE(pkg->isBuiltIn(), false);
    QCOMPARE(pkg->categories().size(), 2);
    QVERIFY(pkg->categories().startsWith(qSL("bar")));
    QVERIFY(pkg->categories().endsWith(qSL("foo")));

    ApplicationInfo *app = pkg->applications().size() == 1 ? pkg->applications().first() : nullptr;
    QVERIFY(app);
    QCOMPARE(QFileInfo(app->codeFilePath()).fileName(), qSL("Test.qml"));
    QCOMPARE(app->runtimeName(), qSL("qml"));
    QCOMPARE(app->runtimeParameters().size(), 1);
    QCOMPARE(app->runtimeParameters().value(qSL("loadDummyData")).toBool(), true);
    QCOMPARE(app->capabilities().size(), 2);
    QVERIFY(app->capabilities().startsWith(qSL("cameraAccess")));
    QVERIFY(app->capabilities().endsWith(qSL("locationAccess")));

    // legacy
    QCOMPARE(app->supportedMimeTypes().size(), 2);
    QVERIFY(app->supportedMimeTypes().startsWith(qSL("text/plain")));
    QVERIFY(app->supportedMimeTypes().endsWith(qSL("x-scheme-handler/mailto")));

    delete pkg;
}

void tst_ApplicationInfo::validApplicationId_data()
{
    QTest::addColumn<QString>("appId");
    QTest::addColumn<bool>("valid");

    // passes
    QTest::newRow("normal") << "Test" << true;
    QTest::newRow("shortest") << "t" << true;
    QTest::newRow("valid-chars") << "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.';[]{}!#$%^&()-_=+@" << true;
    QTest::newRow("longest-name") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.test" << true;

    // failures
    QTest::newRow("empty") << "" << false;
    QTest::newRow("space-only") << " " << false;
    QTest::newRow("space-only2") << "  " << false;
    QTest::newRow("name-too-long") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.xtest" << false;
    QTest::newRow("invalid-char<") << "t<" << false;
    QTest::newRow("invalid-char>") << "t>" << false;
    QTest::newRow("invalid-char:") << "t:" << false;
    QTest::newRow("invalid-char-quote") << "t\"" << false;
    QTest::newRow("invalid-char/") << "t/" << false;
    QTest::newRow("invalid-char\\") << "t\\" << false;
    QTest::newRow("invalid-char|") << "t|" << false;
    QTest::newRow("invalid-char?") << "t?" << false;
    QTest::newRow("invalid-char*") << "t*" << false;
    QTest::newRow("control-char") << "t\t" << false;
}

void tst_ApplicationInfo::validApplicationId()
{
    QFETCH(QString, appId);
    QFETCH(bool, valid);

    QString errorString;
    bool result = PackageInfo::isValidApplicationId(appId, &errorString);

    QVERIFY2(valid == result, qPrintable(errorString));
}

void tst_ApplicationInfo::validIcon_data()
{
    QTest::addColumn<QString>("icon");
    QTest::addColumn<bool>("isValid");

    // valid
    QTest::newRow("'icon.png' is valid") << "icon.png" << true;
    QTest::newRow("'foo.bar' is valid") << "foo.bar" << true;
    QTest::newRow("'foo' is valid") << "foo" << true;

    // invalid
    QTest::newRow("empty is invalid") << "" << false;
    QTest::newRow("'foo/icon.png' is invalid") << "foo/icon.png" << false;
    QTest::newRow("'../icon.png' is invalid") << "../icon.png" << false;
    QTest::newRow("'icon.png/' is invalid") << "icon.png/" << false;
    QTest::newRow("'/foo' is invalid") << "/foo" << false;
}

void tst_ApplicationInfo::validIcon()
{
    QFETCH(QString, icon);
    QFETCH(bool, isValid);

    QString errorString;
    bool result = PackageInfo::isValidIcon(icon, &errorString);

    QCOMPARE(result, isValid);
}

QTEST_APPLESS_MAIN(tst_ApplicationInfo)

#include "tst_applicationinfo.moc"

