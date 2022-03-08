/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QSortFilterProxyModel>
#include <QJSValue>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QJSEngine);

QT_BEGIN_NAMESPACE_AM

class ApplicationModelPrivate;
class Application;

class ApplicationModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ApplicationModel 2.0")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QJSValue filterFunction READ filterFunction WRITE setFilterFunction NOTIFY filterFunctionChanged)
    Q_PROPERTY(QJSValue sortFunction READ sortFunction WRITE setSortFunction NOTIFY sortFunctionChanged)

public:
    ApplicationModel(QObject *parent = nullptr);
    ~ApplicationModel() override;

    int count() const;

    QJSValue filterFunction() const;
    void setFilterFunction(const QJSValue &callback);

    QJSValue sortFunction() const;
    void setSortFunction(const QJSValue &callback);

    Q_INVOKABLE int indexOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(QT_PREPEND_NAMESPACE_AM(Application) *application) const;
    Q_INVOKABLE int mapToSource(int ourIndex) const;
    Q_INVOKABLE int mapFromSource(int sourceIndex) const;

protected:
    using QSortFilterProxyModel::mapToSource;
    using QSortFilterProxyModel::mapFromSource;

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

signals:
    void countChanged();
    void filterFunctionChanged();
    void sortFunctionChanged();

private:
    QJSEngine *getJSEngine() const;

    ApplicationModelPrivate *d;
};

QT_END_NAMESPACE_AM
