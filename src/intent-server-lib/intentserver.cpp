// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentserver.h"
#include "intentserversysteminterface.h"
#include "intentserverrequest.h"
#include "intentmodel.h"

#include <QtAppManCommon/logging.h>

#include <algorithm>

#include <QRegularExpression>
#include <QUuid>
#include <QMetaObject>
#include <QTimer>
#include <QDebug>

#include <QQmlEngine>
#include <QQmlInfo>

#include <memory>

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype IntentServer
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The System UI side singleton representing the Intents sub-system.

    This singleton serves two purposes: for one, it gives the System UI access to the database of
    all the available intents via its item model API, plus it exposes the API to deal with ambigous intent
    requests. Intent requests can be ambigous if the requesting party only specified the \c
    intentId, but not the targeted \c applicationId in its call to
    IntentClient::sendIntentRequest(). In these cases, it is the responsibility of the System UI to
    disambiguate these requests by reacting on the disambiguationRequest() signal.

    The type is derived from \c QAbstractListModel, so it can be used directly
    as a model in app-grid views.

    \target IntentServer Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c intentId
        \li string
        \li The id of the intent.
    \row
        \li \c packageId
        \li string
        \li The unique id of the package that the handling application of this intent is part of.
    \row
        \li \c applicationId
        \li string
        \li The id of the application responsible for handling this intent.
    \row
        \li \c name
        \li string
        \li The name of the intent. If possible, already translated to the current locale.
            If no name was defined for the intent, the name of the corresponding package will be
            returned.
    \row
        \li \c icon
        \li string
        \li The URL of the intent's icon.
            If no icon was defined for the intent, the icon of the corresponding package will be
            returned.
    \row
        \li \c categories
        \li list<string>
        \li The categories this intent is registered for via its meta-data file.
    \row
        \li \c intent
        \li IntentObject
        \li The underlying \l IntentObject for quick access to the properties outside of a
            model delegate.
    \row
        \li \c intentObject
        \li IntentObject
        \li Exactly the same as \c intent. This was added to keep the role names between the
            PackageManager and IntentServer models as similar as possible.
            This role was introduced in Qt version 6.6.
    \endtable
*/

/*! \qmlsignal IntentServer::intentAdded(Intent intent)
    Emitted when a new \a intent gets added to the intentList (e.g. on application installation).
*/

/*! \qmlsignal IntentServer::intentAboutToBeRemoved(Intent intent)
    Emitted when an existing \a intent is going to be removed from the intentList (e.g. on
    application deinstallation).
*/


enum Roles
{
    IntentId = Qt::UserRole,
    ApplicationId,
    PackageId,
    ParameterMatch,
    Name,
    Icon,
    Categories,
    IntentItem,
    IntentObject, // needed to keep the roles similar to PackageManager
};

IntentServer *IntentServer::s_instance = nullptr;
QHash<int, QByteArray> IntentServer::s_roleNames;

IntentServer *IntentServer::createInstance(IntentServerSystemInterface *systemInterface)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("IntentServer::createInstance() was called a second time.");
    if (Q_UNLIKELY(!systemInterface))
        qFatal("IntentServer::createInstance() was called without a systemInterface.");

    std::unique_ptr<IntentServer> is(new IntentServer(systemInterface));
    systemInterface->initialize(is.get());

    qmlRegisterType<Intent>("QtApplicationManager.SystemUI", 2, 0, "IntentObject");
    qmlRegisterType<Intent, 1>("QtApplicationManager.SystemUI", 2, 1, "IntentObject");
    qmlRegisterType<IntentModel>("QtApplicationManager.SystemUI", 2, 0, "IntentModel");

    qmlRegisterSingletonType<IntentServer>("QtApplicationManager.SystemUI", 2, 0, "IntentServer",
                                           [](QQmlEngine *, QJSEngine *) -> QObject * {
        QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
        return instance();
    });
    return s_instance = is.release();
}

IntentServer *IntentServer::instance()
{
    if (!s_instance)
        qFatal("IntentServer::instance() was called before createInstance().");
    return s_instance;
}

