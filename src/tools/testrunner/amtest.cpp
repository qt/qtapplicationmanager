/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2020 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QPointer>
#include <QDir>
#include <QRegularExpression>
#include <QAbstractEventDispatcher>
#if defined(Q_OS_LINUX)
#  include <QTextStream>
#  include <QProcess>
#endif
#include <private/qtestlog_p.h>
#include "amtest.h"
#include "utilities.h"


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
    return QT_PREPEND_NAMESPACE_AM(timeoutFactor)();
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

    connect(obj, &QObject::destroyed, [this, index] () {
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

#if defined(Q_OS_LINUX)
QString AmTest::ps(int pid)
{
    QProcess process;
    process.start(qSL("ps"), QStringList{qSL("--no-headers"), QString::number(pid)});
    if (process.waitForFinished(5000)) {
        QString str = QString::fromLocal8Bit(process.readAllStandardOutput());
        str.chop(1);
        return str;
    }
    return QString();
}

QString AmTest::cmdLine(int pid)
{
    QFile file(QString::fromUtf8("/proc/%1/cmdline").arg(pid));
    return file.open(QFile::ReadOnly) ? QString::fromLocal8Bit(file.readLine()) : QString();
}

QString AmTest::environment(int pid)
{
    QFile file(QString::fromUtf8("/proc/%1/environ").arg(pid));
    return file.open(QFile::ReadOnly) ? QTextStream(&file).readAll() : QString();
}

int AmTest::findChildProcess(int ppid, const QString &substr)
{
    QProcess process;
    process.start(qSL("ps"), QStringList{qSL("--ppid"), QString::number(ppid), qSL("-o"),
                                         qSL("pid,args"), qSL("--no-headers")});
    if (process.waitForFinished(5000)) {
        const QString str = QString::fromLocal8Bit(process.readAllStandardOutput());
        QRegularExpression re(qSL(" *(\\d*) .*") + substr);
        QRegularExpressionMatch match = re.match(str);
        if (match.hasMatch())
            return match.captured(1).toInt();
    }
    return 0;
}
#endif  // Q_OS_LINIX

QT_END_NAMESPACE_AM

#include "moc_amtest.cpp"
