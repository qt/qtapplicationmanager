// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "sysfsreader.h"


QT_BEGIN_NAMESPACE_AM

SysFsReader::SysFsReader(const QByteArray &path, int maxRead)
    : m_path(path)
{
    m_buffer.resize(maxRead);
    m_fd.setFileName(QString::fromLocal8Bit(path));
    m_fd.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

SysFsReader::~SysFsReader()
{ }

bool SysFsReader::isOpen() const
{
    return m_fd.isOpen();
}

QByteArray SysFsReader::fileName() const
{
    return m_path;
}

QByteArray SysFsReader::readValue() const
{
    if (!m_fd.isOpen())
        return QByteArray();
    if (!m_fd.seek(0))
        return QByteArray();

    int offset = 0;
    int read = 0;
    do {
        read = m_fd.read(m_buffer.data() + offset, m_buffer.size() - offset);
        if (read < 0)
            return QByteArray();
        else if (read < (m_buffer.size() - offset))
            m_buffer[read + offset] = 0;
        offset += read;
    } while (read > 0 && offset < m_buffer.size());
    return m_buffer;
}

QT_END_NAMESPACE_AM