void IntentServer::setDisambiguationTimeout(int timeout)
{
    m_disambiguationTimeout = timeout;
}

void IntentServer::setStartApplicationTimeout(int timeout)
{
    m_startingAppTimeout = timeout;
}

void IntentServer::setReplyFromApplicationTimeout(int timeout)
{
    m_sentToAppTimeout = timeout;
}

IntentServer::IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent)
    : QAbstractListModel(parent)
    , m_systemInterface(systemInterface)
{
    m_systemInterface->setParent(this);

    if (s_roleNames.isEmpty()) {
        s_roleNames.insert(IntentId, "intentId");
        s_roleNames.insert(ApplicationId, "applicationId");
        s_roleNames.insert(PackageId, "packageId");
        s_roleNames.insert(ParameterMatch, "parameterMatch");
        s_roleNames.insert(Name, "name");
        s_roleNames.insert(Icon, "icon");
        s_roleNames.insert(Categories, "categories");
        s_roleNames.insert(IntentItem, "intent");
        s_roleNames.insert(IntentObject, "intentObject");
    }
}

IntentServer::~IntentServer()
{
    qDeleteAll(m_intents);
    s_instance = nullptr;
}

bool IntentServer::addPackage(const QString &packageId)
{
    if (m_knownApplications.contains(packageId))
        return false;
    m_knownApplications.insert(packageId, QStringList());
    return true;
}

void IntentServer::removePackage(const QString &packageId)
{
    m_knownApplications.remove(packageId);
}

bool IntentServer::addApplication(const QString &applicationId, const QString &packageId)
{
    if (!m_knownApplications.contains(packageId))
        return false;
    if (m_knownApplications.value(packageId).contains(applicationId))
        return false;
    m_knownApplications[packageId].append(applicationId);
    return true;
}

void IntentServer::removeApplication(const QString &applicationId, const QString &packageId)
{
    m_knownApplications[packageId].removeAll(applicationId);
}

Intent *IntentServer::addIntent(const QString &id, const QString &packageId,
                                const QString &handlingApplicationId,
                                const QStringList &capabilities, Intent::Visibility visibility,
                                const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
                                const QMap<QString, QString> &descriptions, const QUrl &icon,
                                const QStringList &categories, bool handleOnlyWhenRunning)
{
    try {
        if (id.isEmpty())
            throw "no id specified";
        if (packageId.isEmpty())
            throw "no packageId specified";
        if (handlingApplicationId.isEmpty())
            throw "no handlingApplicationId specified";
        if (!m_knownApplications.contains(packageId))
            throw "packageId is not known";
        if (!m_knownApplications.value(packageId).contains(handlingApplicationId))
            throw "applicationId is not known or not part of the specified package";
        if (applicationIntent(id, handlingApplicationId))
            throw "intent with given id/handlingApplicationId already exists";
    } catch (const char *e) {
        qCWarning(LogIntents) << "Cannot add intent" << id << "in package" << packageId
                              << "handled by" << handlingApplicationId << ":" << e;
        return nullptr;
    }

    auto intent = new Intent(id, packageId, handlingApplicationId, capabilities, visibility,
                             parameterMatch, names, descriptions, icon, categories,
                             handleOnlyWhenRunning);
    QQmlEngine::setObjectOwnership(intent, QQmlEngine::CppOwnership);

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_intents << intent;
    endInsertRows();

    emit countChanged();
    emit intentAdded(intent);
    return intent;
}

void IntentServer::removeIntent(Intent *intent)
{
    int index = m_intents.indexOf(intent);
    if (index >= 0) {
        emit intentAboutToBeRemoved(intent);
        beginRemoveRows(QModelIndex(), index, index);
        m_intents.removeAt(index);
        endRemoveRows();

        emit countChanged();

        delete intent;
    }
}

QVector<Intent *> IntentServer::filterByIntentId(const QVector<Intent *> &intents, const QString &intentId,
                                                 const QVariantMap &parameters) const
{
    QVector<Intent *> result;
    std::copy_if(intents.cbegin(), intents.cend(), std::back_inserter(result),
                 [intentId, parameters](Intent *intent) -> bool {
        return (intent->intentId() == intentId) && intent->checkParameterMatch(parameters);

    });
    return result;
}


