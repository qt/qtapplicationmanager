// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2020 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QPointer>
#include <QDir>
#include <QRegularExpression>
#include <QAbstractEventDispatcher>
#include <QProcess>
#include <QCoreApplication>
#include <private/qtestlog_p.h>
#include "amtest.h"
#include "utilities.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM


AmTest::AmTest()
{}

AmTest *AmTest::instance()
{
    static QPointer<AmTest> object = new AmTest;
    if (!object) {
        qWarning("A new appman test object has been created, the behavior may be compromised");
        object = new AmTest;
    }
    return object;
}

int AmTest::timeoutFactor() const
{
    return QtAM::timeoutFactor();
}

QVariant AmTest::buildConfig() const
{
    return qApp->property("_am_buildConfig");
}

QString AmTest::qtVersion() const
{
    return QString::fromLatin1(QT_VERSION_STR);
}

static QtMsgType convertMsgType(AmTest::MsgType type)
{
    QtMsgType ret;

    switch (type) {
    case AmTest::WarningMsg: ret = QtWarningMsg; break;
    case AmTest::CriticalMsg: ret = QtCriticalMsg; break;
    case AmTest::FatalMsg: ret = QtFatalMsg; break;
    case AmTest::InfoMsg: ret = QtInfoMsg; break;
    default: ret = QtDebugMsg;
    }
    return ret;
}

void AmTest::ignoreMessage(MsgType type, const char *msg)
{
    QTestLog::ignoreMessage(convertMsgType(type), msg);
}

void AmTest::ignoreMessage(MsgType type, const QRegularExpression &expression)
{
    QTestLog::ignoreMessage(convertMsgType(type), expression);
}

int AmTest::observeObjectDestroyed(QObject *obj)
{
    static int idx = 0;
    int index = idx++;

    connect(obj, &QObject::destroyed,
            this, [this, index] () {
        emit objectDestroyed(index);
    });

    return index;
}

void AmTest::aboutToBlock()
{
    emit QAbstractEventDispatcher::instance()->aboutToBlock();
}

bool AmTest::dirExists(const QString &dir)
{
    return QDir(dir).exists();
}

QVariantMap AmTest::runProgram(const QStringList &commandLine)
{
    QVariantMap result {
        { u"stdout"_s, QString() },
        { u"stderr"_s, QString() },
        { u"exitCode"_s, -1 },
        { u"error"_s, QString() },
    };

    if (commandLine.isEmpty()) {
        result[u"error"_s] = u"no command"_s;
    } else {
#if QT_CONFIG(process)
        QProcess process;
        process.start(commandLine[0], commandLine.mid(1));
        if (!process.waitForStarted(5000 * timeoutFactor())) {
            result[u"error"_s] = u"could not start process"_s;
        } else {
            if (!process.waitForFinished(5000 * timeoutFactor()))
                result[u"error"_s] = u"process did not exit"_s;
            else
                result[u"exitCode"_s] = process.exitCode();

            result[u"stdout"_s] = QString::fromLocal8Bit(process.readAllStandardOutput());
            result[u"stderr"_s] = QString::fromLocal8Bit(process.readAllStandardError());
        }
#else
        result[u"error"_s] = u"runProgram is not available in this build"_s;
#endif
    }
    return result;
}

QT_END_NAMESPACE_AM

#include "moc_amtest.cpp"
