// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QByteArray>
#include <QFile>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class SysFsReader
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

    Q_DISABLE_COPY(SysFsReader)
};

QT_END_NAMESPACE_AM