QVector<Intent *> IntentServer::filterByRequestingApplicationId(const QVector<Intent *> &intents,
                                                                const QString &requestingApplicationId) const
{
    const QString requestingPackageId = packageIdForApplicationId(requestingApplicationId);

    QVector<Intent *> result;
    std::copy_if(intents.cbegin(), intents.cend(), std::back_inserter(result),
                 [this, requestingPackageId, requestingApplicationId](Intent *intent) -> bool {
        // filter on visibility and capabilities, if the requesting app is different from the
        // handling app

        if (intent->packageId() != requestingPackageId) {
            if (intent->visibility() == Intent::Private) {
                qCDebug(LogIntents) << "Not considering" << intent->intentId() << "in package"
                                    << intent->packageId() << "due to private visibility";
                return false;
            } else if (!intent->requiredCapabilities().isEmpty()
                       && !m_systemInterface->checkApplicationCapabilities(requestingApplicationId,
                                                                           intent->requiredCapabilities())) {
                qCDebug(LogIntents) << "Not considering" << intent->intentId() << "in package"
                                    << intent->packageId()
                                    << "due to missing capabilities of requesting application";
                return false;
            }
        }
        return true;
    });
    return result;
}

int IntentServer::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? -1 : m_intents.count();
}

QVariant IntentServer::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    Intent *intent = m_intents.at(index.row());

    switch (role) {
    case IntentId:
        return intent->intentId();
    case PackageId:
        return intent->packageId();
    case ApplicationId:
        return intent->applicationId();
    case ParameterMatch:
        return intent->parameterMatch();
    case Name:
        return intent->name();
    case Icon:
        return intent->icon();
    case Categories:
        return intent->categories();
    case IntentItem:
    case IntentObject:
        return QVariant::fromValue(intent);
    }
    return QVariant();
}

QHash<int, QByteArray> IntentServer::roleNames() const
{
    return s_roleNames;
}

int IntentServer::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object IntentServer::get(int index)

    Retrieves the model data at \a index as a JavaScript object. See the
    \l {IntentServer Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a index is invalid.

    \note This is very inefficient if you only want to access a single property from QML; use
          intent() instead to access the Intent object's properties directly.
*/
QVariantMap IntentServer::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "IntentServer::get(index): invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(qL1S(it.value()), data(this->index(index), it.key()));
    return map;
}

/*!
    \qmlmethod IntentObject IntentServer::intent(int index)

    Returns the \l{IntentObject}{intent} corresponding to the given \a index in the
    model, or \c null if the index is invalid.

    \note The object ownership of the returned Intent object stays with the application manager.
          If you want to store this pointer, you can use the IntentServer's QAbstractListModel
          signals or the intentAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Intent *IntentServer::intent(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "IntentServer::intent(index): invalid index:" << index;
        return nullptr;
    }
    return m_intents.at(index);
}

/*! \qmlmethod IntentObject IntentServer::applicationIntent(string intentId, string applicationId, var parameters)

    Returns the \l{IntentObject}{intent} corresponding to the given \a intentId, \a applicationId
    and \a parameters or \c null if the id does not exist.

    This method exposes the same functionality that is used internally to match incoming Intent
    requests for the intent identified by \a intentId and targeted for the application identified by
    \a applicationId.
    Although you could iterate over the intentList yourself in JavaScript, this function has the
    added benefit of also checking the optionally provided \a parameters against any given
    \l{IntentObject::parameterMatch}{parameter matches}.

    \note The object ownership of the returned Intent object stays with the application manager.
          If you want to store this pointer, you can use the IntentServer's QAbstractListModel
          signals or the intentAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Intent *IntentServer::applicationIntent(const QString &intentId, const QString &applicationId,
                             const QVariantMap &parameters) const
{
    auto it = std::find_if(m_intents.cbegin(), m_intents.cend(),
                           [intentId, applicationId, parameters](Intent *intent) -> bool {
        return (intent->applicationId() == applicationId) && (intent->intentId() == intentId)
                && intent->checkParameterMatch(parameters);
    });
    return (it != m_intents.cend()) ? *it : nullptr;
}

/*! \qmlmethod IntentObject IntentServer::packageIntent(string intentId, string packageId, var parameters)

    Returns the \l{IntentObject}{intent} corresponding to the given \a intentId, \a packageId
    and \a parameters or \c null if the id does not exist.

    \sa applicationIntent
*/
Intent *IntentServer::packageIntent(const QString &intentId, const QString &packageId,
                                    const QVariantMap &parameters) const
{
    auto it = std::find_if(m_intents.cbegin(), m_intents.cend(),
                           [intentId, packageId, parameters](Intent *intent) -> bool {
        return (intent->packageId() == packageId) && (intent->intentId() == intentId)
                && intent->checkParameterMatch(parameters);
    });
    return (it != m_intents.cend()) ? *it : nullptr;
}

