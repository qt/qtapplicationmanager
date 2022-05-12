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

#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <private/qqmlmetatype_p.h>
#include <private/qvariant_p.h>

#include "logging.h"
#include "utilities.h"
#include "qml-utilities.h"

QT_BEGIN_NAMESPACE_AM

// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
void loadQmlDummyDataFiles(QQmlEngine *engine, const QString &directory)
{
    QDir dir(directory + qSL("/dummydata"), qSL("*.qml"));
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if (comp.isError()) {
            const QList<QQmlError> errors = comp.errors();
            for (const QQmlError &error : errors)
                qCWarning(LogQml) << "Loading dummy data:" << error;
        }

        if (dummyData) {
            qml.truncate(qml.length() - 4);
            engine->rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(engine);
        }
    }
}

QT_END_NAMESPACE_AM
