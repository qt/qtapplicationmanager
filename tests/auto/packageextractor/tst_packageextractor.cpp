// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <memory>

#include <QtTest>
#include <QtNetwork>
#include <QTemporaryDir>
#include <qplatformdefs.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#endif

#include "global.h"
#include "packageextractor.h"
#include "installationreport.h"
#include "packageutilities.h"
#include "utilities.h"

#include "../error-checking.h"

QT_USE_NAMESPACE_AM

class tst_PackageExtractor : public QObject
{
    Q_OBJECT

public:
    tst_PackageExtractor();

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void extractAndVerify_data();
    void extractAndVerify();

    void cancelExtraction();

    void extractFromFifo();

private:
    QString m_taest;
    std::unique_ptr<QTemporaryDir> m_extractDir;
};

tst_PackageExtractor::tst_PackageExtractor()
    : m_taest(QString::fromUtf8("t\xc3\xa4st"))
{ }

void tst_PackageExtractor::initTestCase()
{
    if (!QDir(qL1S(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    QVERIFY(PackageUtilities::checkCorrectLocale());
}

void tst_PackageExtractor::init()
{
    m_extractDir.reset(new QTemporaryDir());
    QVERIFY(m_extractDir->isValid());
}

void tst_PackageExtractor::cleanup()
{
    m_extractDir.reset();
}

void tst_PackageExtractor::extractAndVerify_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("expectedSuccess");
    QTest::addColumn<QString>("errorString");
    QTest::addColumn<QStringList>("entries");
    QTest::addColumn<QMap<QString, QByteArray>>("content");
    QTest::addColumn<QMap<QString, qint64>>("sizes");

    QStringList noEntries;
    QMap<QString, QByteArray> noContent;
    QMap<QString, qint64> noSizes;

    QTest::newRow("normal") << "packages/test.appkg"
                            << true << ""
                            << QStringList {
                                   qSL("info.yaml"),
                                   qSL("icon.png"),
                                   qSL("test"),
                                   m_taest }
                            << QMap<QString, QByteArray> {
                                   { qSL("test"), "test\n" },
                                   { m_taest, "test with umlaut\n" } }
                            << noSizes;

    QTest::newRow("big") << "packages/bigtest.appkg"
                         << true << ""
                         << QStringList {
                                qSL("info.yaml"),
                                qSL("icon.png"),
                                qSL("test"),
                                m_taest,
                                qSL("bigtest") }
                         << QMap<QString, QByteArray> {
                                { qSL("test"), "test\n" },
                                { m_taest, "test with umlaut\n" } }
                         << QMap<QString, qint64> {
                                // { "info.yaml", 213 }, // this is different on Windows: \n vs. \r\n
                                { qSL("icon.png"), 1157 },
                                { qSL("bigtest"), 5*1024*1024 },
                                { qSL("test"), 5 },
                                { m_taest, 17 } };

    QTest::newRow("invalid-url")    << "packages/no-such-file.appkg"
                                    << false << "~Error opening .*: (No such file or directory|The system cannot find the file specified\\.)"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-format") << "packages/test-invalid-format.appkg"
                                    << false << "~.* could not open archive: Unrecognized archive format"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-digest") << "packages/test-invalid-footer-digest.appkg"
                                    << false << "~package digest mismatch.*"
                                    << noEntries << noContent << noSizes;
    QTest::newRow("invalid-path")   << "packages/test-invalid-path.appkg"
                                    << false << "~invalid archive entry .*: pointing outside of extraction directory"
                                    << noEntries << noContent << noSizes;
}

void tst_PackageExtractor::extractAndVerify()
{
    // macros are stupid...
    typedef QMap<QString, QByteArray> ByteArrayMap;
    typedef QMap<QString, qint64> IntMap;

    QFETCH(QString, path);
    QFETCH(bool, expectedSuccess);
    QFETCH(QString, errorString);
    QFETCH(QStringList, entries);
    QFETCH(ByteArrayMap, content);
    QFETCH(IntMap, sizes);

    PackageExtractor extractor(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR) + path), m_extractDir->path());
    bool result = extractor.extract();

    if (expectedSuccess) {
        QVERIFY2(result, qPrintable(extractor.errorString()));
    } else {
        QVERIFY(extractor.errorCode() != Error::None);
        QVERIFY(extractor.errorCode() != Error::Canceled);
        QVERIFY(!extractor.wasCanceled());

        QT_AM_CHECK_ERRORSTRING(extractor.errorString(), errorString);
        return;
    }

    QStringList checkEntries(entries);
    QDirIterator it(m_extractDir->path(), QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString entry = it.next();
        entry = entry.mid(m_extractDir->path().size() + 1);

        QVERIFY2(checkEntries.contains(entry), qPrintable(entry));

        if (content.contains(entry)) {
            QVERIFY(QDir(m_extractDir->path()).exists(entry));
            QFile f(QDir(m_extractDir->path()).absoluteFilePath(entry));
            QVERIFY(f.open(QFile::ReadOnly));
            QCOMPARE(f.readAll(), content.value(entry));
        }

        if (sizes.contains(entry)) {
            QVERIFY(QDir(m_extractDir->path()).exists(entry));
            QFile f(QDir(m_extractDir->path()).absoluteFilePath(entry));
            QCOMPARE(f.size(), sizes.value(entry));
        }

        QVERIFY(checkEntries.removeOne(entry));
    }

    QVERIFY2(checkEntries.isEmpty(), qPrintable(checkEntries.join(qL1C(' '))));

    QStringList reportEntries = extractor.installationReport().files();
    reportEntries.sort();
    entries.sort();
    QCOMPARE(reportEntries, entries);
}

