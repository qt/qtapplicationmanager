// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:communication-protocol

#include "intentserver.h"
#include "intentserversysteminterface.h"
#include "intentserverrequest.h"
#include "intentmodel.h"

#include <QtAppManCommon/logging.h>
#include <QtAppManCommon/exception.h>

#include <algorithm>

#include <QRegularExpression>
#include <QUuid>
#include <QMetaObject>
#include <QTimer>
#include <QDebug>
#include <QScopedValueRollback>
#include <QJsonDocument>

#include <QQmlEngine>
#include <QQmlInfo>
#include <QJSEngine>
#include <QJSValueList>

#include <memory>

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype IntentServer
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The System UI side singleton representing the Intents sub-system.

    This singleton serves two purposes: for one, it gives the System UI access to the database of
    all the available intents via its item model API, plus it exposes the API to deal with ambiguous
    intent requests. Intent requests can be ambiguous if the requesting party only specified the \c
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
        \li \c description
        \li string
        \li The description of the intent. If possible, already translated to the current locale.
            If no description was defined for the intent, the description of the corresponding
            package will be returned.
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
    Description,
    Icon,
    Categories,
    IntentItem,
    IntentObject, // needed to keep the roles similar to PackageManager
};

class IntentDebug
{
public:
    IntentDebug(const QUuid &requestId, const QString &intentId,
                const QString &requestingApplicationId, const QString &handlingApplicationId)
        : m_requestId(requestId)
        , m_intentId(intentId)
        , m_requestingApplicationId(requestingApplicationId)
        , m_handlingApplicationId(handlingApplicationId)
    { }

    IntentDebug(const IntentServerRequest *isr)
        : m_requestId(isr->requestId())
        , m_intentId(isr->intentId())
        , m_requestingApplicationId(isr->requestingApplicationId())
        , m_handlingApplicationId(isr->selectedIntent() ? isr->selectedIntent()->applicationId() : QString())
    { }

    QUuid m_requestId;
    QString m_intentId;
    QString m_requestingApplicationId;
    QString m_handlingApplicationId;
};

class IntentParamsDebug
{
public:
    IntentParamsDebug(const IntentServerRequest *isr)
        : m_parameters(isr->parameters())
    { }

    QVariantMap m_parameters;
};

QDebug operator<<(QDebug ds, const IntentDebug &id)
{
    QDebugStateSaver save(ds);
    const QString reqId = id.m_requestId.isNull() ? u"-----------no-request-id------------"_s
                                                  : id.m_requestId.toString(QUuid::WithoutBraces);

    ds.nospace().noquote()
        << reqId << " [" << id.m_intentId << "] {"
        << (id.m_requestingApplicationId.isEmpty() ? u"?"_s : id.m_requestingApplicationId) << " -> "
        << (id.m_handlingApplicationId.isEmpty() ? u"?"_s : id.m_handlingApplicationId) << "}";
    return ds;
}

QDebug operator<<(QDebug ds, const IntentParamsDebug &ipd)
{
    QDebugStateSaver save(ds);
    QJsonDocument doc = QJsonDocument::fromVariant(ipd.m_parameters);
    ds.nospace().noquote() << doc.toJson(QJsonDocument::Compact);
    return ds;
}

QDebug operator<<(QDebug ds, IntentServerRequest::State state)
{
    ds << QMetaEnum::fromType<IntentServerRequest::State>().valueToKey(static_cast<int>(state));
    return ds;
}

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
        s_roleNames.insert(Description, "description");
        s_roleNames.insert(Icon, "icon");
        s_roleNames.insert(Categories, "categories");
        s_roleNames.insert(IntentItem, "intent");
        s_roleNames.insert(IntentObject, "intentObject");
    }
}

