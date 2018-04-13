/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include <QTimer>
#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <private/qqmlmetatype_p.h>
#include <private/qvariant_p.h>

#include "logging.h"
#include "qml-utilities.h"

QT_BEGIN_NAMESPACE_AM

/*! \internal
    Traverse an arbitrarily deep QVariantMap and replace all invalid QVariants with null values
    that QML is able to understand. We're doing some nasty trickery with v_cast to prevent
    copies due to detaching.
*/

void fixNullValuesForQml(QVariantList &list)
{
    for (auto it = list.cbegin(); it != list.cend(); ++it)
        fixNullValuesForQml(const_cast<QVariant &>(*it));
}

void fixNullValuesForQml(QVariantMap &map)
{
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        fixNullValuesForQml(const_cast<QVariant &>(it.value()));
}

void fixNullValuesForQml(QVariant &v)
{
    switch (static_cast<int>(v.type())) {
    case QVariant::List: {
        QVariantList *list = v_cast<QVariantList>(&v.data_ptr());
        fixNullValuesForQml(*list);
        break;
    }
    case QVariant::Map: {
        QVariantMap *map = v_cast<QVariantMap>(&v.data_ptr());
        fixNullValuesForQml(*map);
        break;
    }
    case QVariant::Invalid: {
        QVariant v2 = QVariant::fromValue(nullptr);
        qSwap(v.data_ptr(), v2.data_ptr());
        break;
    }
    }
}

void retakeSingletonOwnershipFromQmlEngine(QQmlEngine *qmlEngine, QObject *singleton, bool immediately)
{
    // QQmlEngine is taking ownership of singletons after the first call to instanceForQml() and
    // there is nothing to prevent this. This means that the singleton will be destroyed once the
    // QQmlEngine is destroyed, which is not what we want.
    // The workaround (until Qt has an API for that) is to remove the singleton QObject from QML's
    // internal singleton registry *after* the instanceForQml() function has finished.

    auto retake = [qmlEngine, singleton]() {
        const auto types = QQmlMetaType::qmlSingletonTypes();
        for (const auto &singletonType : types) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 2)
            if (singletonType.singletonInstanceInfo()->qobjectApi(qmlEngine) == singleton)
                singletonType.singletonInstanceInfo()->qobjectApis.remove(qmlEngine);
#else
            if (singletonType->singletonInstanceInfo()->qobjectApi(qmlEngine) == singleton)
                singletonType->singletonInstanceInfo()->qobjectApis.remove(qmlEngine);
#endif
        }
    };

    if (immediately)
        retake();
    else
        QTimer::singleShot(0, qmlEngine, retake);
}

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
