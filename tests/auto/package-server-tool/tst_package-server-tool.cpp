// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>
#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QStringBuilder>

#if defined(Q_OS_LINUX)
#  include <sys/prctl.h>
#  include <signal.h>
#endif

#include "utilities.h"


using namespace Qt::StringLiterals;

static QString findAppManTool(const QString &toolName)
{
    static const QStringList possibleLocations = {
        QCoreApplication::applicationDirPath() + u"/../../../bin"_s,
        QLibraryInfo::path(QLibraryInfo::BinariesPath)
    };

    for (const QString &possibleLocation : possibleLocations) {
        QFileInfo fi(possibleLocation + u'/' + toolName);
        if (fi.exists() && fi.isExecutable())
            return fi.absoluteFilePath();
    }
    return { };
}

class tst_PackageServerTool : public QObject
{
    Q_OBJECT

public:
    tst_PackageServerTool();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void httpInterface();

private:
    QTemporaryDir m_tmpDir;
    QProcess m_psProcess;
    QUrl m_url;
    QNetworkAccessManager m_nam;
};


tst_PackageServerTool::tst_PackageServerTool()
{ }

void tst_PackageServerTool::initTestCase()
{
#if !defined(Q_OS_LINUX)
    QSKIP("This test is only supported on Linux");
#endif

    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    auto verbose = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
    qInfo() << "Verbose mode is" << (verbose ? "on" : "off") << "(change by (un)setting $AM_VERBOSE_TEST)";

    const QString psTool = findAppManTool(u"appman-package-server"_s);
    QVERIFY(!psTool.isEmpty());

    QVERIFY(m_tmpDir.isValid());
    QVERIFY(QDir(m_tmpDir.path()).mkpath(u".packages"_s));
    QVERIFY(QFile::copy(QString::fromLatin1(AM_TESTDATA_DIR "/packages/test.appkg"),
                        m_tmpDir.path() + u"/.packages/com.pelagicore.test_all.ampkg"_s));

    m_psProcess.setProgram(psTool);
    QStringList args = { u"--dd"_s, m_tmpDir.path() };
    m_psProcess.setArguments(args);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"AM_FORCE_COLOR_OUTPUT"_s, u"off"_s);
    m_psProcess.setProcessEnvironment(env);

#if defined(Q_OS_LINUX)
    m_psProcess.setChildProcessModifier([]() {
        // at least on Linux we can make sure that the package-server is always killed
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
    });
#endif
    m_psProcess.start();

    QVERIFY(m_psProcess.waitForStarted());
    QVERIFY(m_psProcess.waitForReadyRead());
    QTest::qWait(500 * QtAM::timeoutFactor());

    const auto err = QString::fromLocal8Bit(m_psProcess.readAllStandardError());
    QCOMPARE(err, QString { });
    const auto outLines = QString::fromLocal8Bit(m_psProcess.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);

    const QStringList expectedLines = {
        u"Qt ApplicationManager Package Server"_s,
        u"> Data directory: "_s + m_tmpDir.path(),
        u"> Project: PROJECT"_s,
        u"> Verify developer signature on upload: no"_s,
        u"> Add store signature on download: no"_s,
        u"> Scanning .packages"_s,
        u" + adding   com.pelagicore.test [all]"_s,
        u"> HTTP server listening on: "_s // + <ip>:<port>
    };

    QCOMPARE(outLines.size(), expectedLines.size());
    auto sameLines = expectedLines.size() - 1;
    QCOMPARE(outLines.first(sameLines), expectedLines.first(sameLines));
    QVERIFY(outLines.last().startsWith(expectedLines.last()));
    m_url = u"http://"_s + outLines.last().mid(expectedLines.last().length());
    QVERIFY(m_url.isValid());
    qInfo() << "packager-server URL:" << m_url.toString();
}

void tst_PackageServerTool::cleanupTestCase()
{
    if (m_psProcess.state() != QProcess::NotRunning) {
        m_psProcess.terminate();
        QVERIFY(m_psProcess.waitForFinished());
    }
}

