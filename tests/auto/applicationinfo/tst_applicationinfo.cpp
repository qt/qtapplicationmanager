// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "application.h"
#include "applicationinfo.h"
#include "intent.h"
#include "intentinfo.h"
#include "package.h"
#include "packageinfo.h"
#include "yamlpackagescanner.h"
#include "exception.h"

QT_USE_NAMESPACE_AM

class tst_ApplicationInfo : public QObject
{
    Q_OBJECT

private slots:
    void full();
    void minimal();
    void inherit();
    void legacy();
    void validApplicationId_data();
    void validApplicationId();
};

QT_BEGIN_NAMESPACE_AM

class TestPackageLoader
{
public:
    TestPackageLoader(const char *name)
        : m_dataDir(qL1S(":/data"))
    {
        try {
            static YamlPackageScanner scanner;
            QString path = QString::fromLatin1(name) % qL1S("/info.yaml");

            m_pi = scanner.scan(m_dataDir.absoluteFilePath(path));
            if (m_pi) {
                m_p = new Package(m_pi);
                const auto ais = m_pi->applications();
                for (const auto &ai : ais) {
                    auto a = new Application(ai, m_p);
                    m_a << a;
                    m_p->addApplication(a);
                }
                const auto iis = m_pi->intents();
                for (const auto &ii : iis) {
                    m_i << new Intent(ii->id(), m_pi->id(), ii->handlingApplicationId(),
                                      ii->requiredCapabilities(),
                                      ii->visibility() == IntentInfo::Public ? Intent::Public
                                                                             : Intent::Private,
                                      ii->parameterMatch(), ii->names(),
                                      ii->descriptions(),
                                      QUrl::fromLocalFile(m_pi->baseDir().absoluteFilePath(ii->icon())),
                                      ii->categories());
                }
            }
        } catch (const Exception &e) {
            m_lastLoadFailure = e.errorString();
        }
    }

    ~TestPackageLoader()
    {
        delete m_p;
        qDeleteAll(m_i);
        qDeleteAll(m_a);
        delete m_pi;
    }

    PackageInfo *info() const { return m_pi; }
    Package *package() const { return m_p; }
    QVector<Intent *> intents() const { return m_i; }
    QDir dataDir() const { return m_dataDir; }
    QString lastLoadFailure() const { return m_lastLoadFailure; }

private:
    QDir m_dataDir;
    QString m_lastLoadFailure;
    PackageInfo *m_pi = nullptr;
    Package *m_p = nullptr;
    QVector<Application *> m_a;
    QVector<Intent *> m_i;
};

QT_END_NAMESPACE_AM

static QUrl qmlIcon(PackageInfo *pi, const QString &iconFile)
{
    return QUrl::fromLocalFile(pi->baseDir().absoluteFilePath(iconFile));
}

static QVariantMap qmlStringMap(const QMap<QString, QString> &sm)
{
    QVariantMap vm;
    for (auto it = sm.cbegin(); it != sm.cend(); ++it)
        vm.insert(it.key(), it.value());
    return vm;
}


