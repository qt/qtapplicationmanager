/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
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

class AM_EXPORT Exception : public std::exception
{
public:
    Exception(Error errorCode, const char *errorString = 0);
    Exception(Error errorCode, const QString &errorString);
    Exception(int _errno, const QString &errorString);
    Exception(const QFile &file, const QString &errorString);

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