/*! \qmlmethod IntentObject IntentServer::packageIntent(string intentId, string packageId, string applicationId, var parameters)

    Returns the \l{IntentObject}{intent} corresponding to the given \a intentId, \a packageId,
    \a applicationId and \a parameters or \c null if the id does not exist.

    \sa applicationIntent
*/
Intent *IntentServer::packageIntent(const QString &intentId, const QString &packageId,
                                    const QString &applicationId, const QVariantMap &parameters) const
{
    auto it = std::find_if(m_intents.cbegin(), m_intents.cend(),
                           [intentId, packageId, applicationId, parameters](Intent *intent) -> bool {
        return (intent->packageId() == packageId) && (intent->applicationId() == applicationId)
                && (intent->intentId() == intentId) && intent->checkParameterMatch(parameters);
    });
    return (it != m_intents.cend()) ? *it : nullptr;
}

/*! \qmlmethod int IntentServer::indexOfIntent(string intentId, string applicationId, var parameters)

    Maps the intent corresponding to the given \a intentId, \a applicationId and \a parameters to
    its position within this model. Returns \c -1 if the specified intent is invalid.

    \sa intent()
*/
int IntentServer::indexOfIntent(const QString &intentId, const QString &applicationId,
                                const QVariantMap &parameters) const
{
    return m_intents.indexOf(applicationIntent(intentId, applicationId, parameters));
}

/*! \qmlmethod int IntentServer::indexOfIntent(IntentObject intent)

    Maps the \a intent to its position within this model. Returns \c -1 if the specified intent is
    invalid.

    \sa intent()
*/
int IntentServer::indexOfIntent(Intent *intent)
{
    return m_intents.indexOf(intent);
}

void IntentServer::triggerRequestQueue()
{
    QMetaObject::invokeMethod(this, &IntentServer::processRequestQueue, Qt::QueuedConnection);
}

void IntentServer::enqueueRequest(IntentServerRequest *isr)
{
    qCDebug(LogIntents) << "Enqueueing Intent request:" << isr << isr->requestId() << isr->state();
    m_requestQueue.enqueue(isr);
    triggerRequestQueue();
}

