/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QUrl>
#if defined(QT_QML_LIB)
#  include <QJSValue>
#endif
#if defined(QT_DBUS_LIB)
#  include <QDBusVariant>
#  include <QDBusArgument>
#endif

#include "dbus-utilities.h"


QVariant convertFromJSVariant(const QVariant &variant)
{
#if !defined(QT_QML_LIB)
    return variant;
#else
    int type = variant.userType();
    QVariant result;

    if (type == qMetaTypeId<QJSValue>()) {
        result = variant.value<QJSValue>().toVariant();
    } else if (type == QMetaType::QUrl) {
        result = QVariant(variant.value<QUrl>().toString());
    } else if (type == QMetaType::QVariant) {
        result = variant.value<QVariant>(); // got a matryoshka variant
    } else {
        return variant;
    }
    return convertFromJSVariant(result);
#endif
}

QVariant convertFromDBusVariant(const QVariant &variant)
{
#if !defined(QT_DBUS_LIB)
    return variant;
#else
    int type = variant.userType();

    if (type == qMetaTypeId<QDBusVariant>()) {
        QDBusVariant dbusVariant = variant.value<QDBusVariant>();
        return convertFromDBusVariant(dbusVariant.variant()); // just to be on the safe side
    } else if (type == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument dbusArg = variant.value<QDBusArgument>();
        switch (dbusArg.currentType()) {
        case QDBusArgument::BasicType:
        case QDBusArgument::VariantType:
            return convertFromDBusVariant(dbusArg.asVariant());
        case QDBusArgument::ArrayType: {
            QVariantList vl;
            dbusArg.beginArray();
            while (!dbusArg.atEnd()) {
                QDBusVariant elem;
                dbusArg >> elem;
                vl << convertFromDBusVariant(elem.variant());
            }
            dbusArg.endArray();
            return vl;
        }
        case QDBusArgument::MapType: {
            QVariantMap vm;
            dbusArg.beginMap();
            while (!dbusArg.atEnd()) {
                dbusArg.beginMapEntry();
                QString key;
                QDBusVariant value;
                dbusArg >> key >> value;
                dbusArg.endMapEntry();

                vm.insert(key, convertFromDBusVariant(value.variant()));
            }
            dbusArg.endMap();
            return vm;
        }
        default:
            return QVariant();
        }
    }
    return variant;
#endif
}
