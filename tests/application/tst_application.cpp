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

#include "global.h"
#include "application.h"
#include "applicationdatabase.h"
#include "yamlapplicationscanner.h"
#include "exception.h"

QT_USE_NAMESPACE_AM

class tst_Application : public QObject
{
    Q_OBJECT

public:
    tst_Application();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void database();
    void application_data();
    void application();
    void validApplicationId_data();
    void validApplicationId();

private:
    QVector<const Application *> apps;
};

tst_Application::tst_Application()
{ }


void tst_Application::initTestCase()
{
    YamlApplicationScanner scanner;
    QDir baseDir(qL1S(AM_TESTDATA_DIR "manifests"));
    const QStringList appDirNames = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &appDirName : appDirNames) {
        QDir dir = baseDir.absoluteFilePath(appDirName);
        try {
            const Application *a = scanner.scan(dir.absoluteFilePath(qSL("info.yaml")));
            QVERIFY(a);
            QCOMPARE(appDirName, a->id());
            apps << a;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }

    QCOMPARE(apps.size(), 2);
}

void tst_Application::cleanupTestCase()
{
    qDeleteAll(apps);
}

void tst_Application::database()
{
    QString tmpDbPath = QDir::temp().absoluteFilePath(qSL("autotest-appdb-%1").arg(qApp->applicationPid()));

    QFile::remove(tmpDbPath);
    QVERIFY(!QFile::exists(tmpDbPath));

    {
        ApplicationDatabase adb(QDir::tempPath());
        QVERIFY(!adb.isValid());
    }

    {
        ApplicationDatabase adb(tmpDbPath);
        QVERIFY(adb.isValid());

        try {
            QVector<const Application *> appsInDb = adb.read();
            QVERIFY(appsInDb.isEmpty());

            adb.write(apps);
            qDeleteAll(appsInDb);
        } catch (const Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    QVERIFY(QFileInfo(tmpDbPath).size() > 0);

    {
        ApplicationDatabase adb(tmpDbPath);
        QVERIFY(adb.isValid());

        try {
            QVector<const Application *> appsInDb = adb.read();
            QCOMPARE(appsInDb.size(), apps.size());
            qDeleteAll(appsInDb);
        } catch (Exception &e) {
            QVERIFY2(false, e.what());
        }
    }

    {
#if defined(Q_OS_WIN)
        QString nullDb(qSL("\\\\.\\NUL"));
#else
        QString nullDb(qSL("/dev/zero"));
#endif

        ApplicationDatabase adb(nullDb);
        QVERIFY2(adb.isValid(), qPrintable(adb.errorString()));

        try {
            adb.write(apps);
            QVERIFY(false);
        } catch (const Exception &) {
        }
    }

    /*{
        ApplicationDatabase adb("/proc/self/sched");
        QVERIFY(adb.isValid());

        try {
    //        adb.write(apps);
    //        QVERIFY(false);
        } catch (Exception &) {
        }
    }*/

}
void tst_Application::application_data()
{
    QTest::addColumn<QString>("id");

    QTest::newRow("normal") << "com.pelagicore.test";
    QTest::newRow("json") << "com.pelagicore.json-legacy";
}

void tst_Application::application()
{
    QFETCH(QString, id);

    QString name = QString::fromLatin1(AM_TESTDATA_DIR "manifests/%1/info.yaml").arg(id);

    YamlApplicationScanner scanner;
    Application *app;
    try {
        app = scanner.scan(name);
        QVERIFY(app);
    } catch (const std::exception &e) {
        QFAIL(e.what());
    }

    QCOMPARE(app->id(), id);
    QCOMPARE(QFileInfo(app->icon()).fileName(), qSL("icon.png"));
    QCOMPARE(app->names().size(), 2);
    QCOMPARE(app->names().value(qSL("en")), qSL("english"));
    QCOMPARE(app->names().value(qSL("de")), qSL("deutsch"));
    QCOMPARE(app->name(qSL("en")), qSL("english"));
    QCOMPARE(QFileInfo(app->codeFilePath()).fileName(), qSL("Test.qml"));
    QCOMPARE(app->runtimeName(), qSL("qml"));
    QCOMPARE(app->runtimeParameters().size(), 1);
    QCOMPARE(app->runtimeParameters().value(qSL("loadDummyData")).toBool(), true);
    QCOMPARE(app->isBuiltIn(), false);
    QCOMPARE(app->isPreloaded(), true);
    QCOMPARE(app->importance(), 0.5);
    QVERIFY(app->backgroundMode() == Application::TracksLocation);
    QCOMPARE(app->supportedMimeTypes().size(), 2);
    QVERIFY(app->supportedMimeTypes().startsWith(qSL("text/plain")));
    QVERIFY(app->supportedMimeTypes().endsWith(qSL("x-scheme-handler/mailto")));
    QCOMPARE(app->capabilities().size(), 2);
    QVERIFY(app->capabilities().startsWith(qSL("cameraAccess")));
    QVERIFY(app->capabilities().endsWith(qSL("locationAccess")));
    QCOMPARE(app->categories().size(), 2);
    QVERIFY(app->categories().startsWith(qSL("bar")));
    QVERIFY(app->categories().endsWith(qSL("foo")));

    QVERIFY(!app->currentRuntime());
    QVERIFY(!app->isBlocked());
    QVERIFY(app->block());
    QVERIFY(!app->block());
    QVERIFY(app->isBlocked());
    QVERIFY(app->unblock());
    QVERIFY(!app->unblock());
    QVERIFY(!app->isBlocked());
    QVERIFY(app->state() == Application::Installed);
    QCOMPARE(app->progress(), qreal(0));

    delete app;
}

void tst_Application::validApplicationId_data()
{
    QTest::addColumn<QString>("appId");
    QTest::addColumn<bool>("isAlias");
    QTest::addColumn<bool>("valid");

    // passes
    QTest::newRow("normal") << "Test" << false << true;
    QTest::newRow("shortest") << "t" << false << true;
    QTest::newRow("valid-chars") << "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.';[]{}!#$%^&()-_=+" << false << true;
    QTest::newRow("longest-name") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.test" << false << true;
    QTest::newRow("alias-normal") << "Test@alias" << true << true;
    QTest::newRow("alias-shortest") << "t@a" << true << true;
    QTest::newRow("alias-valid-chars") << "1-2@1-a" << true << true;
    QTest::newRow("alias-longest-part") << "com.012345678901234567890123456789012345678901234567890123456789012.test@012345678901234567890123456789012345678901234567890123456789012" << true << true;
    QTest::newRow("alias-longest-name") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012@test" << true << true;
    QTest::newRow("alias-max-part-cnt") << "a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.0.1.2.3.4.5.6.7.8.9.a.0@12" << true << true;

    // failures
    QTest::newRow("empty") << "" << false << false;
    QTest::newRow("space-only") << " " << false << false;
    QTest::newRow("space-only2") << "  " << false << false;
    QTest::newRow("name-too-long") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012.xtest" << false << false;
    QTest::newRow("empty-alias") << "test@" << true << false;
    QTest::newRow("invalid-char@") << "t@" << false << false;
    QTest::newRow("invalid-char<") << "t<" << false << false;
    QTest::newRow("invalid-char>") << "t>" << false << false;
    QTest::newRow("invalid-char:") << "t:" << false << false;
    QTest::newRow("invalid-char-quote") << "t\"" << false << false;
    QTest::newRow("invalid-char/") << "t/" << false << false;
    QTest::newRow("invalid-char\\") << "t\\" << false << false;
    QTest::newRow("invalid-char|") << "t|" << false << false;
    QTest::newRow("invalid-char?") << "t?" << false << false;
    QTest::newRow("invalid-char*") << "t*" << false << false;
    QTest::newRow("control-char") << "t\t" << false << false;
    QTest::newRow("unicode-char") << QString::fromUtf8("c.p.t@c\xc3\xb6m") << true << false;
    QTest::newRow("no-alias") << "t" << true << false;
    QTest::newRow("alias-name-too-long") << "com.012345678901234567890123456789012345678901234567890123456789012.012345678901234567890123456789012345678901234567890123456789012.0123456789012@xtest" << true << false;
}

void tst_Application::validApplicationId()
{
    QFETCH(QString, appId);
    QFETCH(bool, isAlias);
    QFETCH(bool, valid);

    QString errorString;
    bool result = Application::isValidApplicationId(appId, isAlias, &errorString);

    QVERIFY2(valid == result, qPrintable(errorString));
}

QTEST_APPLESS_MAIN(tst_Application)

#include "tst_application.moc"

