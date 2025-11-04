// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlInfo>
#include <private/qqmlmetatype_p.h>
#include <private/qv4engine_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlcontextdata_p.h>
#include "logging.h"
#include "utilities.h"
#include "qml-utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
void loadQmlDummyDataFiles(QQmlEngine *engine, const QString &directory)
{
    QDir dir(directory + u"/dummydata"_s, u"*.qml"_s);
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

QVariant convertFromJSVariant(const QVariant &variant)
{
    int type = variant.userType();

    if (type == qMetaTypeId<QJSValue>()) {
        return convertFromJSVariant(variant.value<QJSValue>().toVariant());
    } else if (type == QMetaType::QVariant) {
        // got a matryoshka variant
        return convertFromJSVariant(variant.value<QVariant>());
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        QVariantList inList = variant.toList();
        for (auto it = inList.cbegin(); it != inList.cend(); ++it)
            outList.append(convertFromJSVariant(*it));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        QVariantMap inMap = variant.toMap();
        for (auto it = inMap.cbegin(); it != inMap.cend(); ++it)
            outMap.insert(it.key(), convertFromJSVariant(it.value()));
        return outMap;
    } else {
        return variant;
    }
}


static const char *qmlContextTag = "_q_am_context_tag";


QVariant findTaggedQmlContext(QObject *object)
{
    auto findTag = [](QQmlContext *context) -> QVariant {
        while (context) {
            auto v = context->property(qmlContextTag);
            if (v.isValid())
                return v;
            context = context->parentContext();
        }
        return { };
    };

    // check the context the object lives in
    QVariant v  = findTag(QQmlEngine::contextForObject(object));
    if (!v.isValid()) {
        // if this didn't work out, check out the calling context
        if (QQmlEngine *engine = qmlEngine(object)) {
            if (QV4::ExecutionEngine *v4 = engine->handle()) {
                if (QQmlContextData *callingContext = v4->callingQmlContext().data())
                    v = findTag(callingContext->asQQmlContext());
            }
        }
    }
    return v;
}

bool tagQmlContext(QQmlContext *context, const QVariant &value)
{
    if (!context || !value.isValid())
        return false;
    return !context->setProperty(qmlContextTag, value);
}

bool ensureCurrentContextIsSystemUI(QObject *object)
{
    static const char *error = "This object can not be used in an Application context";

    if (findTaggedQmlContext(object).isValid()) {
        qmlWarning(object) << error;
        Q_ASSERT_X(false, object ? object->metaObject()->className() : "", error);
        return false;
    }
    return true;
}

bool ensureCurrentContextIsInProcessApplication(QObject *object)
{
    static const char *error = "This object can not be used in the SystemUI context";

    if (!findTaggedQmlContext(object).isValid()) {
        qmlWarning(object) << error;
        Q_ASSERT_X(false, object ? object->metaObject()->className() : "", error);
        return false;
    }
    return true;
}

QT_END_NAMESPACE_AM