IntentServer::~IntentServer()
{
    qDeleteAll(m_requestQueue);
    qDeleteAll(m_awaitingDisambiguation);
    qDeleteAll(m_awaitingAppStart);
    qDeleteAll(m_awaitingAppReply);
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
            throw Exception("no id specified");
        if (packageId.isEmpty())
            throw Exception("no packageId specified");
        if (handlingApplicationId.isEmpty())
            throw Exception("no handlingApplicationId specified");
        if (!m_knownApplications.contains(packageId))
            throw Exception("packageId is not known");
        if (!m_knownApplications.value(packageId).contains(handlingApplicationId))
            throw Exception("applicationId is not known or not part of the specified package");
        if (applicationIntent(id, handlingApplicationId))
            throw Exception("intent with given id/handlingApplicationId already exists");
    } catch (const Exception &e) {
        qCWarning(LogIntentServer) << "Cannot add intent" << id << "in package" << packageId
                                   << "handled by" << handlingApplicationId << ":" << e.errorString();
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
    qsizetype index = m_intents.indexOf(intent);
    if (index < 0)
        return;

    if (m_aboutToBeRemoved) {
        qCFatal(LogIntentServer) << "IntentServer::removeIntent was called recursively";
        return;
    }
    QScopedValueRollback<bool> rollback(m_aboutToBeRemoved, true);

    emit intentAboutToBeRemoved(intent);
    beginRemoveRows(QModelIndex(), int(index), int(index));
    m_intents.removeAt(index);
    endRemoveRows();

    emit countChanged();

    delete intent;
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
                qCDebug(LogIntentServer) << IntentDebug(QUuid(), intent->intentId(),
                                                        requestingApplicationId, intent->applicationId())
                                         << "not considered, due to private visibility";
                return false;
            } else if (!intent->requiredCapabilities().isEmpty()
                       && !m_systemInterface->checkApplicationCapabilities(requestingApplicationId,
                                                                           intent->requiredCapabilities())) {
                qCDebug(LogIntentServer) << IntentDebug(QUuid(), intent->intentId(),
                                                        requestingApplicationId, intent->applicationId())
                                         << "not considered, due to missing capabilities of requesting application";
                return false;
            }

        }
        return true;
    });
    return result;
}

QVector<Intent *> IntentServer::filterByJSFunction(const QVector<Intent *> &intents,
                                                   const QString &requestingApplicationId) const
{
    if (!m_intentRequestFilterFunction.isCallable())
        return intents;

    if (!m_engine)
        m_engine = qjsEngine(this);
    if (!m_engine) {
        qCWarning(LogIntentServer) << "intentRequestFilterFunction is set, but no JavaScript engine is available";
        return intents;
    }

    QJSValue intentsArray = m_engine->newArray(quint32(intents.size()));
    for (qsizetype i = 0; i < intents.size(); ++i)
        intentsArray.setProperty(quint32(i), m_engine->newQObject(intents.at(i)));

    QJSValueList args = { intentsArray, QJSValue(requestingApplicationId) };
    return m_intentRequestFilterFunction.call(args).toBool() ? intents : QVector<Intent *>{};
}

int IntentServer::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_intents.count());
}

QVariant IntentServer::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return { };

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
    case Description:
        return intent->description();
    case Icon:
        return intent->icon();
    case Categories:
        return intent->categories();
    case IntentItem:
    case IntentObject:
        return QVariant::fromValue(intent);
    }
    return { };
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
    \qmlproperty var IntentServer::intentRequestFilterFunction

    A JavaScript function callback that, if set, is invoked once per intent request after the
    static visibility and capability filters have been applied. The callback receives two
    arguments:
    \list
        \li the list of remaining candidate \l{IntentObject}{IntentObjects}, and
        \li the id of the requesting application as a string.
    \endlist

    The callback must return a Boolean: \c true to let the request proceed with the supplied
    candidates, \c false to reject the request entirely. A rejected request fails the same way
    as if no handler had been registered.

    If no callback is set (the default), the request proceeds with the produced candidates
    unchanged.
*/
QJSValue IntentServer::intentRequestFilterFunction() const
{
    return m_intentRequestFilterFunction;
}

void IntentServer::setIntentRequestFilterFunction(const QJSValue &callback)
{
    if (!callback.equals(m_intentRequestFilterFunction)) {
        m_intentRequestFilterFunction = callback;
        emit intentRequestFilterFunctionChanged();
    }
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
        return { };
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromLatin1(it.value()), data(this->index(index), it.key()));
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
    return int(m_intents.indexOf(applicationIntent(intentId, applicationId, parameters)));
}

