// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QException>

#include <exception>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QFile)

QT_BEGIN_NAMESPACE_AM

class Exception : public QException   // clazy:exclude=copyable-polymorphic
{
public:
    explicit Exception(const char *errorString) noexcept;
    explicit Exception(const QString &errorString) noexcept;
    explicit Exception(Error errorCode, const char *errorString = nullptr) noexcept;
    explicit Exception(Error errorCode, const QString &errorString) noexcept;
    explicit Exception(int _errno, const char *errorString) noexcept;
    explicit Exception(const QFileDevice &file, const char *errorString) noexcept;

    Exception(const Exception &copy) noexcept;
    Exception &operator=(const Exception &copy) noexcept;
    Exception(Exception &&move) noexcept;
    Exception &operator=(Exception &&move) noexcept;

    ~Exception() noexcept override;

    Error errorCode() const noexcept;
    QString errorString() const noexcept;

    void raise() const override;
    Exception *clone() const noexcept override;

    // convenience
    Exception &arg(const QByteArray &fileName) noexcept
    {
        m_errorString = m_errorString.arg(QString::fromLocal8Bit(fileName));
        return *this;
    }
    Exception &arg(const QDir &dir) noexcept
    {
        m_errorString = m_errorString.arg(dir.path());
        return *this;
    }

    // this will generate compiler errors if there's no suitable QString::arg(const C &) overload
    template <typename C> typename std::enable_if<QtPrivate::IsSequentialContainer<C>::Value, Exception>::type &
    arg(const C &c) noexcept
    {
        QString s;
        for (int i = 0; i < c.size(); ++i) {
            s += QStringLiteral("%1").arg(c.at(i));
            if (i < (c.size() - 1))
                s.append(u", ");
        }
        m_errorString = m_errorString.arg(s);
        return *this;
    }

    Exception &arg(const char *str) noexcept
    {
        m_errorString = m_errorString.arg(QString::fromUtf8(str));
        return *this;
    }

    // this will generate compiler errors if there's no suitable QString::arg(const Ts &) overload
    template <typename... Ts> Exception &arg(const Ts & ...ts) noexcept
    {
        m_errorString = m_errorString.arg(ts...); // AXIVION Line Qt-QStringArg: Axivion thinks ts is an int
        return *this;
    }

    // shouldn't be used, but needed for std::exception compatibility
    const char *what() const noexcept override;

protected:
    Error m_errorCode;
    QString m_errorString;

private:
    mutable QByteArray *m_whatBuffer = nullptr;
};

QT_END_NAMESPACE_AM
