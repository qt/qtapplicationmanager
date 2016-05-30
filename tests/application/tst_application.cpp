/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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


class tst_Application : public QObject
{
    Q_OBJECT

public:
    tst_Application();

private slots:
    void initTestCase();
    void database();
    void application_data();
    void application();

private:
    QVector<const Application *> apps;
};

tst_Application::tst_Application()
{ }


void tst_Application::initTestCase()
{
    YamlApplicationScanner scanner;
    QDir baseDir(qL1S(AM_TESTDATA_DIR "manifests"));
    foreach (const QString &appDirName, baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        QDir dir = baseDir.absoluteFilePath(appDirName);
        QString error;
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
    QCOMPARE(app->isBuiltIn(), true);
    QCOMPARE(app->isPreloaded(), true);
    QCOMPARE(app->importance(), 0.5);
    QVERIFY(app->backgroundMode() == Application::TracksLocation);
    QCOMPARE(app->supportedMimeTypes().size(), 2);
    QCOMPARE(app->supportedMimeTypes().first(), qSL("text/plain"));
    QCOMPARE(app->supportedMimeTypes().last(), qSL("x-scheme-handler/mailto"));
    QCOMPARE(app->capabilities().size(), 2);
    QCOMPARE(app->capabilities().first(), qSL("cameraAccess"));
    QCOMPARE(app->capabilities().last(), qSL("locationAccess"));
    QCOMPARE(app->categories().size(), 2);
    QCOMPARE(app->categories().first(), qSL("bar"));
    QCOMPARE(app->categories().last(), qSL("foo"));

    QVERIFY(!app->currentRuntime());
    QVERIFY(!app->isLocked());
    QVERIFY(app->lock());
    QVERIFY(!app->lock());
    QVERIFY(app->isLocked());
    QVERIFY(app->unlock());
    QVERIFY(!app->unlock());
    QVERIFY(!app->isLocked());
    QVERIFY(app->state() == Application::Installed);
    QCOMPARE(app->progress(), qreal(0));

    delete app;
}


QTEST_APPLESS_MAIN(tst_Application)

#include "tst_application.moc"

