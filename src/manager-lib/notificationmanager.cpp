// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QVariant>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <QScopedValueRollback>

#include "global.h"
#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "notificationmanager.h"
#include "qml-utilities.h"
#include "dbus-utilities.h"
#include "package.h"
#include "notificationmodel.h"
#include "qmlinprocnotificationimpl.h"

using namespace Qt::StringLiterals;

/*!
    \qmltype NotificationManager
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The notification model, which handles freedesktop.org compliant notification requests.

    The NotificationManager singleton type provides a QML API only.

    The type is derived from QAbstractListModel and can be directly used as
    a model in notification views.

    Each item in this model corresponds to an active notification.

    \target NotificationManager Roles

    The following roles are available in this model - also take a look at the
    \l {org.freedesktop.Notifications} {freedesktop.org specification} for an
    in-depth explanation of these fields and how clients should populate them:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c id
        \li int
        \li The unique id of this notification.
    \row
        \li \c applicationId
        \li string
        \li The id of the application that created this notification. This can be used to look up
            information about the application in the ApplicationManager model.
            \note The \c applicationId role is neither unique within this model, nor is it
                  guaranteed to be valid. A single application can have multiple active
                  notifications, and on the other hand, system-notifications have no
                  application context at all.
    \row
        \li \c priority
        \li int
        \li See the client side documentation of Notification::priority
    \row
        \li \c summary
        \li string
        \li See the client side documentation of Notification::summary
    \row
        \li \c body
        \li string
        \li See the client side documentation of Notification::body
    \row
        \li \c category
        \li string
        \li See the client side documentation of Notification::category
    \row
        \li \c icon
        \li url
        \li See the client side documentation of Notification::icon
    \row
        \li \c image
        \li url
        \li See the client side documentation of Notification::image
    \row
        \li \c actions
        \li object
        \li See the client side documentation of Notification::actions. This is a list where each
            item is a single property (the key is the actionId and the value the actionText).
            Easier access is provided with the actionList role below.
    \row
        \li \c actionList
        \li object
        \li See the client side documentation of Notification::actions. In contrast to the
            \c actions object above actions are provided as a list with \c actionId and
            \c actionText properties
    \row
        \li \c showActionsAsIcons
        \li bool
        \li See the client side documentation of Notification::showActionsAsIcons
    \row
        \li \c dismissOnAction
        \li bool
        \li See the client side documentation of Notification::dismissOnAction
    \row
        \li \c isAcknowledgeable
        \li bool
        \li See the client side documentation of Notification::acknowledgeable
            For backwards compatibility, \c isClickable can also be used to refer to this role.
    \row
        \li \c isSytemNotification
        \li url
        \li Holds \c true for notifications originating not from an application, but some system
            service. Always holds \c false for notifications coming from UI applications.
    \row
        \li \c isShowingProgress
        \li bool
        \li See the client side documentation of Notification::showProgress
    \row
        \li \c progress
        \li qreal
        \li See the client side documentation of Notification::progress
    \row
        \li \c isSticky
        \li bool
        \li See the client side documentation of Notification::sticky
    \row
        \li \c timeout
        \li int
        \li See the client side documentation of Notification::timeout
    \row
        \li \c extended
        \li object
        \li See the client side documentation of Notification::extended
    \row
        \li \c created
        \li date
        \li The timestamp denoting when the notification was created
    \row
        \li \c updated
        \li date
        \li The timestamp denoting when the notification was last modififed

    \endtable

    The actual backend implementation that is receiving the notifications from other process is
    fully compliant to the D-Bus interface of the \l {org.freedesktop.Notifications}
    {freedesktop.org specification} for notifications.

    For testing purposes, the \e notify-send tool from the \e libnotify package can be used to create
    notifications.
*/
/*!
    \qmlsignal NotificationManager::notificationAdded(uint id)

    This signal is emitted after a new notification, identified by \a id, has been added.
    The model has already been update before this signal is sent out.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          NotificationManager model: notificationAdded, notificationAboutToBeRemoved and
          notificationChanged.
*/

