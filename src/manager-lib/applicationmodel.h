// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONMODEL_H
#define APPLICATIONMODEL_H

#include <QtCore/QSortFilterProxyModel>
#include <QtQml/QJSValue>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QJSEngine);

QT_BEGIN_NAMESPACE_AM

class ApplicationModelPrivate;
class Application;

class ApplicationModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(QJSValue filterFunction READ filterFunction WRITE setFilterFunction NOTIFY filterFunctionChanged FINAL)
    Q_PROPERTY(QJSValue sortFunction READ sortFunction WRITE setSortFunction NOTIFY sortFunctionChanged FINAL)

public:
    ApplicationModel(QObject *parent = nullptr);
    ~ApplicationModel() override;

    int count() const;

    QJSValue filterFunction() const;
    void setFilterFunction(const QJSValue &callback);

    QJSValue sortFunction() const;
    void setSortFunction(const QJSValue &callback);

    Q_INVOKABLE int indexOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfApplication(QtAM::Application *application) const;
    Q_INVOKABLE int mapToSource(int ourIndex) const;
    Q_INVOKABLE int mapFromSource(int sourceIndex) const;
    Q_INVOKABLE void invalidate();

protected:
    using QSortFilterProxyModel::mapToSource;
    using QSortFilterProxyModel::mapFromSource;

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

Q_SIGNALS:
    void countChanged();
    void filterFunctionChanged();
    void sortFunctionChanged();

private:
    ApplicationModelPrivate *d;
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONMODEL_H
