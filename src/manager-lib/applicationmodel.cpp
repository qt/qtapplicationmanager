/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include <QQmlContext>
#include <QQmlEngine>
#include <QJSEngine>
#include <QJSValueList>

#include "global.h"
#include "applicationmanager.h"
#include "applicationmodel.h"
#include "application.h"


/*!
    \qmltype ApplicationModel
    \inherits QSortFilterProxyModel
    \ingroup system-ui
    \inqmlmodule QtApplicationManager.SystemUI
    \brief A proxy model for the ApplicationManager singleton.

    The ApplicationModel type provides a customizable model of applications. It can be used to
    tailor the ApplicationManager model to your needs (the ApplicationManager singleton is itself
    a model that includes all available applications). It supports filtering and sorting.
    Since this model is based on the ApplicationManager model, the latter will be referred to as
    the \e source model. The ApplicationModel includes the same \l {ApplicationManager Roles}
    {roles} as the ApplicationManager model.

    \note If a model with all applications is needed, ApplicationManager should be used directly,
    since it performs slightly better.

    As an example, the following code snippet will show all icons of non-aliased applications in a
    list:

    \qml
    import QtQuick 2.6
    import QtApplicationManager.SystemUI 2.0

    ListView {
        model: ApplicationModel {
            filterFunction: function(application) {
                return !application.alias;
            }
        }

        delegate: Image {
            source: icon
        }
    }
    \endqml
*/

/*!
    \qmlproperty int ApplicationModel::count
    \readonly

    Holds the number of applications included in this model.
*/

/*!
    \qmlproperty var ApplicationModel::filterFunction

    A JavaScript function callback that will be invoked for each application in the
    ApplicationManager source model. It gets one parameter of type ApplicationObject and must return
    a bool. If the passed application should be included in this model the function must return
    \c true and \c false otherwise.

    \note The filter will be reevaluated when this function is changed. However, dynamic properties
    (like \c isRunning) used in this function will not trigger a reevaluation when the property
    changes. Changes in the source model will be reflected, though. Since the type is derived from
    QSortFilterProxyModel, the \l {QSortFilterProxyModel::invalidate()}{invalidate()} slot can be
    used to force a reevalution.
*/

/*!
    \qmlproperty var ApplicationModel::sortFunction

    A JavaScript function callback that will be invoked to sort the applications in this model. It
    gets two parameters of type ApplicationObject and must return a bool. If the first application
    should have a smaller index in this model than the second, the function must return \c true
    and \c false otherwise.

    \note Sorting will be done whenever this function is changed. However, dynamic properties
    (like \c isRunning) used in this function will not trigger sorting when the property changes.
    Changes in the source model will be reflected, though. Since the type is derived from
    QSortFilterProxyModel, the \l {QSortFilterProxyModel::invalidate()}{invalidate()} slot can be
    used to force a reevalution.
*/


QT_BEGIN_NAMESPACE_AM


class ApplicationModelPrivate
{
public:
    QJSEngine *m_engine = nullptr;
    QJSValue m_filterFunction;
    QJSValue m_sortFunction;
};


ApplicationModel::ApplicationModel()
    : d(new ApplicationModelPrivate())
{
    setSourceModel(ApplicationManager::instance());

    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ApplicationModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationModel::countChanged);
}

int ApplicationModel::count() const
{
    return rowCount();
}

bool ApplicationModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (!d->m_engine)
        d->m_engine = getJSEngine();

    if (d->m_engine && d->m_filterFunction.isCallable()) {
        const QObject *app = ApplicationManager::instance()->application(source_row);
        QJSValueList args = { d->m_engine->newQObject(const_cast<QObject*>(app)) };
        return d->m_filterFunction.call(args).toBool();
    }

    return true;
}

bool ApplicationModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (!d->m_engine)
        d->m_engine = getJSEngine();

    if (d->m_engine && d->m_sortFunction.isCallable()) {
        const QObject *app1 = ApplicationManager::instance()->application(source_left.row());
        const QObject *app2 = ApplicationManager::instance()->application(source_right.row());
        QJSValueList args = { d->m_engine->newQObject(const_cast<QObject*>(app1)),
                              d->m_engine->newQObject(const_cast<QObject*>(app2)) };
        return d->m_sortFunction.call(args).toBool();
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QJSValue ApplicationModel::filterFunction() const
{
    return d->m_filterFunction;
}

void ApplicationModel::setFilterFunction(const QJSValue &callback)
{
    if (!callback.equals(d->m_filterFunction)) {
        d->m_filterFunction = callback;
        emit filterFunctionChanged();
        invalidateFilter();
    }
}

QJSValue ApplicationModel::sortFunction() const
{
    return d->m_sortFunction;
}

void ApplicationModel::setSortFunction(const QJSValue &callback)
{
    if (!callback.equals(d->m_sortFunction)) {
        d->m_sortFunction = callback;
        emit sortFunctionChanged();
        invalidate();
        sort(0);
    }
}

/*!
    \qmlmethod int ApplicationModel::indexOfApplication(string id)

    Maps the application \a id to its position within this model. Returns \c -1 if the specified
    \a id is invalid, or not part of this model.
*/
int ApplicationModel::indexOfApplication(const QString &id) const
{
    int idx = ApplicationManager::instance()->indexOfApplication(id);
    return idx != -1 ? mapFromSource(idx) : idx;
}

/*!
    \qmlmethod int ApplicationModel::mapToSource(int index)

    Maps an application's \a index in this model to the corresponding index in the
    ApplicationManager model. Returns \c -1 if the specified \a index is invalid.
*/
int ApplicationModel::mapToSource(int ourIndex) const
{
    return QSortFilterProxyModel::mapToSource(index(ourIndex, 0)).row();
}

/*!
    \qmlmethod int ApplicationModel::mapFromSource(int sourceIndex)

    Maps an application's \a index from the ApplicationManager model to the corresponding index in
    this model. Returns \c -1 if the specified \a index is invalid.
*/
int ApplicationModel::mapFromSource(int sourceIndex) const
{
    return QSortFilterProxyModel::mapFromSource(sourceModel()->index(sourceIndex, 0)).row();
}

QJSEngine *ApplicationModel::getJSEngine() const
{
    QQmlContext *context = QQmlEngine::contextForObject(this);
    return context ? reinterpret_cast<QJSEngine*>(context->engine()) : nullptr;
}

QT_END_NAMESPACE_AM
