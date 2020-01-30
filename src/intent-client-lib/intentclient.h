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
#include <QVariantMap>
#include <QList>
#include <QMap>
#include <QPair>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentHandler;
class IntentClientRequest;
class IntentClientSystemInterface;

/* internal interface used by IntentRequest and IntentHandler */

class IntentClient : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/IntentClient 2.0 SINGLETON")

public:
    ~IntentClient() override;
    static IntentClient *createInstance(IntentClientSystemInterface *systemInterface);
    static IntentClient *instance();

    void setReplyFromSystemTimeout(int timeout);
    void setReplyFromApplicationTimeout(int timeout);

    IntentClientRequest *requestToSystem(const QString &requestingApplicationId, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters);
    void replyFromApplication(IntentClientRequest *icr, const QVariantMap &result);
    void errorReplyFromApplication(IntentClientRequest *icr, const QString &errorMessage);

    void registerHandler(IntentHandler *handler);
    void unregisterHandler(IntentHandler *handler);

    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *sendIntentRequest(const QString &intentId,
                                                                                const QVariantMap &parameters);
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *sendIntentRequest(const QString &intentId,
                                                                                const QString &applicationId,
                                                                                const QVariantMap &parameters);

private:
    void requestToSystemFinished(IntentClientRequest *icr, const QUuid &newRequestId,
                                 bool error, const QString &errorMessage);
    void replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result);

    void requestToApplication(const QUuid &requestId, const QString &intentId, const QString &requestingApplicationId,
                              const QString &applicationId, const QVariantMap &parameters);

private:
    IntentClient(IntentClientSystemInterface *systemInterface, QObject *parent = nullptr);
    Q_DISABLE_COPY(IntentClient)
    static IntentClient *s_instance;

    QList<IntentClientRequest *> m_waiting;
    QMap<QPair<QString, QString>, IntentHandler *> m_handlers; // intentId + appId -> handler

    // no timeouts by default -- these have to be set at runtime
    int m_replyFromSystemTimeout = 0;
    int m_replyFromApplicationTimeout = 0;

    IntentClientSystemInterface *m_systemInterface;
    friend class IntentClientSystemInterface;
};

QT_END_NAMESPACE_AM
