// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSTEMD_H
#define SYSTEMD_H

#include <chrono>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QReadWriteLock>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Systemd
{
public:
    static Systemd *instance();
    ~Systemd();

    bool notify(const QString &state);

    std::optional<std::chrono::milliseconds> watchdogTimeout(bool ignorePid = false);

    QMap<int, QString> listenFds(const QRegularExpression &nameRx, bool ignorePid = false);

    bool canLogToJournal() const;
    bool logToJournal(QtMsgType msgType, const QMessageLogContext &context, const QString &message,
                      QByteArray &tmpBuffer);

    QMap<QByteArray, QByteArray> extraJournalFields();
    void setExtraJournalFields(const QMap<QByteArray, QByteArray> &fields) noexcept(false);

private:
    Systemd();
    Systemd(const Systemd &) = delete;
    Systemd(Systemd &&) = delete;
    Systemd &operator=(const Systemd &) = delete;
    Systemd &operator=(Systemd &&) = delete;

    bool checkPid(const QByteArray &pidVar);

    QByteArray m_notifySocket;
    QByteArray m_watchdogUsec;
    QByteArray m_watchdogPid;
    QByteArray m_listenFds;
    QByteArray m_listenFdNames;
    QByteArray m_listenPid;
    QByteArray m_journalStream;

    int m_notifySocketFd = -1;
    bool m_notifySocketTriedToConnect = false;

    QReadWriteLock m_extraJournalFieldsLock;
    QMap<QByteArray, QByteArray> m_extraJournalFields;
    QByteArray m_extraJournalFieldsBuffer;

    friend class SystemdTest;
};

QT_END_NAMESPACE_AM

#endif // SYSTEMD_H
