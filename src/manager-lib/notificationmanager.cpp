/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QVariant>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>

#include "global.h"
#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "notificationmanager.h"
#include "qml-utilities.h"
#include "dbus-utilities.h"
#include "package.h"

/*!
    \externalpage https://developer.gnome.org/notification-spec/
    \title freedesktop.org specification
*/
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
    \l {freedesktop.org specification} for an in-depth explanation of these fields
    and how clients should populate them:

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
        \li See the client side documentation of Notification::actions
    \row
        \li \c showActionsAsIcons
        \li bool
        \li See the client side documentation of Notification::showActionsAsIcons
    \row
        \li \c dismissOnAction
        \li bool
        \li See the client side documentation of Notification::dismissOnAction
    \row
        \li \c isClickable
        \li bool
        \li See the client side documentation of Notification::clickable
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

    \endtable

    The actual backend implementation that is receiving the notifications from other process is
    fully compliant to the D-Bus interface of the \l {freedesktop.org specification} for
    notifications.

    For testing purposes, the \e notify-send tool from the \e libnotify package can be used to create
    notifications.
*/


QT_BEGIN_NAMESPACE_AM

namespace {
enum Roles
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
    DismissOnAction,

    IsClickable,
    IsSystemNotification,

    IsShowingProgress,
    Progress, // 0..1 or -1 == busy

    IsSticky, // if timeout == 0
    Timeout, // in msec

    Extended // QVariantMap
};
}

struct NotificationData
{
    uint id;
    Application *application;
    uint priority;
    QString summary;
    QString body;
    QString category;
    QString iconUrl;
    QString imageUrl;
    bool showActionIcons;
    QVariantList actions; // list of single element maps: <id (as string) --> text (as string)>
    bool dismissOnAction;
    bool isSticky;
    bool isClickable;
    bool isSystemNotification;
    bool isShowingProgress;
    qreal progress;
    int timeout;
    QVariantMap extended;

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

    NotificationManager *q;
    QHash<int, QByteArray> roleNames;
    QList<NotificationData *> notifications;
};

NotificationManager *NotificationManager::s_instance = nullptr;


NotificationManager *NotificationManager::createInstance()
{
    if (Q_UNLIKELY(s_instance))
        qFatal("NotificationManager::createInstance() was called a second time.");

    qmlRegisterSingletonType<NotificationManager>("QtApplicationManager.SystemUI", 2, 0, "NotificationManager",
                                                 &NotificationManager::instanceForQml);
    return s_instance = new NotificationManager();
}

NotificationManager *NotificationManager::instance()
{
    if (!s_instance)
        qFatal("NotificationManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *NotificationManager::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
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
    d->roleNames.insert(Id, "id");
    d->roleNames.insert(ApplicationId, "applicationId");
    d->roleNames.insert(Priority, "priority");
    d->roleNames.insert(Summary, "summary");
    d->roleNames.insert(Body, "body");
    d->roleNames.insert(Category, "category");
    d->roleNames.insert(Icon, "icon");
    d->roleNames.insert(Image, "image");
    d->roleNames.insert(ShowActionsAsIcons, "showActionsAsIcons");
    d->roleNames.insert(Actions, "actions");
    d->roleNames.insert(DismissOnAction, "dismissOnAction");
    d->roleNames.insert(IsClickable, "isClickable");
    d->roleNames.insert(IsSystemNotification, "isSytemNotification");
    d->roleNames.insert(IsShowingProgress, "isShowingProgress");
    d->roleNames.insert(Progress, "progress");
    d->roleNames.insert(IsSticky, "isSticky");
    d->roleNames.insert(Timeout, "timeout");
    d->roleNames.insert(Extended, "extended");
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
    return d->notifications.count();
}

QVariant NotificationManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    const NotificationData *n = d->notifications.at(index.row());

    switch (role) {
    case Id:
        return n->id;
    case ApplicationId:
        return n->application ? n->application->id() : QString();
    case Priority:
        return n->priority;
    case Summary:
        return n->summary;
    case Body:
        return n->body;
    case Category:
        return n->category;
    case Icon:
         if (!n->iconUrl.isEmpty())
             return n->iconUrl;
         return n->application && n->application->package() ? n->application->package()->icon()
                                                            : QString();
    case Image:
        return n->imageUrl;
    case ShowActionsAsIcons:
        return n->showActionIcons;
    case Actions: {
        QVariantList actions = n->actions;
        actions.removeAll(QVariantMap { { qSL("default"), QString() } });
        return actions;
    }
    case DismissOnAction:
        return n->dismissOnAction;
    case IsClickable:
        return n->actions.contains(QVariantMap { { qSL("default"), QString() } });
    case IsSystemNotification:
        return n->isSystemNotification;
    case IsShowingProgress:
        return n->isShowingProgress;
    case Progress:
        return n->isShowingProgress ? n->progress : -1;
    case IsSticky:
        return n->timeout == 0;
    case Timeout:
        return n->timeout;
    case Extended:
        return n->extended;
    }
    return QVariant();
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
        map.insert(qL1S(it.value()), data(QAbstractListModel::index(index), it.key()));
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
    triggerNotificationAction(id, qSL("default"));
}

/*!
    \qmlmethod NotificationManager::triggerNotificationAction(int id, string actionId)

    This function needs to be called by the System UI when the user triggered a notification action.

    The notification is identified by \a id and the action by \a actionId.

    \note You should only use action-ids that have been set for the the given notification (see the
    \l {Notification::}{actions} role). However, the application manager will accept and forward any
    arbitrary string. Be aware that this string is broadcast on the session D-Bus when running in
    multi-process mode.
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
                                      << (actionId.length() > 20 ? (actionId.left(20) + qSL("...")) : actionId);
        }
        emit ActionInvoked(id, actionId);
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
    D-Bus API: identify ourselves
*/
QString NotificationManager::GetServerInformation(QString &vendor, QString &version, QString &spec_version)
{
    //qCDebug(LogNotifications) << "GetServerInformation";
    vendor = qApp->organizationName();
    version = qSL("1.0");
    spec_version = qSL("1.2");
    return qApp->applicationName();
}