void IntentServer::processRequestQueue()
{
    if (m_requestQueue.isEmpty())
        return;

    IntentServerRequest *isr = m_requestQueue.takeFirst();

    qCDebug(LogIntents) << "Processing intent request" << isr << isr->requestId() << "in state" << isr->state();

    if (isr->state() == IntentServerRequest::State::ReceivedRequest) { // step 1) disambiguate
        if (!isr->isBroadcast() && !isr->selectedIntent()) {
            // not disambiguated yet

            if (!isSignalConnected(QMetaMethod::fromSignal(&IntentServer::disambiguationRequest))) {
                // If the System UI does not react to the signal, then just use the first match.
                isr->setSelectedIntent(isr->potentialIntents().constFirst());
            } else {
                m_disambiguationQueue.enqueue(isr);
                isr->setState(IntentServerRequest::State::WaitingForDisambiguation);
                qCDebug(LogIntents) << "Waiting for disambiguation on intent" << isr->intentId();
                if (m_disambiguationTimeout > 0) {
                    QTimer::singleShot(m_disambiguationTimeout, this, [this, isr]() {
                        if (m_disambiguationQueue.removeOne(isr)) {
                            isr->setRequestFailed(qSL("Disambiguation timed out after %1 ms").arg(m_disambiguationTimeout));
                            enqueueRequest(isr);
                        }
                    });
                }
                emit disambiguationRequest(isr->requestId(), convertToQml(isr->potentialIntents()),
                                           isr->parameters());
            }
        }
        if (isr->isBroadcast() || isr->selectedIntent()) {
            qCDebug(LogIntents) << "No disambiguation necessary/required for intent" << isr->intentId();
            isr->setState(IntentServerRequest::State::Disambiguated);
        }
    }

    if (isr->state() == IntentServerRequest::State::Disambiguated) { // step 2) start app
        auto handlerIPC = m_systemInterface->findClientIpc(isr->selectedIntent()->applicationId());
        if (!handlerIPC) {
            qCDebug(LogIntents) << "Intent handler" << isr->selectedIntent()->applicationId() << "is not running";

            if (isr->potentialIntents().constFirst()->handleOnlyWhenRunning()) {
                qCDebug(LogIntents) << " * skipping, because 'handleOnlyWhenRunning' is set";
                isr->setRequestFailed(qSL("Skipping delivery due to handleOnlyWhenRunning"));
            } else {
                m_startingAppQueue.enqueue(isr);
                isr->setState(IntentServerRequest::State::WaitingForApplicationStart);
                if (m_startingAppTimeout > 0) {
                    QTimer::singleShot(m_startingAppTimeout, this, [this, isr]() {
                        if (m_startingAppQueue.removeOne(isr)) {
                            isr->setRequestFailed(qSL("Starting handler application timed out after %1 ms").arg(m_startingAppTimeout));
                            enqueueRequest(isr);
                        }
                    });
                }
                m_systemInterface->startApplication(isr->selectedIntent()->applicationId());
            }
        } else {
            qCDebug(LogIntents) << "Intent handler" << isr->selectedIntent()->applicationId() << "is already running";
            isr->setState(IntentServerRequest::State::StartedApplication);
        }
    }

    if (isr->state() == IntentServerRequest::State::StartedApplication) { // step 3) send request out
        auto clientIPC = m_systemInterface->findClientIpc(isr->selectedIntent()->applicationId());
        if (!clientIPC) {
            qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                  << isr->selectedIntent()->applicationId() << "to forward the intent request"
                                  << isr->requestId();
            isr->setRequestFailed(qSL("No IPC channel to reach target application."));
        } else {
            qCDebug(LogIntents) << "Sending intent request to handler application"
                                << isr->selectedIntent()->applicationId();
            if (!isr->isBroadcast()) {
                m_sentToAppQueue.enqueue(isr);
                isr->setState(IntentServerRequest::State::WaitingForReplyFromApplication);
                if (m_sentToAppTimeout > 0) {
                    QTimer::singleShot(m_sentToAppTimeout, this, [this, isr]() {
                        if (m_sentToAppQueue.removeOne(isr)) {
                            isr->setRequestFailed(qSL("Waiting for reply from handler application timed out after %1 ms").arg(m_sentToAppTimeout));
                            enqueueRequest(isr);
                        }
                    });
                }
            } else {
                // there are no replies for broadcasts, so we simply skip this step
                isr->setState(IntentServerRequest::State::ReceivedReplyFromApplication);
            }
            m_systemInterface->requestToApplication(clientIPC, isr);
        }
    }

    if (isr->state() == IntentServerRequest::State::ReceivedReplyFromApplication) { // step 5) send reply to requesting app
        if (!isr->isBroadcast()) {
            auto clientIPC = m_systemInterface->findClientIpc(isr->requestingApplicationId());
            if (!clientIPC) {
                qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                      << isr->requestingApplicationId() << "to forward the Intent reply"
                                      << isr->requestId();
            } else {
                qCDebug(LogIntents) << "Forwarding intent reply" << isr->requestId()
                                    << "to requesting application" << isr->requestingApplicationId();
                m_systemInterface->replyFromSystem(clientIPC, isr);
            }
        }
        QMetaObject::invokeMethod(this, [isr]() { delete isr; }, Qt::QueuedConnection); // aka deleteLater for non-QObject
        isr = nullptr;
    }

    triggerRequestQueue();
}

