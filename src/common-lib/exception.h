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

#pragma once

#include <QString>
#include <QDir>
#include <QByteArray>
#include <QScopedPointer>
#include <QException>

#include <exception>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QFile)

QT_BEGIN_NAMESPACE_AM

class Exception : public QException
{
public:
    explicit Exception(const char *errorString) Q_DECL_NOEXCEPT;
    explicit Exception(const QString &errorString) Q_DECL_NOEXCEPT;
    explicit Exception(Error errorCode, const char *errorString = nullptr) Q_DECL_NOEXCEPT;
    explicit Exception(Error errorCode, const QString &errorString) Q_DECL_NOEXCEPT;
    explicit Exception(int _errno, const char *errorString) Q_DECL_NOEXCEPT;
    explicit Exception(const QFile &file, const char *errorString) Q_DECL_NOEXCEPT;

    Exception(const Exception &copy) Q_DECL_NOEXCEPT;
    Exception(Exception &&move) Q_DECL_NOEXCEPT;

    ~Exception() Q_DECL_NOEXCEPT;

    Error errorCode() const Q_DECL_NOEXCEPT;
    QString errorString() const Q_DECL_NOEXCEPT;

    void raise() const override;
    Exception *clone() const Q_DECL_NOEXCEPT override;

    // convenience
    Exception &arg(const QByteArray &fileName) Q_DECL_NOEXCEPT
    {
        m_errorString = m_errorString.arg(QString::fromLocal8Bit(fileName));
        return *this;
    }
    Exception &arg(const QDir &dir) Q_DECL_NOEXCEPT
    {
        m_errorString = m_errorString.arg(dir.path());
        return *this;
    }

    // this will generate compiler errors if there's no suitable QString::arg(const C &) overload
    template <typename C> typename std::enable_if<QtPrivate::IsSequentialContainer<C>::Value, Exception>::type &
    arg(const C &c) Q_DECL_NOEXCEPT
    {
        QString s;
        for (int i = 0; i < c.size(); ++i) {
            s += qSL("%1").arg(c.at(i));
            if (i < (c.size() - 1))
                s.append(qL1S(", "));
        }
        m_errorString = m_errorString.arg(s);
        return *this;
    }

    Exception &arg(const char *str) Q_DECL_NOEXCEPT
    {
        m_errorString = m_errorString.arg(QString::fromUtf8(str));
        return *this;
    }

    // this will generate compiler errors if there's no suitable QString::arg(const Ts &) overload
    template <typename... Ts> Exception &arg(const Ts & ...ts) Q_DECL_NOEXCEPT
    {
        m_errorString = m_errorString.arg(ts...);
        return *this;
    }

    // shouldn't be used, but needed for std::exception compatibility
    const char *what() const Q_DECL_NOEXCEPT override;

protected:
    Error m_errorCode;
    QString m_errorString;

private:
    mutable QByteArray *m_whatBuffer = nullptr;
};

QT_END_NAMESPACE_AM
