// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <array>
#include <QStringEncoder>

#include "colorprint.h"


QT_BEGIN_NAMESPACE_AM

ColorPrint::ColorPrint(FILE *stream, bool colorSupport)
    : m_stream(stream)
    , m_buffer(m_internal)
    , m_colorSupport(colorSupport)
{ }

ColorPrint::ColorPrint(QByteArray &buffer, bool colorSupport)
    : m_buffer(buffer)
    , m_colorSupport(colorSupport)
{ }

ColorPrint::~ColorPrint()
{
    if (m_buffer.isEmpty())
        return;

    if (m_stream) {
        if (!m_buffer.endsWith('\n'))
            m_buffer.append('\n');
        ::fputs(m_buffer.constData(), m_stream);
        ::fflush(m_stream);
    }
}

ColorPrint &ColorPrint::operator<<(const QString &str)
{
    // Don't use QString::toLocal8Bit() here, because it always allocates memory.
    // QStringEncoder together with op+=() on the other hand only allocates if necessary.
    m_buffer += QStringEncoder(QStringEncoder::System).encode(str);
    return *this;
}

ColorPrint &ColorPrint::operator<<(const QByteArray &ba)
{
    m_buffer.append(ba);
    return *this;
}

ColorPrint &ColorPrint::operator<<(char c)
{
    m_buffer.append(c);
    return *this;
}

ColorPrint &ColorPrint::operator<<(const char *str)
{
    m_buffer.append(str);
    return *this;
}

ColorPrint &ColorPrint::operator<<(int i)
{
    char tmp[12]; // 32 bit ints are at most 10 digits long, plus sign and terminating null
    qsnprintf(tmp, sizeof(tmp), "%d", i);
    m_buffer.append(tmp);
    return *this;
}

ColorPrint &ColorPrint::operator<<(bool b)
{
    (*this) << (b ? green : red) << (b ? "yes" : "no") << reset;
    return *this;
}

ColorPrint &ColorPrint::operator<<(const Command cmd)
{
    static constexpr std::array<const char *, CommandCount> cmds = {
        "\033[0m",
        "\033[31m",
        "\033[32m",
        "\033[33m",
        "\033[34m",
        "\033[35m",
        "\033[36m",
        "\033[37m",
        "\033[31;1m",
        "\033[32;1m",
        "\033[33;1m",
        "\033[34;1m",
        "\033[35;1m",
        "\033[36;1m",
        "\033[37;1m",
        "\033[41m",
        "\033[42m",
        "\033[43m",
        "\033[44m",
        "\033[45m",
        "\033[46m",
        "\033[47m",
    };

    if (m_colorSupport && (int(cmd) >= 0) && (size_t(cmd) < cmds.size()))
        m_buffer.append(cmds[cmd]);
    return *this;
}

// Operators:

ColorPrint::repeat::repeat(qsizetype count, char character)
    : m_count(count)
    , m_character(character)
{ }

void ColorPrint::repeat::operator()(ColorPrint &cp) const
{
    cp.m_buffer.append(m_count, m_character);
}

ColorPrint::subString::subString(const char *string, qsizetype size)
    : m_string(string)
    , m_size(size)
{ }

void ColorPrint::subString::operator()(ColorPrint &cp) const
{
    cp.m_buffer.append(m_string, m_size);
}

QT_END_NAMESPACE_AM
