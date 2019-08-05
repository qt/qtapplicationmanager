/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#pragma once

#include <QAbstractListModel>
#include <QtAppManCommon/global.h>
#include <QtQml/qqmllist.h>
#include <QList>
#include <QStringList>
#include <QTimer>

QT_BEGIN_NAMESPACE_AM

class MonitorModel : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/MonitorModel 2.0")

    Q_PROPERTY(QQmlListProperty<QObject> dataSources READ dataSources)
    Q_CLASSINFO("DefaultProperty", "dataSources")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int maximumCount READ maximumCount WRITE setMaximumCount  NOTIFY maximumCountChanged)

    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

public:
    MonitorModel(QObject *parent = nullptr);
    ~MonitorModel() override;

    QQmlListProperty<QObject> dataSources();

    static void dataSources_append(QQmlListProperty<QObject> *property, QObject *value);
    static int dataSources_count(QQmlListProperty<QObject> *property);
    static QObject *dataSources_at(QQmlListProperty<QObject> *property, int index);
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