void tst_PackageExtractor::cancelExtraction()
{
    {
        PackageExtractor extractor(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test.appkg")), m_extractDir->path());
        extractor.cancel();
        QVERIFY(!extractor.extract());
        QVERIFY(extractor.wasCanceled());
        QVERIFY(extractor.errorCode() == Error::Canceled);
        QVERIFY(extractor.hasFailed());
    }
    {
        PackageExtractor extractor(QUrl::fromLocalFile(qL1S(AM_TESTDATA_DIR "packages/test.appkg")), m_extractDir->path());
        connect(&extractor, &PackageExtractor::progress, this, [&extractor](qreal p) {
            if (p >= 0.1)
                extractor.cancel();
        });
        QVERIFY(!extractor.extract());
        QVERIFY(extractor.wasCanceled());
        QVERIFY(extractor.errorCode() == Error::Canceled);
        QVERIFY(extractor.hasFailed());
    }
}

class FifoSource : public QThread // clazy:exclude=missing-qobject-macro
{
public:
    FifoSource(const QString &file)
        : m_file(file)
    {
        m_fifoPath = QDir::temp().absoluteFilePath(qSL("autotext-package-extractor-%1.fifo"))
                .arg(QCoreApplication::applicationPid())
                .toLocal8Bit();
#ifdef Q_OS_UNIX
        QVERIFY2(m_file.open(QFile::ReadOnly), qPrintable(m_file.errorString()));
        QVERIFY2(::mkfifo(m_fifoPath, 0600) == 0, ::strerror(errno));
#endif
    }

    ~FifoSource()
    {
#ifdef Q_OS_UNIX
        ::unlink(m_fifoPath);
#endif
    }

    QString path() const
    {
        return QString::fromLocal8Bit(m_fifoPath);
    }

    void run() override
    {
#ifdef Q_OS_UNIX
        int fifoFd = QT_OPEN(m_fifoPath, O_WRONLY);
        QVERIFY2(fifoFd >= 0, ::strerror(errno));

        QByteArray buffer;
        buffer.resize(1024 * 1024);

        while (!m_file.atEnd()) {
            qint64 bytesRead = m_file.read(buffer.data(), buffer.size());
            QVERIFY(bytesRead >= 0);
            qint64 bytesWritten = QT_WRITE(fifoFd, buffer.constData(), bytesRead);
            QCOMPARE(bytesRead, bytesWritten);
        }
        QT_CLOSE(fifoFd);
#endif
    }

private:
    QFile m_file;
    QByteArray m_fifoPath;
};

void tst_PackageExtractor::extractFromFifo()
{
#if !defined(Q_OS_UNIX)
    QSKIP("No FIFO support on this platform");
#endif

    FifoSource fifo(qL1S(AM_TESTDATA_DIR "packages/test.appkg"));
    fifo.start();

    PackageExtractor extractor(QUrl::fromLocalFile(fifo.path()), m_extractDir->path());
    QVERIFY2(extractor.extract(), qPrintable(extractor.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(fifo.isFinished(), 5000 * timeoutFactor());
}

int main(int argc, char *argv[])
{
    PackageUtilities::ensureCorrectLocale();
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    tst_PackageExtractor tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_packageextractor.moc"
