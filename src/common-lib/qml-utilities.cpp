// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <private/qqmlmetatype_p.h>

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
