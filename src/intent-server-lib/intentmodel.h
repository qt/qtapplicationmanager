// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INTENTMODEL_H
#define INTENTMODEL_H

#include <QtCore/QSortFilterProxyModel>
#include <QtQml/QJSValue>
#include <QtQml/QQmlParserStatus>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QJSEngine);

QT_BEGIN_NAMESPACE_AM

class Intent;
class IntentModelPrivate;

class IntentModel : public QSortFilterProxyModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(QJSValue filterFunction READ filterFunction WRITE setFilterFunction NOTIFY filterFunctionChanged FINAL)
    Q_PROPERTY(QJSValue sortFunction READ sortFunction WRITE setSortFunction NOTIFY sortFunctionChanged FINAL)

public:
    IntentModel(QObject *parent = nullptr);

    int count() const;

    QJSValue filterFunction() const;
    void setFilterFunction(const QJSValue &callback);

    QJSValue sortFunction() const;
    void setSortFunction(const QJSValue &callback);

    Q_INVOKABLE int indexOfIntent(const QString &intentId, const QString &applicationId,
                                  const QVariantMap &parameters = {}) const;
    Q_INVOKABLE int indexOfIntent(QtAM::Intent *intent);
    Q_INVOKABLE int mapToSource(int ourIndex) const;
    Q_INVOKABLE int mapFromSource(int sourceIndex) const;
    Q_INVOKABLE void invalidate();

    void classBegin() override;
    void componentComplete() override;

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
    IntentModelPrivate *d;
};

QT_END_NAMESPACE_AM

#endif // INTENTMODEL_H