void tst_ApplicationInfo::full()
{
    TestPackageLoader pl("full");
    QVERIFY2(pl.info(), qPrintable(pl.lastLoadFailure()));

    QCOMPARE(pl.info()->baseDir(), pl.dataDir().absoluteFilePath(pl.info()->id()));
    QCOMPARE(pl.info()->id(), qL1S("full"));
    QCOMPARE(pl.info()->icon(), qL1S("full.png"));
    QCOMPARE(pl.info()->version(), qL1S("v1"));
    QMap<QString, QString> names { { qL1S("en"), qL1S("pkg.name.en") }, { qL1S("de"), qL1S("pkg.name.de") } };
    QCOMPARE(pl.info()->names(), names);
    QMap<QString, QString> descs { { qL1S("en"), qL1S("pkg.desc.en") }, { qL1S("de"), qL1S("pkg.desc.de") } };
    QCOMPARE(pl.info()->descriptions(), descs);
    QStringList cats { qL1S("pkg.cat.1"), qL1S("pkg.cat.2") };
    QCOMPARE(pl.info()->categories(), cats);
    QCOMPARE(pl.info()->applications().size(), 2);
    QCOMPARE(pl.info()->intents().size(), 2);

    const ApplicationInfo *ai = pl.info()->applications().constFirst();

    QCOMPARE(ai->id(), qSL("full.app.1"));
    QCOMPARE(ai->icon(), qSL("app1.png"));
    QMap<QString, QString> app1names { { qL1S("en"), qL1S("app1.name.en") }, { qL1S("de"), qL1S("app1.name.de") } };
    QCOMPARE(ai->names(), app1names);
    QMap<QString, QString> app1descs { { qL1S("en"), qL1S("app1.desc.en") }, { qL1S("de"), qL1S("app1.desc.de") } };
    QCOMPARE(ai->descriptions(), app1descs);
    QStringList app1cats { qL1S("app1.cat.1"), qL1S("app1.cat.2") };
    QCOMPARE(ai->categories(), app1cats);
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("app1.qml"));
    QCOMPARE(ai->runtimeName(), qSL("qml"));
    QCOMPARE(ai->runtimeParameters().size(), 5);
    QCOMPARE(ai->supportsApplicationInterface(), true);
    QCOMPARE(ai->capabilities(), QStringList { qSL("app1.cap") });
    QVariantMap app1ogl { { qSL("desktopProfile"), qSL("core") }, { qSL("esMajorVersion"), 3 }, { qSL("esMinorVersion"), 2 }};
    QCOMPARE(ai->openGLConfiguration(), app1ogl);
    QVariantMap app1prop { { qSL("custom.app1.key"), 42 } };
    QCOMPARE(ai->applicationProperties(), app1prop);
    QVariantMap app1dlt { { qSL("id"), qSL("app1.dlt.id") }, { qSL("description"), qSL("app1.dlt.desc") } };
    QCOMPARE(ai->dltConfiguration(), app1dlt);

    ai = pl.info()->applications().constLast();

    QCOMPARE(ai->id(), qSL("full.app.2"));
    QCOMPARE(ai->icon(), qSL("app2.png"));
    QMap<QString, QString> app2names { { qL1S("en"), qL1S("app2.name.en") }, { qL1S("de"), qL1S("app2.name.de") } };
    QCOMPARE(ai->names(), app2names);
    QMap<QString, QString> app2descs { { qL1S("en"), qL1S("app2.desc.en") }, { qL1S("de"), qL1S("app2.desc.de") } };
    QCOMPARE(ai->descriptions(), app2descs);
    QStringList app2cats { qL1S("app2.cat.1"), qL1S("app2.cat.2") };
    QCOMPARE(ai->categories(), app2cats);
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("app2.exe"));
    QCOMPARE(ai->runtimeName(), qSL("native"));
    QCOMPARE(ai->runtimeParameters().size(), 2);

    IntentInfo *ii = pl.info()->intents().constFirst();

    QCOMPARE(ii->id(), qSL("full.int.1"));
    QCOMPARE(ii->icon(), qSL("int1.png"));
    QMap<QString, QString> int1names { { qL1S("en"), qL1S("int1.name.en") }, { qL1S("de"), qL1S("int1.name.de") } };
    QCOMPARE(ii->names(), int1names);
    QMap<QString, QString> int1descs { { qL1S("en"), qL1S("int1.desc.en") }, { qL1S("de"), qL1S("int1.desc.de") } };
    QCOMPARE(ii->descriptions(), int1descs);
    QStringList int1cats { qL1S("int1.cat.1"), qL1S("int1.cat.2") };
    QCOMPARE(ii->categories(), int1cats);
    QCOMPARE(ii->handlingApplicationId(), pl.info()->applications().constFirst()->id());
    QCOMPARE(ii->visibility(), IntentInfo::Private);
    QStringList req1caps { qSL("int1.cap.1"), qSL("int1.cap.2") };
    QCOMPARE(ii->requiredCapabilities(), req1caps);
    QVariantMap param1match { { qSL("mimeType"), qSL("^image/.*\\.png$") } };
    QCOMPARE(ii->parameterMatch(), param1match);

    ii = pl.info()->intents().constLast();

    QCOMPARE(ii->id(), qSL("full.int.2"));
    QCOMPARE(ii->icon(), qSL("int2.png"));
    QMap<QString, QString> int2names { { qL1S("en"), qL1S("int2.name.en") }, { qL1S("de"), qL1S("int2.name.de") } };
    QCOMPARE(ii->names(), int2names);
    QMap<QString, QString> int2descs { { qL1S("en"), qL1S("int2.desc.en") }, { qL1S("de"), qL1S("int2.desc.de") } };
    QCOMPARE(ii->descriptions(), int2descs);
    QStringList int2cats { qL1S("int2.cat.1"), qL1S("int2.cat.2") };
    QCOMPARE(ii->categories(), int2cats);
    QCOMPARE(ii->handlingApplicationId(), pl.info()->applications().constLast()->id());
    QCOMPARE(ii->visibility(), IntentInfo::Public);
    QStringList req2caps { qSL("int2.cap.1"), qSL("int2.cap.2") };
    QCOMPARE(ii->requiredCapabilities(), req2caps);
    QVariantMap param2match { { qSL("test"), qSL("foo") } };
    QCOMPARE(ii->parameterMatch(), param2match);

    // now the QML interface

    Package *p = pl.package();

    QVERIFY(p);
    QCOMPARE(p->id(), pl.info()->id());
    QCOMPARE(p->icon(), qmlIcon(pl.info(), pl.info()->icon()));
    QCOMPARE(p->version(), pl.info()->version());
    QVERIFY(p->names().values().contains(p->name()));
    QCOMPARE(p->names(), qmlStringMap(pl.info()->names()));
    QVERIFY(p->descriptions().values().contains(p->description()));
    QCOMPARE(p->descriptions(), qmlStringMap(pl.info()->descriptions()));
    QCOMPARE(p->categories(), pl.info()->categories());
    QCOMPARE(p->applications().size(), pl.info()->applications().size());
    QCOMPARE(pl.intents().size(), pl.info()->intents().size());

    Application *a = qobject_cast<Application *>(p->applications().last());

    QVERIFY(a);
    QCOMPARE(a->id(), ai->id());
    QCOMPARE(a->icon(), qmlIcon(pl.info(), ai->icon()));
    QVERIFY(a->names().values().contains(a->name()));
    QCOMPARE(a->names(), qmlStringMap(ai->names()));
    QVERIFY(a->descriptions().values().contains(a->description()));
    QCOMPARE(a->descriptions(), qmlStringMap(ai->descriptions()));
    QCOMPARE(a->categories(), ai->categories());
    QCOMPARE(a->supportedMimeTypes(), ai->supportedMimeTypes());
    QCOMPARE(a->capabilities(), ai->capabilities());
    QCOMPARE(a->runtimeName(), ai->runtimeName());
    QCOMPARE(a->runtimeParameters(), ai->runtimeParameters());

    Intent *i = pl.intents().last();

    QVERIFY(i);
    QCOMPARE(i->intentId(), ii->id());
    QCOMPARE(i->icon(), qmlIcon(pl.info(), ii->icon()));
    QVERIFY(i->names().values().contains(i->name()));
    QCOMPARE(i->names(), qmlStringMap(ii->names()));
    QVERIFY(i->descriptions().values().contains(i->description()));
    QCOMPARE(i->descriptions(), qmlStringMap(ii->descriptions()));
    QCOMPARE(i->categories(), ii->categories());
    QCOMPARE(int(i->visibility()), int(ii->visibility()));
    QCOMPARE(i->requiredCapabilities(), ii->requiredCapabilities());
    QCOMPARE(i->parameterMatch(), ii->parameterMatch());
}

