// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtAppManCommon/global.h>
#include <QtQml/qqmllist.h>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE_AM

class MonitorModel : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/MonitorModel 2.0")

    Q_PROPERTY(QQmlListProperty<QObject> dataSources READ dataSources NOTIFY dataSourcesChanged FINAL)
    Q_CLASSINFO("DefaultProperty", "dataSources")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int maximumCount READ maximumCount WRITE setMaximumCount  NOTIFY maximumCountChanged FINAL)

    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged FINAL)

public:
    MonitorModel(QObject *parent = nullptr);
    ~MonitorModel() override;

    QQmlListProperty<QObject> dataSources();

    static void dataSources_append(QQmlListProperty<QObject> *property, QObject *value);
    static qsizetype dataSources_count(QQmlListProperty<QObject> *property);
    static QObject *dataSources_at(QQmlListProperty<QObject> *property, qsizetype index);
    static void dataSources_clear(QQmlListProperty<QObject> *property);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    bool running() const;
    void setRunning(bool value);

    int interval() const;
    void setInterval(int value);

    int maximumCount() const;
    void setMaximumCount(int value);

    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void countChanged();
    void intervalChanged();
    void runningChanged();
    void maximumCountChanged();
    void dataSourcesChanged();

private slots:
    void readDataSourcesAndAddRow();

private:
    struct DataRow {
        QHash<int, QVariant> dataFromRoleIndex;
    };

    struct DataSource {
        QObject *obj;
        QVector<QByteArray> roleNames;
    };

    void clearDataSources();
    void appendDataSource(QObject *dataSource);
    void fillDataRow(DataRow *dataRow);
    void readDataSource(DataSource *dataSource, DataRow *dataRow);
    void trimHistory();
    bool extractRoleNamesFromJsArray(DataSource *dataSource);
    bool extractRoleNamesFromStringList(DataSource *dataSource);
    void addRoleName(QByteArray roleName, DataSource *dataSource);

    QList<DataSource*> m_dataSources;
    QList<QByteArray> m_roleNamesList; // also maps a role index to its name
    QHash<QByteArray, int> m_roleNameToIndex;

    QList<DataRow*> m_rows;

    QTimer m_timer;
    int m_maximumCount = 10;
};

QT_END_NAMESPACE_AM