/*!
    \qmlsignal NotificationManager::notificationAboutToBeRemoved(uint id)

    This signal is emitted before an existing notification, identified by \a id, is removed from the
    model.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          NotificationManager model: notificationAdded, notificationAboutToBeRemoved and
          notificationChanged.
*/

/*!
    \qmlsignal NotificationManager::notificationChanged(uint id, list<string> changedRoles)

    Emitted whenever one or more data roles, denoted by \a changedRoles, changed on the notification
    identified by \a id. An empty list in the \a changedRoles argument means that all roles should
    be considered modified.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          NotificationManager model: notificationAdded, notificationAboutToBeRemoved and
          notificationChanged.
*/


QT_BEGIN_NAMESPACE_AM

namespace {
enum NMRoles
{
    Id = Qt::UserRole + 2000,

    ApplicationId,
    Priority, // 0: base, bigger number: increasing importance
    Summary,
    Body,
    Category,

    Icon,
    Image,

    ShowActionsAsIcons,
    Actions,
    ActionList,
    DismissOnAction,

    IsAcknowledgeable,
    IsClickable, // legacy, replaced by IsAcknowledgeable
    IsSystemNotification,

    IsShowingProgress,
    Progress, // 0..1 or -1 == busy

    IsSticky, // if timeout == 0
    Timeout, // in msec

    Extended, // QVariantMap

    Created,
    Updated
};
}

struct NotificationData
{
    uint id = 0;
    Application *application = nullptr;
    uint priority = 0;
    QString summary;
    QString body;
    QString category;
    QString iconUrl;
    QString imageUrl;
    bool showActionIcons = false;
    QVariantList actions; // list of single element maps: <id (as string) --> text (as string)>
    bool dismissOnAction = false;
    bool isSticky = false;
    bool isSystemNotification = false;
    bool isShowingProgress = false;
    qreal progress = 0.0;
    int timeout = 0;
    QVariantMap extended;
    QDateTime created;
    QDateTime updated;

    QTimer *timer = nullptr;
};

enum CloseReason
{
    TimeoutExpired = 1,
    UserDismissed = 2,
    CloseNotificationCalled = 3
};


class NotificationManagerPrivate
{
public:
    ~NotificationManagerPrivate()
    {
        qDeleteAll(notifications);
        notifications.clear();
    }

    int findNotificationById(uint id) const
    {
        for (int i = 0; i < notifications.count(); ++i) {
            if (notifications.at(i)->id == id)
                return i;
        }
        return -1;
    }

    void closeNotification(uint id, CloseReason reason);

    NotificationManager *q = nullptr;
    QHash<int, QByteArray> roleNames;
    QList<NotificationData *> notifications;
    bool aboutToBeRemoved = false;
};

NotificationManager *NotificationManager::s_instance = nullptr;


NotificationManager *NotificationManager::createInstance()
{
    if (Q_UNLIKELY(s_instance))
        qFatal("NotificationManager::createInstance() was called a second time.");

    NotificationImpl::setFactory([](Notification *notification, const QString &applicationId) {
        return new QmlInProcNotificationImpl(notification, applicationId);
    });

    return s_instance = new NotificationManager();
}

NotificationManager *NotificationManager::instance()
{
    if (!s_instance)
        qFatal("NotificationManager::instance() was called before createInstance().");
    return s_instance;
}