void tst_PackageServerTool::httpInterface()
{
    // we cannot use test data here, as the steps have to be done in order and most
    // cannot be repeated

    static QByteArray iconPng;
    static QByteArray testAmpkg;

    if (iconPng.isEmpty()) {
        QFile f(QString::fromLatin1(AM_TESTDATA_DIR "/icon.png"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        iconPng = f.readAll();
        QVERIFY(!iconPng.isEmpty());
    }

    if (testAmpkg.isEmpty()) {
        QFile f(QString::fromLatin1(AM_TESTDATA_DIR "/packages/test.appkg"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        testAmpkg = f.readAll();
        QVERIFY(!testAmpkg.isEmpty());
    }

    static const auto Get = QNetworkAccessManager::GetOperation;
    static const auto Post = QNetworkAccessManager::PostOperation;
    static const auto Put = QNetworkAccessManager::PutOperation;

    std::vector<std::tuple<QByteArray, QByteArray, QUrlQuery, QNetworkAccessManager::Operation, int, QVariant>> testCases = {

        { "hello-no-project", "/hello", QUrlQuery(), Get, 200,
         QJsonObject { { u"status"_s, u"incompatible-project-id"_s } } },

        { "hello", "/hello", QUrlQuery({ { u"project-id"_s, u"PROJECT"_s } }), Get, 200,
         QJsonObject { { u"status"_s, u"ok"_s } } },

        { "categories", "/category/list", QUrlQuery(), Get, 200,
         QJsonArray { u"test-category"_s } },

        { "packages", "/package/list", QUrlQuery(), Get, 200,
         QJsonArray { QJsonObject {
             { u"architecture"_s, u""_s },
             { u"id"_s, u"com.pelagicore.test"_s },
             { u"categories"_s, QJsonArray { u"test-category"_s } },
             { u"iconUrl"_s, u"package/icon?id=com.pelagicore.test"_s },
             { u"names"_s, QJsonObject { { u"de"_s, u"Hallo"_s }, { u"en"_s, u"Hello"_s } } },
             { u"descriptions"_s, QJsonObject { } },
             { u"version"_s, u"1.0"_s },
        } } },

        { "no-icon", "/package/icon", QUrlQuery(), Get, 404,
         QByteArray { } },

        { "icon", "/package/icon", QUrlQuery { { u"id"_s, u"com.pelagicore.test"_s} }, Get, 200,
         iconPng },

        { "no-download", "/package/download", QUrlQuery(), Get, 404,
         QByteArray { } },

        { "download", "/package/download", QUrlQuery { { u"id"_s, u"com.pelagicore.test"_s} }, Get, 200,
         testAmpkg },

        { "remove-non-existent", "/package/remove", QUrlQuery({ { u"id"_s, u"com.pelagicore.test"_s }, { u"architecture"_s, u"x86_64"_s } }), Post, 200,
         QJsonObject { { u"status"_s, u"fail"_s }, { u"removed"_s, 0 } } },

        { "remove", "/package/remove", QUrlQuery({ { u"id"_s, u"com.pelagicore.test"_s } }), Post, 200,
         QJsonObject { { u"status"_s, u"ok"_s }, { u"removed"_s, 1 } } },

        { "no-categories", "/category/list", QUrlQuery(), Get, 200,
         QJsonArray { } },

        { "no-packages", "/package/list", QUrlQuery(), Get, 200,
         QJsonArray { } },

        { "upload", "/package/upload", QUrlQuery(), Put, 200,
         QJsonObject {
             { u"status"_s, u"ok"_s },
             { u"result"_s, u"added"_s },
             { u"id"_s, u"com.pelagicore.test"_s },
             { u"architecture"_s, u"all"_s },
        } },

        { "upload-again", "/package/upload", QUrlQuery(), Put, 200,
         QJsonObject {
             { u"status"_s, u"ok"_s },
             { u"result"_s, u"no changes"_s },
             { u"id"_s, u"com.pelagicore.test"_s },
             { u"architecture"_s, u"all"_s },
        } },

    };

    for (const auto &testCase : testCases) {
        const auto &name = std::get<0>(testCase);
        const auto &path = std::get<1>(testCase);
        const auto &query = std::get<2>(testCase);
        const auto &operation = std::get<3>(testCase);
        const auto &expectedStatusCode = std::get<4>(testCase);
        const auto &expectedReply = std::get<5>(testCase);

        qInfo() << "tag:" << name.constData();

        auto url = m_url;
        url.setPath(QString::fromLatin1(path));
        url.setQuery(query);
        QNetworkRequest req(url);
        QNetworkReply *reply = nullptr;
        switch (operation) {
        case QNetworkAccessManager::GetOperation:
            reply = m_nam.get(req);
            break;
        case QNetworkAccessManager::PostOperation:
            req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
            reply = m_nam.post(req, QByteArray());
            break;
        case QNetworkAccessManager::PutOperation:
            reply = m_nam.put(req, testAmpkg); // hardcoded to keep the tuple small
            break;
        default:
            break;
        }
        QVERIFY(reply);
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 3000 * QtAM::timeoutFactor());
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), expectedStatusCode);

        switch (expectedReply.userType()) {
        case QMetaType::QJsonArray:
        case QMetaType::QJsonObject: {
            auto json = QJsonDocument::fromJson(reply->readAll());
            QVERIFY(!json.isNull());
            QJsonDocument expectedJson;
            if (expectedReply.userType() == QMetaType::QJsonArray)
                expectedJson.setArray(expectedReply.toJsonArray());
            else
                expectedJson.setObject(expectedReply.toJsonObject());
            QCOMPARE(json, expectedJson);
            break;
        }
        case QMetaType::QByteArray:
            QCOMPARE(reply->readAll(), expectedReply.toByteArray());
            break;
        default:
            break;
        }
    }
}

QTEST_GUILESS_MAIN(tst_PackageServerTool)

#include "tst_package-server-tool.moc"
