// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlInfo>
#include <QJSEngine>
#include <QJSValueList>

#include "global.h"
#include "logging.h"
#include "notificationmanager.h"
#include "notificationmodel.h"
#include "notification.h"
#include "utilities.h"


/*!
    \qmltype NotificationModel
    \inherits QSortFilterProxyModel
    \ingroup system-ui-instantiable
    \inqmlmodule QtApplicationManager.SystemUI
    \brief A proxy model for the NotificationManager singleton.

    The NotificationModel type provides a customizable model that can be used to tailor the
    NotificationManager model to your needs. The NotificationManager singleton is a model itself,
    that includes all available notifications. In contrast, the NotificationModel supports filtering
    and sorting.

    Since the NotificationModel is based on the NotificationManager model, the latter is referred
    to as the \e source model. The NotificationModel includes the same \l {NotificationManager Roles}
    {roles} as the NotificationManager model.

    \note If you require a model with all notifications, with no filtering whatsoever, you should
    use the NotificationManager directly, as it has better performance.

    The following code snippet displays the summary of all notifications with a priority greater
    than 5 in a list:

    \qml
    import QtQuick
    import QtApplicationManager.SystemUI

    ListView {
        model: NotificationModel {
            filterFunction: function(notification) {
                return notification.priority > 5;
            }
        }

        Text {
            required property string summary
            text: summary
        }
    }
    \endqml
*/

/*!
    \qmlproperty int NotificationModel::count
    \readonly

    Holds the number of notifications included in this model.
*/

/*!
    \qmlproperty var NotificationModel::filterFunction

    A JavaScript function callback that is invoked for each notification in the NotificationManager
    source model. This function gets one Notification parameter and must return a Boolean.
    If the notification passed should be included in this model, then the function must return
    \c true; \c false otherwise.

    If you need no filtering at all, you should set this property to an undefined (the default) or
    null value.

    \note Whenever this function is changed, the filter is reevaluated. Changes in the source model
    are also reflected. To force a complete reevaluation, call \l {NotificationModel::invalidate()}
    {invalidate()}.
*/

/*!
    \qmlproperty var NotificationModel::sortFunction

    A JavaScript function callback that is invoked to sort the notifications in this model. This
    function gets two Notification parameters and must return a Boolean. If the first
    notification should have a smaller index in this model than the second, the function must return
    \c true; \c false otherwise.

    If you need no sorting at all, you should set this property to an undefined (the default) or
    null value.

    \note Whenever this function is changed, the model is sorted. Changes in the source model are
    also reflected. To force a complete reevaluation, call \l {NotificationModel::invalidate()}
    {invalidate()}.
*/


QT_BEGIN_NAMESPACE_AM


class NotificationModelPrivate
{
public:
    QJSEngine *m_engine = nullptr;
    QJSValue m_filterFunction;
    QJSValue m_sortFunction;
};


NotificationModel::NotificationModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , d(new NotificationModelPrivate())
{
    setSourceModel(NotificationManager::instance());

    connect(this, &QAbstractItemModel::rowsInserted, this, &NotificationModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &NotificationModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &NotificationModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &NotificationModel::countChanged);
}

NotificationModel::~NotificationModel()
{
    delete d;
}

int NotificationModel::count() const
{
    return rowCount();
}

bool NotificationModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (!d->m_engine) {
        d->m_engine = qjsEngine(this);
        if (!d->m_engine)
            qCWarning(LogNotifications) << "NotificationModel can't filter without a JavaScript engine";
    }

    if (d->m_engine && d->m_filterFunction.isCallable()) {
        const QVariantMap notification = NotificationManager::instance()->get(source_row);
        QJSValueList args = { d->m_engine->toScriptValue(notification) };
        return d->m_filterFunction.call(args).toBool();
    }

    return true;
}

bool NotificationModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (!d->m_engine) {
        d->m_engine = qjsEngine(this);
        if (!d->m_engine)
            qCWarning(LogNotifications) << "NotificationModel can't sort without a JavaScript engine";
    }

    if (d->m_engine && d->m_sortFunction.isCallable()) {
        const QVariantMap left = NotificationManager::instance()->get(source_left.row());
        const QVariantMap right = NotificationManager::instance()->get(source_right.row());
        QJSValueList args = { d->m_engine->toScriptValue(left), d->m_engine->toScriptValue(right) };
        return d->m_sortFunction.call(args).toBool();
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QJSValue NotificationModel::filterFunction() const
{
    return d->m_filterFunction;
}

void NotificationModel::setFilterFunction(const QJSValue &callback)
{
    if (!callback.isCallable() && !callback.isNull() && !callback.isUndefined()) {
        qmlWarning(this) << "The filterFunction property of NotificationModel needs to be either "
                            "callable, or undefined/null to clear it.";
    }

    if (!callback.equals(d->m_filterFunction)) {
        d->m_filterFunction = callback;
        emit filterFunctionChanged();
        invalidateFilter();
    }
}

QJSValue NotificationModel::sortFunction() const
{
    return d->m_sortFunction;
}

void NotificationModel::setSortFunction(const QJSValue &callback)
{
    if (!callback.isCallable() && !callback.isNull() && !callback.isUndefined()) {
        qmlWarning(this) << "The sortFunction property of NotificationModel needs to be either "
                            "callable, or undefined/null to clear it.";
    }
    if (!callback.equals(d->m_sortFunction)) {
        d->m_sortFunction = callback;
        emit sortFunctionChanged();
        invalidate();
        sort(0);
    }
}

/*!
    \qmlmethod int NotificationModel::indexOfNotification(int notificationId)

    Maps the notification corresponding to the given \a notificationId to its position within this
    model. Returns \c -1 if the specified notification is invalid, or not part of this model.
*/
int NotificationModel::indexOfNotification(uint notificationId) const
{
    int idx = NotificationManager::instance()->indexOfNotification(notificationId);
    return idx != -1 ? mapFromSource(idx) : idx;
}

/*!
    \qmlmethod int NotificationModel::indexOfNotification(object notification)

    Maps the \a notification to its position within this model. Returns \c -1 if the specified
    notification is invalid, or not part of this model.
*/
int NotificationModel::indexOfNotification(Notification *notification) const
{
    return indexOfNotification(notification->notificationId());
}

/*!
    \qmlmethod int NotificationModel::mapToSource(int index)

    Maps a notification's \a index in this model to the corresponding index in the
    NotificationManager model. Returns \c -1 if the specified \a index is invalid.
*/
int NotificationModel::mapToSource(int ourIndex) const
{
    return QSortFilterProxyModel::mapToSource(index(ourIndex, 0)).row();
}

/*!
    \qmlmethod int NotificationModel::mapFromSource(int index)

    Maps a notification's \a index from the NotificationManager model to the corresponding index in
    this model. Returns \c -1 if the specified \a index is invalid.
*/
int NotificationModel::mapFromSource(int sourceIndex) const
{
    return QSortFilterProxyModel::mapFromSource(sourceModel()->index(sourceIndex, 0)).row();
}

/*!
    \qmlmethod NotificationModel::invalidate()

    Forces a reevaluation of the model.
*/
void NotificationModel::invalidate()
{
    QSortFilterProxyModel::invalidate();
}

QT_END_NAMESPACE_AM

#include "moc_notificationmodel.cpp"
