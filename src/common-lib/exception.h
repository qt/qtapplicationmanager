/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QString>
#include <QDir>
#include <QByteArray>
#include <QScopedPointer>

#include <exception>
#include "error.h"
#include "global.h"

QT_FORWARD_DECLARE_CLASS(QFile)

AM_BEGIN_NAMESPACE

class AM_EXPORT Exception : public std::exception
{
public:
    explicit Exception(Error errorCode, const char *errorString = 0);
    explicit Exception(Error errorCode, const QString &errorString);
    explicit Exception(int _errno, const char *errorString);
    explicit Exception(const QFile &file, const char *errorString);

    Exception(const Exception &copy);
    Exception(Exception &&move);

    ~Exception() throw();

    Error errorCode() const;
    QString errorString() const;

    // convenience
    Exception &arg(const QByteArray &fileName)
    {
        m_errorString = m_errorString.arg(QString::fromLocal8Bit(fileName));
        return *this;
    }
    Exception &arg(const QDir &dir)
    {
        m_errorString = m_errorString.arg(dir.path());
        return *this;
    }

    template <typename... Ts> Exception &arg(const Ts & ...ts)
    {
        m_errorString = m_errorString.arg(ts...);
        return *this;
    }

    // shouldn't be used, but needed for std::exception compatibility
    const char *what() const throw();

protected:
    Error m_errorCode;
    QString m_errorString;

private:
    mutable QByteArray *m_whatBuffer = 0;
};

AM_END_NAMESPACE
