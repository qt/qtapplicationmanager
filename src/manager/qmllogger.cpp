/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QMessageLogger>

#include "qmllogger.h"
#include "global.h"

QmlLogger::QmlLogger(QQmlEngine *engine)
    : QObject(engine)
{
    connect(engine, &QQmlEngine::warnings, this, &QmlLogger::warnings);
}

void QmlLogger::warnings(const QList<QQmlError> &list)
{
    foreach (const QQmlError &err, list) {
        QByteArray func;
        if (err.object())
            func = err.object()->objectName().toLocal8Bit();
        QByteArray file;
        if (err.url().scheme() == qL1S("file"))
            file = err.url().toLocalFile().toLocal8Bit();
        else
            file = err.url().toDisplayString().toLocal8Bit();

        QMessageLogger ml(file, err.line(), func, LogQml().categoryName());
        ml.warning().nospace() << qPrintable(err.description());
    }
}
