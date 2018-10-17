/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
#include <QVariantMap>
#include <QVector>
#include <QUuid>
#include <QQueue>
#include <QtAppManCommon/global.h>
#include <QtAppManIntentServer/intent.h>


QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;
class IntentServerRequest;
class IntentServerSystemInterface;

// We cannot expose a list of Q_GADGETs to QML in a type-safe way. We can however
// at least try to make the C++ side of the API a bit more descriptive.
typedef QVariantList IntentList;

class IntentServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(IntentList intentList READ intentList NOTIFY intentListChanged)

public:
    ~IntentServer() override;
    static IntentServer *createInstance(IntentServerSystemInterface *systemInterface);
    static IntentServer *instance();

    bool addApplication(const QString &applicationId);
    void removeApplication(const QString &applicationId);

    bool addApplicationBackgroundHandler(const QString &applicationId, const QString &backgroundServiceId);
    void removeApplicationBackgroundHandler(const QString &applicationId, const QString &backgroundServiceId);

    Intent addIntent(const QString &id, const QString &applicationId, const QStringList &capabilities,
                     Intent::Visibility visibility, const QVariantMap &parameterMatch = QVariantMap());

    Intent addIntent(const QString &id, const QString &applicationId, const QString &backgroundHandlerId,
                     const QStringList &capabilities, Intent::Visibility visibility,
                     const QVariantMap &parameterMatch = QVariantMap());

    void removeIntent(const Intent &intent);

    QVector<Intent> all() const;
    QVector<Intent> filterByIntentId(const QVector<Intent> &intents, const QString &intentId,
                                     const QVariantMap &parameters = QVariantMap{}) const;
    QVector<Intent> filterByHandlingApplicationId(const QVector<Intent> &intents,
                                                  const QString &handlingApplicationId,
                                                  const QVariantMap &parameters = QVariantMap{}) const;
    QVector<Intent> filterByRequestingApplicationId(const QVector<Intent> &intents,
                                                    const QString &requestingApplicationId) const;

    // vvv QML API vvv

    IntentList intentList() const;

    Q_INVOKABLE Intent find(const QString &intentId, const QString &applicationId,
                            const QVariantMap &parameters = QVariantMap{}) const;

    Q_INVOKABLE void acknowledgeDisambiguationRequest(const QUuid &requestId, const Intent &selectedIntent);
    Q_INVOKABLE void rejectDisambiguationRequest(const QUuid &requestId);

signals:
    void intentAdded(const Intent &intent);
    void intentRemoved(const Intent &intent);
    void intentListChanged();

    void disambiguationRequest(const QUuid &requestId, const IntentList &potentialIntents,
                               const QVariantMap &parameters);
    /// ^^^ QML API ^^^

private:
    void internalDisambiguateRequest(const QUuid &requestId, bool reject, const Intent &selectedIntent);
    void applicationWasStarted(const QString &applicationId);
    void replyFromApplication(const QString &replyingApplicationId, const QUuid &requestId,
                              bool error, const QVariantMap &result);
    IntentServerRequest *requestToSystem(const QString &requestingApplicationId, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters);

    void triggerRequestQueue();
    void enqueueRequest(IntentServerRequest *irs);
    void processRequestQueue();

    static IntentList convertToQml(const QVector<Intent> &intents);

private:
    IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent = nullptr);
    Q_DISABLE_COPY(IntentServer)
    static IntentServer *s_instance;

    QStringList m_knownApplications;
    QMap<QString, QStringList> m_knownBackgroundServices;

    QQueue<IntentServerRequest *> m_requestQueue;

    QQueue<IntentServerRequest *> m_disambiguationQueue;
    QQueue<IntentServerRequest *> m_startingAppQueue;
    QQueue<IntentServerRequest *> m_sentToAppQueue;

    QVector<Intent> m_intents;

    IntentServerSystemInterface *m_systemInterface;
    friend class IntentServerSystemInterface;
};

QT_END_NAMESPACE_AM