QList<QObject *> IntentServer::convertToQml(const QVector<Intent *> &intents)
{
    //TODO: we really should copy here, because the Intent pointers may die: a disambiguation might
    //      be active, while one of the apps involved is removed or updated

    QList<QObject *> ol;
    for (auto intent : intents)
        ol << intent;
    return ol;
}

QString IntentServer::packageIdForApplicationId(const QString &applicationId) const
{
    for (auto pit = m_knownApplications.cbegin(); pit != m_knownApplications.cend(); ++pit) {
        for (auto ait = pit.value().cbegin(); ait != pit.value().cend(); ++ait) {
            if (*ait == applicationId)
                return pit.key();
        }
    }
    return QString();
}

/*!
    \qmlsignal IntentServer::disambiguationRequest(uuid requestId, list<Intent> potentialIntents, var parameters)

    This signal is emitted when the IntentServer receives an intent request that could potentially
    be handled by more than one application.

    \note This signal is only emitted, if there is a receiver connected at all. If the signal is not
          connected, an arbitrary application from the list of potential matches will be chosen to
          handle this request.

    The receiver of this signal gets the requested \a requestId and its \a parameters. It can
    then either call acknowledgeDisambiguationRequest() to choose from one of the supplied \a
    potentialIntents or call rejectDisambiguationRequest() to reject the intent request completely.
    In both cases the unique \a requestId needs to be sent along to identify the intent request.

    Not calling one of these two functions will result in memory leaks.

    \sa IntentClient::sendIntentRequest
*/

/*! \qmlmethod IntentServer::acknowledgeDisambiguationRequest(uuid requestId, Intent selectedIntent)

    Tells the IntentServer to go ahead with the sender's intent request identified by \a requestId.
    The chosen \a selectedIntent needs to be one of the \c potentialIntents supplied to the
    receiver of the disambiguationRequest signal.

    \sa IntentClient::sendIntentRequest
*/
void IntentServer::acknowledgeDisambiguationRequest(const QUuid &requestId, Intent *selectedIntent)
{
    internalDisambiguateRequest(requestId, false, selectedIntent);
}


/*! \qmlmethod IntentServer::rejectDisambiguationRequest(uuid requestId)

    Tells the IntentServer to ignore the sender's intent request identified by \a requestId.
    The original sender will get an error reply back in this case.

    \sa IntentClient::sendIntentRequest
*/
void IntentServer::rejectDisambiguationRequest(const QUuid &requestId)
{
    internalDisambiguateRequest(requestId, true, nullptr);
}

void IntentServer::internalDisambiguateRequest(const QUuid &requestId, bool reject, Intent *selectedIntent)
{
    IntentServerRequest *isr = nullptr;
    for (int i = 0; i < m_disambiguationQueue.size(); ++i) {
        if (m_disambiguationQueue.at(i)->requestId() == requestId) {
            isr = m_disambiguationQueue.takeAt(i);
            break;
        }
    }

    if (!isr) {
        qmlWarning(this) << "Got a disambiguation acknowledge or reject for intent " << requestId
                         << ", but no disambiguation was expected for this intent";
    } else {
        if (reject) {
            isr->setRequestFailed(qSL("Disambiguation was rejected"));
        } else if (isr->potentialIntents().contains(selectedIntent)) {
            isr->setSelectedIntent(selectedIntent);
            isr->setState(IntentServerRequest::State::Disambiguated);
        } else {
            qCWarning(LogIntents) << "IntentServer::acknowledgeDisambiguationRequest for intent"
                                  << requestId << "tried to disambiguate to the intent" << selectedIntent->intentId()
                                  << "which was not in the list of potential disambiguations";

            isr->setRequestFailed(qSL("Failed to disambiguate"));
        }
        enqueueRequest(isr);
    }
}

