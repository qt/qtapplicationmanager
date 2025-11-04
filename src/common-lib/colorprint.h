// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef COLORPRINT_H
#define COLORPRINT_H

#include <cstdio>
#include <QtCore/QByteArray>
#include <QtAppManCommon/global.h>


QT_BEGIN_NAMESPACE_AM

class ColorPrint
{
public:
    explicit ColorPrint(FILE *stream, bool colorSupport);
    explicit ColorPrint(QByteArray &buffer, bool colorSupport);
    ~ColorPrint();

    ColorPrint &operator<<(const QString &str);
    ColorPrint &operator<<(const QByteArray &ba);
    ColorPrint &operator<<(char c);
    ColorPrint &operator<<(const char *str);
    ColorPrint &operator<<(int i);
    ColorPrint &operator<<(bool b);

    enum Command {
        reset = 0,

        red,
        green,
        yellow,
        blue,
        magenta,
        cyan,
        white,

        bred,
        bgreen,
        byellow,
        bblue,
        bmagenta,
        bcyan,
        bwhite,

        redBg,
        greenBg,
        yellowBg,
        blueBg,
        magentaBg,
        cyanBg,
        whiteBg,

        CommandCount
    };
    ColorPrint &operator<<(const Command cmd);

    // anything we can invoke with a ColorPrint & parameter is a valid operator<< source
    template <typename OP>
    typename std::enable_if<std::is_invocable_v<OP, ColorPrint &>, ColorPrint &>::type
    operator<<(const OP &op)
    {
        std::invoke(op, *this);
        return *this;
    }

    struct repeat
    {
        explicit repeat(qsizetype count, char character);
        void operator()(ColorPrint &cp) const;

    private:
        qsizetype m_count;
        char m_character;
    };

    struct subString
    {
        explicit subString(const char *string, qsizetype size);
        void operator()(ColorPrint &cp) const;

    private:
        const char *m_string;
        qsizetype m_size;
    };

private:
    FILE *m_stream = nullptr;
    QByteArray m_internal;
    QByteArray &m_buffer;
    bool m_colorSupport;

    Q_DISABLE_COPY_MOVE(ColorPrint)
};

QT_END_NAMESPACE_AM

#endif // COLORPRINT_H
