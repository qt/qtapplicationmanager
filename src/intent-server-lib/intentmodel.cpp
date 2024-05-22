// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlInfo>
#include <QJSEngine>
#include <QJSValueList>

#include "global.h"
#include "logging.h"
#include "intentserver.h"
#include "intentmodel.h"
#include "intent.h"
#include "qml-utilities.h"
#include "utilities.h"


/*!
    \qmltype IntentModel
    \inherits QSortFilterProxyModel
    \ingroup system-ui-instantiable
    \inqmlmodule QtApplicationManager.SystemUI
    \brief A proxy model for the IntentServer singleton.

    The IntentModel type provides a customizable model that can be used to tailor the
    IntentServer model to your needs. The IntentServer singleton is a model itself,
    that includes all available intents. In contrast, the IntentModel supports filtering
    and sorting.

    Since the IntentModel is based on the IntentServer model, the latter is referred
    to as the \e source model. The IntentModel includes the same \l {IntentServer Roles}
    {roles} as the IntentServer model.

    \note If you require a model with all intents, with no filtering whatsoever, you should
    use the IntentServer directly, as it has better performance.

    The following code snippet displays the icons of all the intents belonging to the \c
    com.pelagicore.test package:

    \qml
    import QtQuick
    import QtApplicationManager.SystemUI

    ListView {
        model: IntentModel {
            filterFunction: function(intent) {
                return intent.packageId == 'com.pelagicore.test'
            }
        }

        delegate: Image {
            required property string icon
            source: icon
        }
    }
    \endqml
*/

/*!
    \qmlproperty int IntentModel::count
    \readonly

    Holds the number of intents included in this model.
*/

/*!
    \qmlproperty var IntentModel::filterFunction

    A JavaScript function callback that is invoked for each intent in the IntentServer
    source model. This function gets one IntentObject parameter and must return a Boolean.
    If the intent passed should be included in this model, then the function must return
    \c true; \c false otherwise.

    If you need no filtering at all, you should set this property to an undefined (the default) or
    null value.

    \note Whenever this function is changed, the filter is reevaluated. Changes in the source model
    are reflected. To force a complete reevaluation, call \l {IntentModel::invalidate()}
    {invalidate()}.
*/

/*!
    \qmlproperty var IntentModel::sortFunction

    A JavaScript function callback that is invoked to sort the intents in this model. This function
    gets two IntentObject parameters and must return a Boolean. If the first intent should have a
    smaller index in this model than the second, the function must return \c true; \c false
    otherwise.

    If you need no sorting at all, you should set this property to an undefined (the default) or
    null value.

    \note Whenever this function is changed, the model is sorted. Changes in the source model are
    reflected. To force a complete reevaluation, call \l {IntentModel::invalidate()}{invalidate()}.
*/


QT_BEGIN_NAMESPACE_AM


class IntentModelPrivate
{
public:
    QJSEngine *m_engine = nullptr;
    QJSValue m_filterFunction;
    QJSValue m_sortFunction;
};


IntentModel::IntentModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , d(new IntentModelPrivate())
{
    setSourceModel(IntentServer::instance());

    connect(this, &QAbstractItemModel::rowsInserted, this, &IntentModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &IntentModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &IntentModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &IntentModel::countChanged);
}

int IntentModel::count() const
{
    return rowCount();
}

bool IntentModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (!d->m_engine) {
        d->m_engine = qjsEngine(this);
        if (!d->m_engine)
            qCWarning(LogIntents) << "IntentModel can't filter without a JavaScript engine";
    }

    if (d->m_engine && d->m_filterFunction.isCallable()) {
        const QObject *intent = IntentServer::instance()->intent(source_row);
        QJSValueList args = { d->m_engine->newQObject(const_cast<QObject*>(intent)) };
        return d->m_filterFunction.call(args).toBool();
    }

    return true;
}

bool IntentModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (!d->m_engine) {
        d->m_engine = qjsEngine(this);
        if (!d->m_engine)
            qCWarning(LogIntents) << "IntentModel can't sort without a JavaScript engine";
    }

    if (d->m_engine && d->m_sortFunction.isCallable()) {
        const QObject *intent1 = IntentServer::instance()->intent(source_left.row());
        const QObject *intent2 = IntentServer::instance()->intent(source_right.row());
        QJSValueList args = { d->m_engine->newQObject(const_cast<QObject*>(intent1)),
                              d->m_engine->newQObject(const_cast<QObject*>(intent2)) };
        return d->m_sortFunction.call(args).toBool();
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QJSValue IntentModel::filterFunction() const
{
    return d->m_filterFunction;
}

void IntentModel::setFilterFunction(const QJSValue &callback)
{
    if (!callback.equals(d->m_filterFunction)) {
        d->m_filterFunction = callback;
        emit filterFunctionChanged();
        invalidateFilter();
    }
}

QJSValue IntentModel::sortFunction() const
{
    return d->m_sortFunction;
}

void IntentModel::setSortFunction(const QJSValue &callback)
{
    if (!callback.equals(d->m_sortFunction)) {
        d->m_sortFunction = callback;
        emit sortFunctionChanged();
        invalidate();
        sort(0);
    }
}

/*!
    \qmlmethod int IntentModel::indexOfIntent(string intentId, string applicationId, var parameters)

    Maps the intent corresponding to the given \a intentId, \a applicationId and \a parameters to
    its position within this model. Returns \c -1 if the specified intent is invalid, or not part of
    this model.
*/
int IntentModel::indexOfIntent(const QString &intentId, const QString &applicationId,
                               const QVariantMap &parameters) const
{
    int idx = IntentServer::instance()->indexOfIntent(intentId, applicationId, parameters);
    return idx != -1 ? mapFromSource(idx) : idx;
}

/*!
    \qmlmethod int IntentModel::indexOfIntent(IntentObject intent)

    Maps the \a intent to its position within this model. Returns \c -1 if the specified intent is
    invalid, or not part of this model.
*/
int IntentModel::indexOfIntent(Intent *intent)
{
    int idx = IntentServer::instance()->indexOfIntent(intent);
    return idx != -1 ? mapFromSource(idx) : idx;

}

/*!
    \qmlmethod int IntentModel::mapToSource(int index)

    Maps an intent's \a index in this model to the corresponding index in the
    IntentServer model. Returns \c -1 if the specified \a index is invalid.
*/
int IntentModel::mapToSource(int ourIndex) const
{
    return QSortFilterProxyModel::mapToSource(index(ourIndex, 0)).row();
}

/*!
    \qmlmethod int IntentModel::mapFromSource(int index)

    Maps an intenet's \a index from the IntentServer model to the corresponding index in
    this model. Returns \c -1 if the specified \a index is invalid.
*/
int IntentModel::mapFromSource(int sourceIndex) const
{
    return QSortFilterProxyModel::mapFromSource(sourceModel()->index(sourceIndex, 0)).row();
}

/*!
    \qmlmethod IntentModel::invalidate()

    Forces a reevaluation of the model.
*/
void IntentModel::invalidate()
{
    QSortFilterProxyModel::invalidate();
}

void IntentModel::classBegin()
{ }

void IntentModel::componentComplete()
{
    ensureCurrentContextIsSystemUI(this);
}

QT_END_NAMESPACE_AM

#include "moc_intentmodel.cpp"