NotificationManager::NotificationManager(QObject *parent)
    : QAbstractListModel(parent)
    , d(new NotificationManagerPrivate())
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &NotificationManager::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &NotificationManager::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &NotificationManager::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &NotificationManager::countChanged);

    d->q = this;
    d->roleNames.insert(NMRoles::Id, "id");
    d->roleNames.insert(NMRoles::ApplicationId, "applicationId");
    d->roleNames.insert(NMRoles::Priority, "priority");
    d->roleNames.insert(NMRoles::Summary, "summary");
    d->roleNames.insert(NMRoles::Body, "body");
    d->roleNames.insert(NMRoles::Category, "category");
    d->roleNames.insert(NMRoles::Icon, "icon");
    d->roleNames.insert(NMRoles::Image, "image");
    d->roleNames.insert(NMRoles::ShowActionsAsIcons, "showActionsAsIcons");
    d->roleNames.insert(NMRoles::Actions, "actions");
    d->roleNames.insert(NMRoles::ActionList, "actionList");
    d->roleNames.insert(NMRoles::DismissOnAction, "dismissOnAction");
    d->roleNames.insert(NMRoles::IsAcknowledgeable, "isAcknowledgeable");
    d->roleNames.insert(NMRoles::IsClickable, "isClickable");
    d->roleNames.insert(NMRoles::IsSystemNotification, "isSytemNotification");
    d->roleNames.insert(NMRoles::IsShowingProgress, "isShowingProgress");
    d->roleNames.insert(NMRoles::Progress, "progress");
    d->roleNames.insert(NMRoles::IsSticky, "isSticky");
    d->roleNames.insert(NMRoles::Timeout, "timeout");
    d->roleNames.insert(NMRoles::Extended, "extended");
    d->roleNames.insert(NMRoles::Created, "created");
    d->roleNames.insert(NMRoles::Updated, "updated");
}

NotificationManager::~NotificationManager()
{
    delete d;
    s_instance = nullptr;
}

int NotificationManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(d->notifications.count());
}

QVariant NotificationManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return { };

    const NotificationData *n = d->notifications.at(index.row());

    switch (role) {
    case NMRoles::Id:
        return n->id;
    case NMRoles::ApplicationId:
        return n->application ? n->application->id() : QString();
    case NMRoles::Priority:
        return n->priority;
    case NMRoles::Summary:
        return n->summary;
    case NMRoles::Body:
        return n->body;
    case NMRoles::Category:
        return n->category;
    case NMRoles::Icon:
         if (!n->iconUrl.isEmpty())
             return n->iconUrl;
         return n->application && n->application->package() ? n->application->package()->icon()
                                                            : QString();
    case NMRoles::Image:
        return n->imageUrl;
    case NMRoles::ShowActionsAsIcons:
        return n->showActionIcons;
    case NMRoles::Actions: {
        QVariantList actions = n->actions;
        actions.removeAll(QVariantMap { { u"default"_s, QString() } });
        return actions;
    }
    case NMRoles::ActionList: {
        QVariantList actionList;
        for (const auto &e : std::as_const(n->actions)) {
            const QString key = e.toMap().keys()[0];
            const QString value = e.toMap().value(key).toString();
            if (key != u"default"_s || !value.isEmpty())
                actionList.append(QVariantMap { { u"actionId"_s, key }, { u"actionText"_s, value } });
        }
        return actionList;
    }
    case NMRoles::DismissOnAction:
        return n->dismissOnAction;
    case NMRoles::IsClickable: // legacy
    case NMRoles::IsAcknowledgeable:
        return n->actions.contains(QVariantMap { { u"default"_s, QString() } });
    case NMRoles::IsSystemNotification:
        return n->isSystemNotification;
    case NMRoles::IsShowingProgress:
        return n->isShowingProgress;
    case NMRoles::Progress:
        return n->isShowingProgress ? n->progress : -1;
    case NMRoles::IsSticky:
        return n->timeout == 0;
    case NMRoles::Timeout:
        return n->timeout;
    case NMRoles::Extended:
        return n->extended;
    case NMRoles::Created:
        return n->created;
    case NMRoles::Updated:
        return n->updated;
    }
    return { };
}

QHash<int, QByteArray> NotificationManager::roleNames() const
{
    return d->roleNames;
}

