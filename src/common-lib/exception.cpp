/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QFile>
#include <errno.h>

#include "exception.h"

QT_BEGIN_NAMESPACE_AM

Exception::Exception(const char *errorString) Q_DECL_NOEXCEPT
    : m_errorCode(Error::System)
    , m_errorString(errorString ? qL1S(errorString) : QString())
{ }

Exception::Exception(const QString &errorString) Q_DECL_NOEXCEPT
    : m_errorCode(Error::System)
    , m_errorString(errorString)
{ }

Exception::Exception(Error errorCode, const char *errorString) Q_DECL_NOEXCEPT
    : m_errorCode(errorCode)
    , m_errorString(errorString ? qL1S(errorString) : QString())
{ }

Exception::Exception(Error errorCode, const QString &errorString) Q_DECL_NOEXCEPT
    : m_errorCode(errorCode)
    , m_errorString(errorString)
{ }

Exception::Exception(int _errno, const char *errorString) Q_DECL_NOEXCEPT
    : m_errorCode(_errno == EACCES ? Error::Permissions : Error::IO)
    , m_errorString(qL1S(errorString) + qSL(": ") + QString::fromLocal8Bit(strerror(_errno)))
{ }

Exception::Exception(const QFile &file, const char *errorString) Q_DECL_NOEXCEPT
    : m_errorCode(file.error() == QFile::PermissionsError ? Error::Permissions : Error::IO)
    , m_errorString(qL1S(errorString) + qSL(" (") + file.fileName() + qSL("): ") + file.errorString())
{ }

Exception::Exception(const Exception &copy) Q_DECL_NOEXCEPT
    : m_errorCode(copy.m_errorCode)
    , m_errorString(copy.m_errorString)
{ }

Exception::Exception(Exception &&move) Q_DECL_NOEXCEPT
    : m_errorCode(move.m_errorCode)
    , m_errorString(move.m_errorString)
{
    qSwap(m_whatBuffer, move.m_whatBuffer);
}

Exception::~Exception() Q_DECL_NOEXCEPT
{
    delete m_whatBuffer;
}

Error Exception::errorCode() const Q_DECL_NOEXCEPT
{
    return m_errorCode;
}

QString Exception::errorString() const Q_DECL_NOEXCEPT
{
    return m_errorString;
}

void Exception::raise() const
{
    throw *this;
}

Exception *Exception::clone() const Q_DECL_NOEXCEPT
{
    return new Exception(*this);
}

const char *Exception::what() const Q_DECL_NOEXCEPT
{
    if (!m_whatBuffer)
        m_whatBuffer = new QByteArray;
    *m_whatBuffer = m_errorString.toLocal8Bit();
    return m_whatBuffer->constData();
}

QT_END_NAMESPACE_AM