void IntentServer::applicationWasStarted(const QString &applicationId)
{
    // check if any intent request is waiting for this app to start
    bool foundOne = false;
    for (auto it = m_startingAppQueue.cbegin(); it != m_startingAppQueue.cend(); ) {
        auto isr = *it;
        if (isr->selectedIntent()->applicationId() == applicationId) {
            qCDebug(LogIntents) << "Intent request" << isr->intentId()
                                << "can now be forwarded to application" << applicationId;

            isr->setState(IntentServerRequest::State::StartedApplication);
            m_requestQueue << isr;
            foundOne = true;

            it = m_startingAppQueue.erase(it);
        } else {
            ++it;
        }
    }
    if (foundOne)
        triggerRequestQueue();
}

void IntentServer::replyFromApplication(const QString &replyingApplicationId, const QUuid &requestId,
                                        bool error, const QVariantMap &result)
{
    IntentServerRequest *isr = nullptr;
    for (int i = 0; i < m_sentToAppQueue.size(); ++i) {
        if (m_sentToAppQueue.at(i)->requestId() == requestId) {
            isr = m_sentToAppQueue.takeAt(i);
            break;
        }
    }

    if (!isr) {
        qCWarning(LogIntents) << "Got a reply for intent" << requestId << "from application"
                              << replyingApplicationId << "but no reply was expected for this intent";
    } else {
        if (isr->selectedIntent() && (isr->selectedIntent()->applicationId() != replyingApplicationId)) {
            qCWarning(LogIntents) << "Got a reply for intent" << isr->requestId() << "from application"
                                  << replyingApplicationId << "but expected a reply from"
                                  << isr->selectedIntent()->applicationId() << "instead";
            isr->setRequestFailed(qSL("Request reply received from wrong application"));
        } else {
            QString errorMessage;
            if (error) {
                errorMessage = result.value(qSL("errorMessage")).toString();
                qCDebug(LogIntents) << "Got an error reply for intent" << isr->requestId() << "from application"
                                    << replyingApplicationId << ":" << errorMessage;
                isr->setRequestFailed(errorMessage);
            } else {
                qCDebug(LogIntents) << "Got a reply for intent" << isr->requestId() << "from application"
                                    << replyingApplicationId << ":" << result;
                isr->setRequestSucceeded(result);
            }
        }
        enqueueRequest(isr);
    }
}

IntentServerRequest *IntentServer::requestToSystem(const QString &requestingApplicationId,
                                                   const QString &intentId, const QString &applicationId,
                                                   const QVariantMap &parameters)
{
    qCDebug(LogIntents) << "Server: Incoming intent request" << intentId << "from application"
                        << requestingApplicationId << "to application" << applicationId;

    if (!m_systemInterface->findClientIpc(requestingApplicationId)) {
        qCWarning(LogIntents) << "Intent" << intentId << "was requested from unknown application"
                              << requestingApplicationId;
        return nullptr;
    }

    QVector<Intent *> intents;
    bool broadcast = (applicationId == qSL(":broadcast:"));
    if (applicationId.isEmpty() || broadcast) {
        intents = filterByIntentId(m_intents, intentId, parameters);
    } else {
        if (Intent *intent = this->applicationIntent(intentId, applicationId, parameters))
            intents << intent;
    }

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Unknown intent" << intentId << "was requested from application"
                              << requestingApplicationId;
        return nullptr;
    }

    intents = filterByRequestingApplicationId(intents, requestingApplicationId);

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Inaccessible intent" << intentId << "was requested from application"
                              << requestingApplicationId;
        return nullptr;
    }

    if (broadcast) {
        for (auto intent : std::as_const(intents)) {
            auto isr = new IntentServerRequest(requestingApplicationId, intentId, { intent }, parameters, broadcast);
            enqueueRequest(isr);
        }
        return nullptr; // this is not an error condition for broadcasts - there simply is no return value for the sender
    } else {
        auto isr = new IntentServerRequest(requestingApplicationId, intentId, intents, parameters, broadcast);
        enqueueRequest(isr);
        return isr;
    }
}

QT_END_NAMESPACE_AM

#include "moc_intentserver.cpp"
