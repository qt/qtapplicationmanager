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
#include "qml-utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// guard against attacker-shaped variants nested deeply enough to blow the stack
static constexpr int MaxConversionDepth = 64;

static QVariant sanitizeWindowPropertyValueImpl(const QVariant &variant, int depth)
{
    if (depth >= MaxConversionDepth) {
        qCCritical(LogQml) << "sanitizeWindowPropertyValue: nesting level exceeds" << MaxConversionDepth
                           << "- returning empty variant";
        return { };
    }
    int type = variant.userType();

    if (type == qMetaTypeId<QJSValue>()) {
        return sanitizeWindowPropertyValueImpl(variant.value<QJSValue>().toVariant(), depth + 1);
    } else if (type == QMetaType::QVariant) {
        // got a matryoshka variant
        return sanitizeWindowPropertyValueImpl(variant.value<QVariant>(), depth + 1);
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        const QVariantList inList = variant.toList();
        for (const auto &v : inList)
            outList.append(sanitizeWindowPropertyValueImpl(v, depth + 1));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        const QVariantMap inMap = variant.toMap();
        for (const auto &[k, v] : inMap.asKeyValueRange())
            outMap.insert(k, sanitizeWindowPropertyValueImpl(v, depth + 1));
        return outMap;
    } else {
        return variant;
    }
}

QVariant sanitizeWindowPropertyValue(const QVariant &variant)
{
    return sanitizeWindowPropertyValueImpl(variant, 0);
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
