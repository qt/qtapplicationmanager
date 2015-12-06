/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QFile>
#include <errno.h>

#include "exception.h"


Exception::Exception(Error errorCode, const char *errorString)
    : m_errorCode(errorCode)
    , m_errorString(errorString ? qL1S(errorString) : QString())
{ }

Exception::Exception(Error errorCode, const QString &errorString)
    : m_errorCode(errorCode)
    , m_errorString(errorString)
{ }

Exception::Exception(int _errno, const char *errorString)
    : m_errorCode(_errno == EACCES ? Error::Permissions : Error::IO)
    , m_errorString(qL1S(errorString) + qSL(": ") + QString::fromLocal8Bit(strerror(_errno)))
{ }

Exception::Exception(const QFile &file, const char *errorString)
    : m_errorCode(file.error() == QFile::PermissionsError ? Error::Permissions : Error::IO)
    , m_errorString(qL1S(errorString) + qSL(" (") + file.fileName() + qSL("): ") + file.errorString())
{ }

Exception::Exception(const Exception &copy)
    : m_errorCode(copy.m_errorCode)
    , m_errorString(copy.m_errorString)
{ }

Exception::Exception(Exception &&move)
    : m_errorCode(move.m_errorCode)
    , m_errorString(move.m_errorString)
    , m_whatBuffer(move.m_whatBuffer)
{ }

Exception::~Exception() throw()
{
    delete m_whatBuffer;
}

Error Exception::errorCode() const
{
    return m_errorCode;
}

QString Exception::errorString() const
{
    return m_errorString;
}

const char *Exception::what() const throw()
{
    if (!m_whatBuffer)
        m_whatBuffer = new QByteArray(m_errorString.toLocal8Bit());
    return *m_whatBuffer;
}
