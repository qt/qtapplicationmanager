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

#include <QAbstractListModel>
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


class IntentServer : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/IntentServer 2.0 SINGLETON")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    ~IntentServer() override;
    static IntentServer *createInstance(IntentServerSystemInterface *systemInterface);
    static IntentServer *instance();

    void setDisambiguationTimeout(int timeout);
    void setStartApplicationTimeout(int timeout);
    void setReplyFromApplicationTimeout(int timeout);

    bool addPackage(const QString &packageId);
    void removePackage(const QString &packageId);

    bool addApplication(const QString &applicationId, const QString &packageId);
    void removeApplication(const QString &applicationId, const QString &packageId);

    Intent *addIntent(const QString &id, const QString &packageId, const QString &handlingApplicationId,
                      const QStringList &capabilities, Intent::Visibility visibility,
                      const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
                      const QUrl &icon, const QStringList &categories);

    void removeIntent(Intent *intent);

    QVector<Intent *> filterByIntentId(const QVector<Intent *> &intents, const QString &intentId,
                                       const QVariantMap &parameters = QVariantMap{}) const;
    QVector<Intent *> filterByRequestingApplicationId(const QVector<Intent *> &intents,
                                                      const QString &requestingApplicationId) const;

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // vvv QML API vvv

    int count() const;

    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Intent) *intent(int index) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Intent) *applicationIntent(const QString &intentId, const QString &applicationId,
                                                                   const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Intent) *packageIntent(const QString &intentId, const QString &packageId,
                                                               const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Intent) *packageIntent(const QString &intentId, const QString &packageId,
                                                               const QString &applicationId,
                                                               const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE int indexOfIntent(const QString &intentId, const QString &applicationId,
                                  const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE int indexOfIntent(Intent *intent);

    Q_INVOKABLE void acknowledgeDisambiguationRequest(const QUuid &requestId, Intent *selectedIntent);
    Q_INVOKABLE void rejectDisambiguationRequest(const QUuid &requestId);

signals:
    void intentAdded(QT_PREPEND_NAMESPACE_AM(Intent) *intent);
    void intentAboutToBeRemoved(QT_PREPEND_NAMESPACE_AM(Intent) *intent);

    void countChanged();

    // QML can only accept QList<QObject *> as signal parameter. Using QList<Intent *> or
    // QVector<QObject> will lead to an undefined QVariant on the QML side.
    void disambiguationRequest(const QUuid &requestId, const QList<QObject *> &potentialIntents,
                               const QVariantMap &parameters);
    /// ^^^ QML API ^^^

private:
    void internalDisambiguateRequest(const QUuid &requestId, bool reject, Intent *selectedIntent);
    void applicationWasStarted(const QString &applicationId);
    void replyFromApplication(const QString &replyingApplicationId, const QUuid &requestId,
                              bool error, const QVariantMap &result);
    IntentServerRequest *requestToSystem(const QString &requestingApplicationId, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters);

    void triggerRequestQueue();
    void enqueueRequest(IntentServerRequest *isr);
    void processRequestQueue();

    static QList<QObject *> convertToQml(const QVector<Intent *> &intents);
    QString packageIdForApplicationId(const QString &applicationId) const;

private:
    IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent = nullptr);
    Q_DISABLE_COPY(IntentServer)
    static IntentServer *s_instance;

    static QHash<int, QByteArray> s_roleNames;

    QMap<QString, QStringList> m_knownApplications;

    QQueue<IntentServerRequest *> m_requestQueue;

    QQueue<IntentServerRequest *> m_disambiguationQueue;
    QQueue<IntentServerRequest *> m_startingAppQueue;
    QQueue<IntentServerRequest *> m_sentToAppQueue;

    // no timeouts by default -- these have to be set at runtime
    int m_disambiguationTimeout = 0;
    int m_startingAppTimeout = 0;
    int m_sentToAppTimeout = 0;

    QVector<Intent *> m_intents;

    IntentServerSystemInterface *m_systemInterface;
    friend class IntentServerSystemInterface;
};

QT_END_NAMESPACE_AM
