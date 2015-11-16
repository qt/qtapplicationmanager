/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include <QTimer>
#include <private/qqmlmetatype_p.h>

#include "qml-utilities.h"

void retakeSingletonOwnershipFromQmlEngine(QQmlEngine *qmlEngine, QObject *singleton)
{
    // QQmlEngine is taking ownership of singletons after the first call to instanceForQml() and
    // there is nothing to prevent this. This means that the singleton will be destroyed once the
    // QQmlEngine is destroyed, which is not what we want.
    // The workaround (until Qt has an API for that) is to remove the singleton QObject from QML's
    // internal singleton registry *after* the instanceForQml() function has finished.

    QTimer::singleShot(0, qmlEngine, [qmlEngine, singleton]() {
        foreach (const QQmlType *singletonType, QQmlMetaType::qmlSingletonTypes()) {
            if (singletonType->singletonInstanceInfo()->qobjectApi(qmlEngine) == singleton)
                singletonType->singletonInstanceInfo()->qobjectApis.remove(qmlEngine);
        }
    });
}