/*! \internal
    D-Bus API: announce supported features
*/
QStringList NotificationManager::GetCapabilities()
{
    //qCDebug(LogNotifications) << "GetCapabilities";
    return QStringList() << qSL("action-icons")
                         << qSL("actions")
                         << qSL("body")
                         << qSL("body-hyperlinks")
                         << qSL("body-images")
                         << qSL("body-markup")
                         << qSL("icon-static")
                         << qSL("persistence");
}

/*! \internal
    D-Bus API: someone wants to create or update a notification
*/
uint NotificationManager::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions,
                                 const QVariantMap &hints, int timeout)
{
    static uint idCounter = 0;

    qCDebug(LogNotifications) << "Notify" << app_name << replaces_id << app_icon << summary << body << actions << hints << timeout;

    if (replaces_id == 0) { // new notification
        uint id = ++idCounter;
        // we need to delay the model update until the client has a valid id
        QMetaObject::invokeMethod(this, [this, app_name, id, app_icon, summary, body, actions, hints, timeout]() {
            notifyHelper(app_name, id, false, app_icon, summary, body, actions, hints, timeout);
        }, Qt::QueuedConnection);
        return id;
    } else {
        return notifyHelper(app_name, replaces_id, true, app_icon, summary, body, actions, hints, timeout);
    }
}

uint NotificationManager::notifyHelper(const QString &app_name, uint id, bool replaces, const QString &app_icon,
                                       const QString &summary, const QString &body, const QStringList &actions,
                                       const QVariantMap &hints, int timeout)
{
    Q_ASSERT(id);
    NotificationData *n = nullptr;

    if (replaces) {
       int i = d->findNotificationById(id);

       if (i < 0) {
           qCDebug(LogNotifications) << "  -> failed to update existing notification";
           return 0;
       }
       n = d->notifications.at(i);
       qCDebug(LogNotifications) << "  -> updating existing notification";
    } else {
        n = new NotificationData;
        n->id = id;

        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        qCDebug(LogNotifications) << "  -> adding new notification with id" << id;
    }

    Application *app = ApplicationManager::instance()->fromId(app_name);

    if (replaces && app != n->application) {
        // no hijacking allowed
        qCDebug(LogNotifications) << "  -> failed to update notification, due to hijacking attempt";
        return 0;
    }
    n->application = app;
    n->priority = hints.value(qSL("urgency"), QVariant(0)).toUInt();
    n->summary = summary;
    n->body = body;
    n->category = hints.value(qSL("category")).toString();
    n->iconUrl = app_icon;

    if (hints.contains(qSL("image-data"))) {
        //TODO: how can we parse this - the dbus sig of value is "(iiibiiay)"
    } else if (hints.contains(qSL("image-path"))) {
        n->imageUrl = hints.value(qSL("image-path")).toString();
    }

    n->showActionIcons = hints.value(qSL("action-icons")).toBool();
    n->actions.clear();
    for (int ai = 0; ai != (actions.size() & ~1); ai += 2)
        n->actions.append(QVariantMap { { actions.at(ai), actions.at(ai + 1) } });
    n->dismissOnAction = !hints.value(qSL("resident")).toBool();

    n->isSystemNotification = hints.value(qSL("x-pelagicore-system-notification")).toBool();
    n->isShowingProgress = hints.value(qSL("x-pelagicore-show-progress")).toBool();
    n->progress = hints.value(qSL("x-pelagicore-progress")).toReal();
    n->timeout = qMax(0, timeout);
    n->extended = convertFromDBusVariant(hints.value(qSL("x-pelagicore-extended"))).toMap();

    if (replaces) {
        QModelIndex idx = index(d->notifications.indexOf(n), 0);
        emit dataChanged(idx, idx);
        static const auto nChanged = QMetaMethod::fromSignal(&NotificationManager::notificationChanged);
        if (isSignalConnected(nChanged)) {
            // we could do better here and actually find out which fields changed...
            emit notificationChanged(n->id, QStringList());
        }
    } else {
        d->notifications << n;
        endInsertRows();
        emit notificationAdded(n->id);
    }

    if (timeout > 0) {
        delete n->timer;
        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, [this,id,t]() { d->closeNotification(id, TimeoutExpired); t->deleteLater(); });
        t->start(timeout);
        n->timer = t;
    }

    qCDebug(LogNotifications) << "  -> returning id" << id;
    return id;
}

/*! \internal
    D-Bus API: some requests that a notification should be closed
*/
void NotificationManager::CloseNotification(uint id)
{
    d->closeNotification(id, CloseNotificationCalled);
}

void NotificationManagerPrivate::closeNotification(uint id, CloseReason reason)
{
    qCDebug(LogNotifications) << "Close" << id;

    int i = findNotificationById(id);

    if (i >= 0) {
        emit q->notificationAboutToBeRemoved(id);

        q->beginRemoveRows(QModelIndex(), i, i);
        auto n = notifications.takeAt(i);
        q->endRemoveRows();

        emit q->NotificationClosed(id, uint(reason));

        qCDebug(LogNotifications) << "Deleting notification with id:" << id;
        delete n;
    }
}

QT_END_NAMESPACE_AM

#include "moc_notificationmanager.cpp"
