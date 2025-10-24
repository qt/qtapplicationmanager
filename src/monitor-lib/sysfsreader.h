// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSFSREADER_H
#define SYSFSREADER_H

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtAppManMonitor/qtappmanmonitorglobal.h>

QT_BEGIN_NAMESPACE_AM

class Q_APPMANMONITOR_EXPORT SysFsReader
{
public:
    SysFsReader(const QByteArray &path, int maxRead = 2048);
    ~SysFsReader();
    bool isOpen() const;
    QByteArray fileName() const;
    QByteArray readValue() const;

private:
    mutable QFile m_fd; // seek() is non-const
    QByteArray m_path;
    mutable QByteArray m_buffer;

    Q_DISABLE_COPY_MOVE(SysFsReader)
};

QT_END_NAMESPACE_AM

#endif // SYSFSREADER_H
