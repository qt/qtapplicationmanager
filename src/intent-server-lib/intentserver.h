// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INTENTSERVER_H
#define INTENTSERVER_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>
#include <QtCore/QUuid>
#include <QtCore/QQueue>
#include <QtAppManCommon/global.h>
#include <QtAppManIntentServer/intent.h>


QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;
class IntentServerRequest;
class IntentServerSystemInterface;


class IntentServer : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

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
                      const QMap<QString, QString> &descriptions, const QUrl &icon,
                      const QStringList &categories, bool handleOnlyWhenRunning);

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
    Q_INVOKABLE QtAM::Intent *intent(int index) const;
    Q_INVOKABLE QtAM::Intent *applicationIntent(const QString &intentId, const QString &applicationId,
                                                const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE QtAM::Intent *packageIntent(const QString &intentId, const QString &packageId,
                                            const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE QtAM::Intent *packageIntent(const QString &intentId, const QString &packageId,
                                            const QString &applicationId,
                                            const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE int indexOfIntent(const QString &intentId, const QString &applicationId,
                                  const QVariantMap &parameters = QVariantMap{}) const;
    Q_INVOKABLE int indexOfIntent(QtAM::Intent *intent);

    Q_INVOKABLE void acknowledgeDisambiguationRequest(const QString &requestId,
                                                      QtAM::Intent *selectedIntent);
    Q_INVOKABLE void rejectDisambiguationRequest(const QString &requestId);

Q_SIGNALS:
    void intentAdded(QtAM::Intent *intent);
    void intentAboutToBeRemoved(QtAM::Intent *intent);

    void countChanged();

    void disambiguationRequest(const QString &requestId,
                               const QList<QtAM::Intent *> &potentialIntents,
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

    QString packageIdForApplicationId(const QString &applicationId) const;

private:
    IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent = nullptr);
    Q_DISABLE_COPY_MOVE(IntentServer)
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

#endif // INTENTSERVER_H
