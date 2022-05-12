/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

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
    std::swap(m_whatBuffer, move.m_whatBuffer);
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
