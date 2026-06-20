// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef TOOLRUNNER_H
#define TOOLRUNNER_H

#include <initializer_list>
#include <memory>

#include <QtTest>
#include <QByteArray>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include "utilities.h"

QT_USE_NAMESPACE_AM

// Base class for running one of the appman command-line tools as a subprocess and capturing its
// result and outputs.
class ToolRunner
{
public:
    // locate an installed appman tool binary (e.g. "appman-controller"), or return an empty string
    static QString findTool(const QString &toolName)
    {
        static const QStringList possibleLocations = {
            QCoreApplication::applicationDirPath() + QStringLiteral("/../../../bin"),
            QLibraryInfo::path(QLibraryInfo::BinariesPath)
        };

        QString toolPath = u'/' + toolName;
#if defined(Q_OS_WIN)
        toolPath += u".exe";
#endif
        for (const QString &possibleLocation : possibleLocations) {
            QFileInfo fi(possibleLocation + toolPath);
            if (fi.exists() && fi.isExecutable())
                return fi.absoluteFilePath();
        }
        qWarning() << "Could not find" << toolName << "in any of the following locations:"
                   << possibleLocations;
        return { };
    }

    int exitCode = 0;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QByteArray stdOut;
    QStringList stdOutList;
    QByteArray stdErr;
    QStringList stdErrList;
    QByteArray failure;

    bool call()
    {
        return start() && waitForFinished();
    }

    bool start()
    {
        if (m_started)
            return false;

        m_proc.reset(new QProcess);
        m_spy.reset(new QSignalSpy(m_proc.get(), &QProcess::finished));
        m_proc->setProgram(m_program);
        m_proc->setArguments(m_arguments);
        m_proc->start();

        if (!m_proc->waitForStarted()) {
            failure = "could not start " + m_name;
            return false;
        }
        return m_started = true;
    }

    bool waitForFinished()
    {
        if (!m_started)
            return false;

        if (m_proc->state() == QProcess::Running) {
            m_spy->wait(5000 * timeoutFactor());
            if (m_proc->state() != QProcess::NotRunning) {
                failure = m_name + " did not exit";
                return false;
            }
        }

        exitCode = m_proc->exitCode();
        exitStatus = m_proc->exitStatus();
        stdOut = m_proc->readAllStandardOutput();
        stdOutList = QString::fromLocal8Bit(stdOut).split(u'\n', Qt::SkipEmptyParts);
        stdErr = m_proc->readAllStandardError();
        stdErr.replace("QML debugging is enabled. Only use this in a safe environment.\n", "");
        stdErrList = QString::fromLocal8Bit(stdErr).split(u'\n', Qt::SkipEmptyParts);

        if (exitStatus == QProcess::CrashExit)
            failure = m_name + " crashed, signal: " + QByteArray::number(exitCode);
        else if (exitCode != 0)
            failure = m_name + " returned an error code: " + QByteArray::number(exitCode);

        // enable for debugging
        // if (!failure.isEmpty()) {  qWarning() << "STDOUT" << stdOut << "\nSTDERR" << stdErr; }

        m_started = false;
        return failure.isEmpty();
    }

protected:
    ToolRunner(const QByteArray &name, const QString &program, const QStringList &arguments)
        : m_name(name)
        , m_program(program)
        , m_arguments(arguments)
    { }

private:
    QByteArray m_name;
    QString m_program;
    QStringList m_arguments;
    bool m_started = false;
    std::unique_ptr<QSignalSpy> m_spy;
    std::unique_ptr<QProcess> m_proc;
};

#endif // TOOLRUNNER_H