/*! \qmlmethod int IntentServer::indexOfIntent(IntentObject intent)

    Maps the \a intent to its position within this model. Returns \c -1 if the specified intent is
    invalid.

    \sa intent()
*/
int IntentServer::indexOfIntent(Intent *intent)
{
    return int(m_intents.indexOf(intent));
}

void IntentServer::triggerRequestQueue()
{
    QMetaObject::invokeMethod(this, &IntentServer::processRequestQueue, Qt::QueuedConnection);
}

void IntentServer::enqueueRequest(IntentServerRequest *isr)
{
    m_requestQueue.enqueue(isr);
    triggerRequestQueue();
}

void IntentServer::processRequestQueue()
{
    if (m_requestQueue.isEmpty())
        return;

    IntentServerRequest *isr = m_requestQueue.takeFirst();

    qCDebug(LogIntentServer) << IntentDebug(isr) << "is now in state" << isr->state();

    if (isr->state() == IntentServerRequest::State::ReceivedRequest) { // step 1) disambiguate
        qCDebug(LogIntentParams) << IntentDebug(isr) << "params:" << IntentParamsDebug(isr);

        if (!isr->isBroadcast() && !isr->selectedIntent()) {
            // not disambiguated yet

            if (!isSignalConnected(QMetaMethod::fromSignal(&IntentServer::disambiguationRequest))) {
                qCWarning(LogIntentServer) << IntentDebug(isr)
                                           << "requires disambiguation, but no receiver is connected to the disambiguationRequest signal";
                isr->setRequestFailed(u"Disambiguation required, but the System UI does not handle disambiguationRequest"_s);
            } else {
                m_awaitingDisambiguation.insert(isr->requestId(), isr);
                isr->setState(IntentServerRequest::State::WaitingForDisambiguation);
                qCDebug(LogIntentServer) << IntentDebug(isr) << "is now in state" << isr->state();

                if (m_disambiguationTimeout > 0) {
                    QTimer::singleShot(m_disambiguationTimeout, this, [this, requestId = isr->requestId()]() {
                        if (auto *pisr = m_awaitingDisambiguation.take(requestId)) {
                            pisr->setRequestFailed(u"Disambiguation timed out after %1 ms"_s.arg(m_disambiguationTimeout));
                            enqueueRequest(pisr);
                        }
                    });
                }
                emit disambiguationRequest(isr->requestId().toString(), isr->potentialIntents(),
                                           isr->parameters());
            }
        }
        if (isr->isBroadcast() || isr->selectedIntent()) {
            qCDebug(LogIntentServer) << IntentDebug(isr) << "does not require disambiguation";
            isr->setState(IntentServerRequest::State::Disambiguated);
        }
    }

    if (isr->state() == IntentServerRequest::State::Disambiguated) { // step 2) start app
        auto handlerIPC = m_systemInterface->findClientIpc(isr->selectedIntent()->applicationId());
        if (!handlerIPC) {

            if (isr->selectedIntent()->handleOnlyWhenRunning()) {
                qCDebug(LogIntentServer) << IntentDebug(isr) << "would need to start the handling application, but it's 'handleOnlyWhenRunning'";
                isr->setRequestFailed(u"Skipping delivery due to handleOnlyWhenRunning"_s);
            } else {
                m_awaitingAppStart.append(isr);
                isr->setState(IntentServerRequest::State::WaitingForApplicationStart);
                qCDebug(LogIntentServer) << IntentDebug(isr) << "is now in state" << isr->state();

                if (m_startingAppTimeout > 0) {
                    QTimer::singleShot(m_startingAppTimeout, this, [this, pisr = QPointer(isr)]() {
                        if (pisr && m_awaitingAppStart.removeOne(pisr)) {
                            qCDebug(LogIntentServer) << IntentDebug(pisr.get()) << "starting handler application timed out";
                            pisr->setRequestFailed(u"Starting handler application timed out after %1 ms"_s.arg(m_startingAppTimeout));
                            enqueueRequest(pisr);
                        }
                    });
                }
                m_systemInterface->startApplication(isr->selectedIntent()->applicationId());
            }
        } else {
            qCDebug(LogIntentServer) << IntentDebug(isr) << "handling application already running";
            isr->setState(IntentServerRequest::State::StartedApplication);
        }
    }

    if (isr->state() == IntentServerRequest::State::StartedApplication) { // step 3) send request out
        auto clientIPC = m_systemInterface->findClientIpc(isr->selectedIntent()->applicationId());
        if (!clientIPC) {
            qCWarning(LogIntentServer) << IntentDebug(isr) << "could not find an IPC connection for handling application to forward the intent request to";
            isr->setRequestFailed(u"No IPC channel to reach handling application."_s);
        } else {
            qCDebug(LogIntentServer) << IntentDebug(isr) << "sending intent request to handling application";
            if (!isr->isBroadcast()) {
                m_awaitingAppReply.insert(isr->requestId(), isr);
                isr->setState(IntentServerRequest::State::WaitingForReplyFromApplication);
                if (m_sentToAppTimeout > 0) {
                    QTimer::singleShot(m_sentToAppTimeout, this, [this, requestId = isr->requestId()]() {
                        if (auto *pisr = m_awaitingAppReply.take(requestId)) {
                            qCDebug(LogIntentServer) << IntentDebug(pisr) << "waiting for reply from handler application timed out";
                            pisr->setRequestFailed(u"Waiting for reply from handler application timed out after %1 ms"_s.arg(m_sentToAppTimeout));
                            enqueueRequest(pisr);
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
            qCDebug(LogIntentParams) << IntentDebug(isr) << "reply:" << IntentParamsDebug(isr);

            auto clientIPC = m_systemInterface->findClientIpc(isr->requestingApplicationId());
            if (!clientIPC) {
                qCWarning(LogIntentServer) << IntentDebug(isr) << "could not find an IPC connection for requesting application to forward the intent reply to";
            } else {
                qCDebug(LogIntentServer) << IntentDebug(isr) << "forwarding intent reply to requesting application";
                m_systemInterface->replyFromSystem(clientIPC, isr);
            }
        }
        isr->deleteLater();
    }

    triggerRequestQueue();
}

QString IntentServer::packageIdForApplicationId(const QString &applicationId) const
{
    for (auto pit = m_knownApplications.cbegin(); pit != m_knownApplications.cend(); ++pit) {
        for (auto ait = pit.value().cbegin(); ait != pit.value().cend(); ++ait) {
            if (*ait == applicationId)
                return pit.key();
        }
    }
    return { };
}

/*!
    \qmlsignal IntentServer::disambiguationRequest(string requestId, list<Intent> potentialIntents, var parameters)

    This signal is emitted when the IntentServer receives an intent request that could potentially
    be handled by more than one application.

    \note If no receiver is connected to this signal, the IntentServer will refuse any request that
          would require disambiguation and fail it with an error message. Before 6.12, an arbitrary
          candidate would be chosen to handle this request, which could lead to unexpected behavior
          and security issues, as this would allow an application to "steal" intents from other
          applications by registering the same intent id and relying on the requester to omit the
          target applicationId.

    The receiver of this signal gets the requested \a requestId and its \a parameters. It can
    then either call acknowledgeDisambiguationRequest() to choose from one of the supplied \a
    potentialIntents or call rejectDisambiguationRequest() to reject the intent request completely.
    In both cases the unique \a requestId needs to be sent along to identify the intent request.

    Not calling one of these two functions will result in memory leaks.

    \sa IntentClient::sendIntentRequest
*/

/*! \qmlmethod void IntentServer::acknowledgeDisambiguationRequest(string requestId, Intent selectedIntent)

    Tells the IntentServer to go ahead with the sender's intent request identified by \a requestId.
    The chosen \a selectedIntent needs to be one of the \c potentialIntents supplied to the
    receiver of the disambiguationRequest signal.

    \sa IntentClient::sendIntentRequest
*/
void IntentServer::acknowledgeDisambiguationRequest(const QString &requestId, Intent *selectedIntent)
{
    internalDisambiguateRequest(QUuid::fromString(requestId), false, selectedIntent);
}


/*! \qmlmethod void IntentServer::rejectDisambiguationRequest(string requestId)

    Tells the IntentServer to ignore the sender's intent request identified by \a requestId.
    The original sender will get an error reply back in this case.

    \sa IntentClient::sendIntentRequest
*/
void IntentServer::rejectDisambiguationRequest(const QString &requestId)
{
    internalDisambiguateRequest(QUuid::fromString(requestId), true, nullptr);
}

void IntentServer::internalDisambiguateRequest(const QUuid &requestId, bool reject, Intent *selectedIntent)
{
    IntentServerRequest *isr = m_awaitingDisambiguation.take(requestId);

    if (!isr) {
        qmlWarning(this) << "Got a disambiguation acknowledge or reject for intent " << requestId
                         << ", but no disambiguation was expected for this intent";
    } else {
        if (reject) {
            isr->setRequestFailed(u"Disambiguation was rejected"_s);
        } else if (isr->potentialIntents().contains(selectedIntent)) {
            isr->setSelectedIntent(selectedIntent);
            isr->setState(IntentServerRequest::State::Disambiguated);
        } else {
            qCWarning(LogIntentServer).nospace().noquote()
                << IntentDebug(isr)
                << " disambiguated intent id [" << selectedIntent->intentId() << "] is not valid";
            isr->setRequestFailed(u"Failed to disambiguate"_s);
        }
        enqueueRequest(isr);
    }
}

void IntentServer::applicationWasStarted(const QString &applicationId)
{
    // check if any intent request is waiting for this app to start
    bool foundOne = false;
    for (auto it = m_awaitingAppStart.cbegin(); it != m_awaitingAppStart.cend(); ) {
        auto isr = *it;
        if (isr->selectedIntent()->applicationId() == applicationId) {
            isr->setState(IntentServerRequest::State::StartedApplication);
            m_requestQueue << isr;
            foundOne = true;

            it = m_awaitingAppStart.erase(it); // clazy:exclude=strict-iterators
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
    IntentServerRequest *isr = m_awaitingAppReply.take(requestId);

    if (!isr) {
        qCWarning(LogIntentServer).nospace().noquote()
            << requestId.toString(QUuid::WithoutBraces)
            << " [?] {? -> " << replyingApplicationId << "} "
            << "received a reply, but this request ID is not known";
    } else {
        if (isr->selectedIntent() && (isr->selectedIntent()->applicationId() != replyingApplicationId)) {
            qCWarning(LogIntentServer) << IntentDebug(isr) << "received a reply from the wrong application:"
                                       << replyingApplicationId;
            isr->setRequestFailed(u"Request reply received from wrong application"_s);
        } else {
            QString errorMessage;
            if (error) {
                errorMessage = result.value(u"errorMessage"_s).toString();
                isr->setRequestFailed(errorMessage);
            } else {
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
    auto printWarning = [requestingApplicationId, intentId, applicationId](const char *reason) {
        qCWarning(LogIntentServer) << IntentDebug(QUuid(), intentId, requestingApplicationId, applicationId)
                                   << reason;
    };

    if (!m_systemInterface->findClientIpc(requestingApplicationId)) {
        printWarning("was requested from unknown application");
        return nullptr;
    }

    QVector<Intent *> intents;
    bool broadcast = (applicationId == u":broadcast:");
    if (applicationId.isEmpty() || broadcast) {
        intents = filterByIntentId(m_intents, intentId, parameters);
    } else {
        if (Intent *intent = this->applicationIntent(intentId, applicationId, parameters))
            intents << intent;
    }

    if (intents.isEmpty()) {
        printWarning("is an unknown intent");
        return nullptr;
    }

    intents = filterByRequestingApplicationId(intents, requestingApplicationId);

    if (intents.isEmpty()) {
        printWarning("is not accessible for the requesting application");
        return nullptr;
    }

    intents = filterByJSFunction(intents, requestingApplicationId);

    if (intents.isEmpty()) {
        printWarning("was rejected by the intentRequestFilterFunction");
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