/*!
    \qmlproperty int NotificationManager::count
    \readonly

    This property holds the number of active notifications in the model.
*/
int NotificationManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object NotificationManager::get(int index) const

    Retrieves the model data at \a index as a JavaScript object. See the \l {NotificationManager
    Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a index is invalid.
*/
QVariantMap NotificationManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "NotificationManager::get(index): invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromLatin1(it.value()), data(QAbstractListModel::index(index), it.key()));
    return map;
}

/*!
    \qmlmethod object NotificationManager::notification(int id)

    Retrieves the model data for the notification identified by \a id as a JavaScript object.
    See the \l {NotificationManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a id is invalid.
*/
QVariantMap NotificationManager::notification(uint id) const
{
    return get(indexOfNotification(id));
}

/*!
    \qmlmethod int NotificationManager::indexOfNotification(int id)

    Maps the notification \a id to its position within the model.

    Returns \c -1 if the specified \a id is invalid.
*/
int NotificationManager::indexOfNotification(uint id) const
{
    return d->findNotificationById(id);
}

/*!
    \qmlmethod NotificationManager::acknowledgeNotification(int id)

    This function needs to be called by the System UI when the user acknowledged the notification
    identified by \a id (most likely by clicking on it).
*/
void NotificationManager::acknowledgeNotification(uint id)
{
    triggerNotificationAction(id, u"default"_s);
}

/*!
    \qmlmethod NotificationManager::triggerNotificationAction(int id, string actionId)

    This function needs to be called by the System UI when the user triggered a notification action.
    Unless the notification is made resident (\c {dismissOnAction} is \c false), the notification
    will also be dismissed.

    The notification is identified by \a id and the action by \a actionId.

    \note You should only use action-ids that have been set for the the given notification (see the
    \l {Notification::}{actions} role). However, the application manager will accept and forward any
    arbitrary string. Be aware that this string is broadcast on the session D-Bus when running in
    multi-process mode.

    \sa dismissNotification()
*/
void NotificationManager::triggerNotificationAction(uint id, const QString &actionId)
{
    int i = d->findNotificationById(id);

    if (i >= 0) {
        NotificationData *n = d->notifications.at(i);
        bool found = false;
        for (auto it = n->actions.cbegin(); it != n->actions.cend(); ++it) {
            const QVariantMap map = (*it).toMap();
            Q_ASSERT(map.size() == 1);
            if (map.constBegin().key() == actionId) {
                found = true;
                break;
            }
        }
        if (!found) {
            qCDebug(LogNotifications) << "Requested to trigger a notification action, but the action is not registered:"
                                      << (actionId.length() > 20 ? (actionId.left(20) + u"..."_s) : actionId);
        }
        emit internalSignals.actionInvoked(id, actionId);

        if (n->dismissOnAction)
            dismissNotification(id);
    }
}

/*!
    \qmlmethod NotificationManager::dismissNotification(int id)

    This function needs to be called by the System UI when the notification identified by \a id is
    no longer needed.

    The creator of the notification will be notified about this dismissal.
*/
void NotificationManager::dismissNotification(uint id)
{
    d->closeNotification(id, UserDismissed);
}

