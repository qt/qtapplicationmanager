// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSTEMD_P_H
#define SYSTEMD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QReadWriteLock>

#include "systemd.h"
#include "unix-utilities.h"

QT_BEGIN_NAMESPACE_AM

class SystemdPrivate
{
public:
    QByteArray notifySocket;
    QByteArray watchdogUsec;
    QByteArray watchdogPid;
    QByteArray listenFds;
    QByteArray listenFdNames;
    QByteArray listenPid;
    QByteArray journalStream;

#if defined(Q_OS_UNIX)
    Unix::Fd notifySocketFd;
#endif
    bool notifySocketTriedToConnect = false;

    QReadWriteLock extraJournalFieldsLock;
    QMap<QByteArray, QByteArray> extraJournalFields;
    QByteArray extraJournalFieldsBuffer;
    bool extraJournalFieldsHasSyslogIdentifier = false;

#if defined(QT_BUILD_INTERNAL) && defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Auto-test hook: redirect logToJournal() away from /run/systemd/journal/socket onto a socket
    // the test controls. An empty path restores the default. (@ prefix selects an abstract socket)
    // Returns false (and changes nothing) if the path does not fit into a struct sockaddr_un.
    Q_AUTOTEST_EXPORT static bool setJournalSocketPathForTesting(const QByteArray &path);
    static QByteArray s_journalSocketPathForTesting;
#endif
};

QT_END_NAMESPACE_AM

#endif // SYSTEMD_P_H
