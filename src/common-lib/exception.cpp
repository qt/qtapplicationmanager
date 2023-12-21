// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <errno.h>

#include "exception.h"

QT_BEGIN_NAMESPACE_AM

Exception::Exception(const char *errorString) noexcept
    : m_errorCode(Error::System)
    , m_errorString(errorString ? QString::fromLatin1(errorString) : QString())
{ }

Exception::Exception(const QString &errorString) noexcept
    : m_errorCode(Error::System)
    , m_errorString(errorString)
{ }

Exception::Exception(Error errorCode, const char *errorString) noexcept
    : m_errorCode(errorCode)
    , m_errorString(errorString ? QString::fromLatin1(errorString) : QString())
{ }

Exception::Exception(Error errorCode, const QString &errorString) noexcept
    : m_errorCode(errorCode)
    , m_errorString(errorString)
{ }

Exception::Exception(int _errno, const char *errorString) noexcept
    : m_errorCode(_errno == EACCES ? Error::Permissions : Error::IO)
    , m_errorString(QString::fromLatin1(errorString) + u": " + QString::fromLocal8Bit(strerror(_errno)))
{ }

Exception::Exception(const QFileDevice &file, const char *errorString) noexcept
    : m_errorCode(file.error() == QFileDevice::PermissionsError ? Error::Permissions : Error::IO)
    , m_errorString(QString::fromLatin1(errorString) + u" (" + file.fileName() + u"): " + file.errorString())
{ }

Exception::Exception(const Exception &copy) noexcept
    : m_errorCode(copy.m_errorCode)
    , m_errorString(copy.m_errorString)
{ }

Exception::Exception(Exception &&move) noexcept
    : m_errorCode(move.m_errorCode)
    , m_errorString(move.m_errorString)
{
    std::swap(m_whatBuffer, move.m_whatBuffer);
}

Exception::~Exception() noexcept
{
    delete m_whatBuffer;
}

Error Exception::errorCode() const noexcept
{
    return m_errorCode;
}

QString Exception::errorString() const noexcept
{
    return m_errorString;
}

void Exception::raise() const
{
    throw *this;
}

Exception *Exception::clone() const noexcept
{
    return new Exception(*this);
}

const char *Exception::what() const noexcept
{
    if (!m_whatBuffer)
        m_whatBuffer = new QByteArray;
    *m_whatBuffer = m_errorString.toLocal8Bit();
    return m_whatBuffer->constData();
}

QT_END_NAMESPACE_AM