/*! \internal
    Someone wants to create or update a notification: either via D-Bus or the in-process QML API
*/
uint NotificationManager::showNotification(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions,
                                 const QVariantMap &hints, int timeout)
{
    static uint idCounter = 0;

    auto now = QDateTime::currentDateTime();

    qCDebug(LogNotifications) << "Notify" << app_name << replaces_id << app_icon << summary << body << actions << hints
                              << now << timeout;

    Application *app = ApplicationManager::instance()->fromId(app_name);
    NotificationData *n;
    if (replaces_id) {
        int i = d->findNotificationById(replaces_id);

        if (i < 0) {
            qCDebug(LogNotifications) << "  -> failed to update existing notification";
            return 0;
        }
        n = d->notifications.at(i);

        if (app != n->application) {
            qCDebug(LogNotifications) << "  -> failed to update notification, due to hijacking attempt";
            return 0;
        }

        n->updated = now;

        qCDebug(LogNotifications) << "  -> updating existing notification";
    } else {
        n = new NotificationData;
        n->id = ++idCounter;
        n->created = now;
        n->updated = now;

        qCDebug(LogNotifications) << "  -> adding new notification with id" << n->id;
    }

    n->application = app;
    n->priority = hints.value(u"urgency"_s, QVariant(0)).toUInt();
    n->summary = summary;
    n->body = body;
    n->category = hints.value(u"category"_s).toString();
    n->iconUrl = app_icon;

    if (hints.contains(u"image-data"_s)) {
        //TODO: how can we parse this - the dbus sig of value is "(iiibiiay)"
    } else if (hints.contains(u"image-path"_s)) {
        n->imageUrl = hints.value(u"image-path"_s).toString();
    }

    n->showActionIcons = hints.value(u"action-icons"_s).toBool();
    n->actions.clear();
    for (int ai = 0; ai != (actions.size() & ~1); ai += 2)
        n->actions.append(QVariantMap { { actions.at(ai), actions.at(ai + 1) } });
    n->dismissOnAction = !hints.value(u"resident"_s).toBool();

    n->isSystemNotification = hints.value(u"x-pelagicore-system-notification"_s).toBool();
    n->isShowingProgress = hints.value(u"x-pelagicore-show-progress"_s).toBool();
    n->progress = hints.value(u"x-pelagicore-progress"_s).toReal();
    n->timeout = qMax(0, timeout);
    n->extended = convertFromDBusVariant(hints.value(u"x-pelagicore-extended"_s)).toMap();

    if (replaces_id == 0) { // new notification
        // we need to delay the model update until the client has a valid id
        QMetaObject::invokeMethod(this, [this, n, timeout]() {
                    notifyHelper(n, false, timeout);
                }, Qt::QueuedConnection);
        return n->id;
    } else {
        return notifyHelper(n, true, timeout);
    }
}

uint NotificationManager::notifyHelper(NotificationData *n, bool replaces, int timeout)
{
    uint id = n->id;
    Q_ASSERT(id);

    if (replaces) {
        QModelIndex idx = index(int(d->notifications.indexOf(n)), 0);
        emit dataChanged(idx, idx);
        static const auto nChanged = QMetaMethod::fromSignal(&NotificationManager::notificationChanged);
        if (isSignalConnected(nChanged)) {
            // we could do better here and actually find out which fields changed...
            emit notificationChanged(n->id, QStringList());
        }
    } else {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        d->notifications << n;
        endInsertRows();
        emit notificationAdded(n->id);
    }

    if (timeout > 0) {
        delete n->timer;
        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this, id, t]() {
                    d->closeNotification(id, TimeoutExpired);
                    t->deleteLater();
                });
        t->start(timeout);
        n->timer = t;
    }

    qCDebug(LogNotifications) << "  -> returning id" << id;
    return id;
}

/*! \internal
    Someone wants to close a notification: either via D-Bus or the in-process QML API
*/
void NotificationManager::closeNotification(uint id)
{
    d->closeNotification(id, CloseNotificationCalled);
}

void NotificationManagerPrivate::closeNotification(uint id, CloseReason reason)
{
    qCDebug(LogNotifications) << "Close id:" << id << "reason:" << reason;

    int i = findNotificationById(id);
    if (i < 0)
        return;

    if (aboutToBeRemoved) {
        qCFatal(LogNotifications) << "NotificationManager::closeNotification() was called recursively.";
        return;
    }
    QScopedValueRollback<bool> rollback(aboutToBeRemoved, true);

    emit q->notificationAboutToBeRemoved(id);

    q->beginRemoveRows(QModelIndex(), i, i);
    auto n = notifications.takeAt(i);
    q->endRemoveRows();

    emit q->internalSignals.notificationClosed(id, uint(reason));

    qCDebug(LogNotifications) << "Deleting notification with id:" << id;
    delete n;
}

QT_END_NAMESPACE_AM

#include "moc_notificationmanager.cpp"
