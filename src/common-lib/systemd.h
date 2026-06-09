// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSTEMD_H
#define SYSTEMD_H

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtAppManCommon/qtappmancommonglobal.h>

QT_BEGIN_NAMESPACE_AM

class SystemdPrivate;

class Q_APPMANCOMMON_EXPORT Systemd
{
public:
    Q_DISABLE_COPY_MOVE(Systemd)

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

    // Parse the contents of a systemd-style EnvironmentFile (see systemd.exec(5))
    static QMap<QString, QString> parseEnvironmentFile(const QString &contents);

private:
    Systemd();

    std::unique_ptr<SystemdPrivate> d;

    friend class SystemdTest;
};

QT_END_NAMESPACE_AM

#endif // SYSTEMD_H
