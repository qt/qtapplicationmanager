/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <errno.h>
#include <qplatformdefs.h>
#include "sysfsreader.h"

#  define EINTR_LOOP(cmd) __extension__ ({int res = 0; do { res = cmd; } while (res == -1 && errno == EINTR); res; })

static inline int qt_safe_open(const char *pathname, int flags, mode_t mode = 0777)
{
    flags |= O_CLOEXEC;
    int fd = EINTR_LOOP(QT_OPEN(pathname, flags, mode));
    // unknown flags are ignored, so we have no way of verifying if
    // O_CLOEXEC was accepted
    if (fd != -1)
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}
#  undef QT_OPEN
#  define QT_OPEN         qt_safe_open

QT_BEGIN_NAMESPACE_AM

SysFsReader::SysFsReader(const QByteArray &path, int maxRead)
    : m_path(path)
{
    m_buffer.resize(maxRead);
    m_fd = QT_OPEN(m_path, QT_OPEN_RDONLY);
}

SysFsReader::~SysFsReader()
{
    if (m_fd >= 0)
        QT_CLOSE(m_fd);
}

bool SysFsReader::isOpen() const
{
    return m_fd >= 0;
}

QByteArray SysFsReader::fileName() const
{
    return m_path;
}

QByteArray SysFsReader::readValue() const
{
    if (m_fd < 0)
        return QByteArray();
    if (EINTR_LOOP(QT_LSEEK(m_fd, 0, SEEK_SET)) != QT_OFF_T(0))
        return QByteArray();

    int offset = 0;
    int read = 0;
    do {
        read = EINTR_LOOP(QT_READ(m_fd, m_buffer.data() + offset, m_buffer.size() - offset));
        if (read < 0)
            return QByteArray();
        else if (read < (m_buffer.size() - offset))
            m_buffer[read + offset] = 0;
        offset += read;
    } while (read > 0 && offset < m_buffer.size());
    return m_buffer;
}

QT_END_NAMESPACE_AM
