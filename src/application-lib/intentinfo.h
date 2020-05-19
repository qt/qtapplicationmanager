/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
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

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

QT_BEGIN_NAMESPACE_AM

class YamlPackageScanner;
class PackageInfo;

class IntentInfo
{
public:
    IntentInfo(PackageInfo *packageInfo);
    ~IntentInfo();

    static const quint32 DataStreamVersion;

    enum Visibility {
        Public,
        Private
    };

    QString id() const;
    Visibility visibility() const;
    QStringList requiredCapabilities() const;
    QVariantMap parameterMatch() const;
    QString handlingApplicationId() const;

    QStringList categories() const;

    QMap<QString, QString> names() const;
    QString name(const QString &language) const;
    QMap<QString, QString> descriptions() const;
    QString description(const QString &language) const;
    QString icon() const;

    void writeToDataStream(QDataStream &ds) const;
    static IntentInfo *readFromDataStream(PackageInfo *pkg, QDataStream &ds);

private:
    PackageInfo *m_packageInfo;
    QString m_id;
    Visibility m_visibility = Public;
    QStringList m_requiredCapabilities;
    QVariantMap m_parameterMatch;

    QString m_handlingApplicationId;
    QStringList m_categories;
    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QString m_icon; // relative to info.json location

    friend class YamlPackageScanner;
    Q_DISABLE_COPY(IntentInfo)
};

QT_END_NAMESPACE_AM