void tst_ApplicationInfo::minimal()
{
    TestPackageLoader pl("minimal");
    QVERIFY2(pl.info(), qPrintable(pl.lastLoadFailure()));

    QCOMPARE(pl.info()->baseDir(), pl.dataDir().absoluteFilePath(pl.info()->id()));
    QCOMPARE(pl.info()->id(), qL1S("minimal"));
    QCOMPARE(pl.info()->icon(), QString { });
    QCOMPARE(pl.info()->version(), QString { });
    QMap<QString, QString> emptyMap;
    QCOMPARE(pl.info()->names(), emptyMap);
    QCOMPARE(pl.info()->descriptions(), emptyMap);
    QCOMPARE(pl.info()->categories(), QStringList { });
    QCOMPARE(pl.info()->applications().size(), 1);
    QCOMPARE(pl.info()->intents().size(), 0);

    const ApplicationInfo *ai = pl.info()->applications().constFirst();

    QCOMPARE(ai->id(), qSL("minimal.app"));
    QCOMPARE(ai->icon(), pl.info()->icon());
    QCOMPARE(ai->names(), pl.info()->names());
    QCOMPARE(ai->descriptions(), pl.info()->descriptions());
    QCOMPARE(ai->categories(), pl.info()->categories());
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("minimal.qml"));
    QCOMPARE(ai->runtimeName(), qSL("qml"));
    QCOMPARE(ai->runtimeParameters().size(), 0);
}

