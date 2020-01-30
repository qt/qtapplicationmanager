/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include <QObject>
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Intent : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/IntentObject 2.0 UNCREATABLE")

    Q_PROPERTY(QString intentId READ intentId CONSTANT)
    Q_PROPERTY(QString packageId READ packageId CONSTANT)
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT)
    Q_PROPERTY(QT_PREPEND_NAMESPACE_AM(Intent)::Visibility visibility READ visibility CONSTANT)
    Q_PROPERTY(QStringList requiredCapabilities READ requiredCapabilities CONSTANT)
    Q_PROPERTY(QVariantMap parameterMatch READ parameterMatch CONSTANT)

    Q_PROPERTY(QUrl icon READ icon CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QVariantMap names READ names CONSTANT)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)

public:
    enum Visibility {
        Public,
        Private
    };
    Q_ENUM(Visibility)

    Intent();

    QString intentId() const;
    Visibility visibility() const;
    QStringList requiredCapabilities() const;
    QVariantMap parameterMatch() const;

    QString packageId() const;
    QString applicationId() const;

    bool checkParameterMatch(const QVariantMap &parameters) const;

    QUrl icon() const;
    QString name() const;
    QVariantMap names() const;
    QString description() const;
    QVariantMap descriptions() const;
    QStringList categories() const;

private:
    Intent(const QString &intentId, const QString &packageId, const QString &applicationId,
           const QStringList &capabilities, Intent::Visibility visibility,
           const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
           const QUrl &icon, const QStringList &categories);

    QString m_intentId;
    Visibility m_visibility = Private;
    QStringList m_requiredCapabilities;
    QVariantMap m_parameterMatch;

    QString m_packageId;
    QString m_applicationId;

    QVariantMap m_names; // language -> name
    QStringList m_categories;
    QUrl m_icon;

    friend class IntentServer;
    friend class IntentServerHandler;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(Intent *))
