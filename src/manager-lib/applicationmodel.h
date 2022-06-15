// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
    Q_CLASSINFO("AM-QmlPrototype", "QObject")

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