void tst_ApplicationInfo::inherit()
{
    TestPackageLoader pl("inherit");
    QVERIFY2(pl.info(), qPrintable(pl.lastLoadFailure()));

    QCOMPARE(pl.info()->baseDir(), pl.dataDir().absoluteFilePath(pl.info()->id()));
    QCOMPARE(pl.info()->id(), qL1S("inherit"));
    QCOMPARE(pl.info()->icon(), qL1S("pkg.png"));
    QCOMPARE(pl.info()->version(), QString { });
    QMap<QString, QString> names { { qL1S("en"), qL1S("pkg.name.en") }, { qL1S("de"), qL1S("pkg.name.de") } };
    QCOMPARE(pl.info()->names(), names);
    QMap<QString, QString> descs { { qL1S("en"), qL1S("pkg.desc.en") }, { qL1S("de"), qL1S("pkg.desc.de") } };
    QCOMPARE(pl.info()->descriptions(), descs);
    QStringList cats { qL1S("pkg.cat.1"), qL1S("pkg.cat.2") };
    QCOMPARE(pl.info()->categories(), cats);
    QCOMPARE(pl.info()->applications().size(), 2);
    QCOMPARE(pl.info()->intents().size(), 2);

    const ApplicationInfo *ai = pl.info()->applications().constFirst();

    QCOMPARE(ai->id(), qSL("inherit.app.1"));
    QCOMPARE(ai->icon(), pl.info()->icon());
    QCOMPARE(ai->names(), pl.info()->names());
    QCOMPARE(ai->descriptions(), pl.info()->descriptions());
    QCOMPARE(ai->categories(), pl.info()->categories());
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("app1.qml"));
    QCOMPARE(ai->runtimeName(), qSL("qml"));
    QCOMPARE(ai->runtimeParameters().size(), 0);

    ai = pl.info()->applications().constLast();

    QCOMPARE(ai->id(), qSL("inherit.app.2"));
    QCOMPARE(ai->icon(), pl.info()->icon());
    QCOMPARE(ai->names(), pl.info()->names());
    QCOMPARE(ai->descriptions(), pl.info()->descriptions());
    QCOMPARE(ai->categories(), pl.info()->categories());
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("app2.exe"));
    QCOMPARE(ai->runtimeName(), qSL("native"));
    QCOMPARE(ai->runtimeParameters().size(), 0);

    IntentInfo *ii = pl.info()->intents().constFirst();

    QCOMPARE(ii->id(), qSL("inherit.int.1"));
    QCOMPARE(ii->icon(), pl.info()->icon());
    QCOMPARE(ii->names(), pl.info()->names());
    QCOMPARE(ii->descriptions(), pl.info()->descriptions());
    QCOMPARE(ii->categories(), pl.info()->categories());
    QCOMPARE(ii->handlingApplicationId(), pl.info()->applications().constFirst()->id());

    ii = pl.info()->intents().constLast();

    QCOMPARE(ii->id(), qSL("inherit.int.2"));
    QCOMPARE(ii->icon(), pl.info()->icon());
    QCOMPARE(ii->names(), pl.info()->names());
    QCOMPARE(ii->descriptions(), pl.info()->descriptions());
    QCOMPARE(ii->categories(), pl.info()->categories());
    QCOMPARE(ii->handlingApplicationId(), pl.info()->applications().constLast()->id());

    // now the QML interface

    Package *p = pl.package();

    QVERIFY(p);
    QVERIFY(!p->name().isEmpty() && (p->name() != p->id()));
    QCOMPARE(p->names(), qmlStringMap(pl.info()->names()));
    QVERIFY(!p->description().isEmpty());
    QCOMPARE(p->descriptions(), qmlStringMap(pl.info()->descriptions()));
    QCOMPARE(p->categories(), pl.info()->categories());

    Application *a = qobject_cast<Application *>(p->applications().first());

    QVERIFY(a);
    QCOMPARE(a->icon(), p->icon());
    QCOMPARE(a->name(), p->name());
    QCOMPARE(a->description(), p->description());
    QCOMPARE(a->names(), p->names());
    QCOMPARE(a->descriptions(), p->descriptions());

    Intent *i = pl.intents().first();

    QVERIFY(i);
    QCOMPARE(i->icon(), p->icon());
    QCOMPARE(i->name(), p->name());
    QCOMPARE(i->description(), p->description());
    QCOMPARE(i->names(), p->names());
    QCOMPARE(i->descriptions(), p->descriptions());
}

