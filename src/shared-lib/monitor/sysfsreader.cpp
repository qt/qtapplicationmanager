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
    if (!m_fd.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        qCritical("Couldn't open file for reading: %s (%s)",
                 qPrintable(m_fd.fileName()), qPrintable(m_fd.errorString()));
    }
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
        return { };
    if (!m_fd.seek(0))
        return { };

    qsizetype offset = 0;
    qsizetype read = 0;
    do {
        read = m_fd.read(m_buffer.data() + offset, m_buffer.size() - offset);
        if (read < 0)
            return { };
        else if (read < (m_buffer.size() - offset))
            m_buffer[read + offset] = 0;
        offset += read;
    } while (read > 0 && offset < m_buffer.size());
    return m_buffer;
}

QT_END_NAMESPACE_AM
