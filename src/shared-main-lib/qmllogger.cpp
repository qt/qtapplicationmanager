// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QMessageLogger>

#include "qmllogger.h"
#include "global.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

QmlLogger::QmlLogger(QQmlEngine *engine)
    : QObject(engine)
{
    connect(engine, &QQmlEngine::warnings, this, &QmlLogger::warnings);
}

void QmlLogger::warnings(const QList<QQmlError> &list)
{
    if (!LogQml().isWarningEnabled())
        return;

    for (const QQmlError &err : list) {
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

QT_END_NAMESPACE_AM

#include "moc_qmllogger.cpp"