void tst_ApplicationInfo::legacy()
{
    TestPackageLoader pl("legacy");
    QVERIFY2(pl.info(), qPrintable(pl.lastLoadFailure()));

    QCOMPARE(pl.info()->baseDir(), pl.dataDir().absoluteFilePath(pl.info()->id()));
    QCOMPARE(pl.info()->id(), qL1S("legacy"));
    QCOMPARE(pl.info()->icon(), qL1S("icon.png"));
    QCOMPARE(pl.info()->version(), qL1S("v1"));
    QMap<QString, QString> names { { qL1S("en"), qL1S("legacy.en") }, { qL1S("de"), qL1S("legacy.de") } };
    QCOMPARE(pl.info()->names(), names);
    QStringList cats { qL1S("bar"), qL1S("foo") };
    QVERIFY(pl.info()->descriptions().isEmpty());
    QCOMPARE(pl.info()->categories(), cats);
    QCOMPARE(pl.info()->applications().size(), 1);
    QCOMPARE(pl.info()->intents().size(), 0);

    const ApplicationInfo *ai = pl.info()->applications().constFirst();

    QCOMPARE(ai->id(), pl.info()->id());
    QCOMPARE(ai->icon(), pl.info()->icon());
    QCOMPARE(ai->names(), pl.info()->names());
    QVERIFY(ai->descriptions().isEmpty());
    QCOMPARE(ai->categories(), pl.info()->categories());
    QStringList mimes { qL1S("text/plain"), qL1S("x-scheme-handler/mailto") };
    QCOMPARE(ai->supportedMimeTypes(), mimes);
    QStringList caps { qL1S("cameraAccess"), qL1S("locationAccess") };
    QCOMPARE(ai->capabilities(), caps);
    QCOMPARE(QFileInfo(ai->codeFilePath()).fileName(), qSL("legacy.qml"));
    QCOMPARE(ai->runtimeName(), qSL("qml"));
    QCOMPARE(ai->runtimeParameters().size(), 1);
    QCOMPARE(ai->runtimeParameters().value(qSL("loadDummyData")).toBool(), true);
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

QTEST_APPLESS_MAIN(tst_ApplicationInfo)

#include "tst_applicationinfo.moc"
