/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QVariant>
#include <QCoreApplication>
#include <QTimer>

#include "global.h"

#include "application.h"
#include "applicationmanager.h"
#include "notificationmanager.h"
#include "qml-utilities.h"

/*!
    \class NotificationManager
    \internal
*/

/*!
    \qmltype NotificationManager
    \inqmlmodule QtApplicationManager 1.0
    \brief The NotificationManager singleton

    This singleton class is the window managing part of the application manager. It provides a QML
    API only.

    To make QML programmers lifes easier, the class is derived from \c QAbstractListModel,
    so you can directly use this singleton as a model in your notification views.

    Each item in this model corresponds to an active notification.

    \target NotificationManager Roles

    The following roles are available in this model - please also take a look at the freedesktop.org specification for an in-depth explanation of these fields and how clients should populate them:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c id
        \li \c int
        \li The unique id of this notification.
    \row
        \li \c applicationId
        \li \c string
        \li The id of the application that created this notification. This can be used to look up
            information about the application in the ApplicationManager model.
            \note The \c applicationId role is neither unique within this model, nor is it
                  guaranteed to be valid at all. On the one hand, a single application can have
                  multiple active notifications and on the other hand, system-notifications have no
                  application context at all.
    \row
        \li \c priority
        \li \c int
        \li See the client side documentation of Notification::priority
    \row
        \li \c summary
        \li \c string
        \li See the client side documentation of Notification::summary
    \row
        \li \c body
        \li \c string
        \li See the client side documentation of Notification::body
    \row
        \li \c category
        \li \c string
        \li See the client side documentation of Notification::category
    \row
        \li \c icon
        \li \c url
        \li See the client side documentation of Notification::icon
    \row
        \li \c image
        \li \c url
        \li See the client side documentation of Notification::image
    \row
        \li \c actions
        \li \c object
        \li See the client side documentation of Notification::actions
    \row
        \li \c showActionsAsIcons
        \li \c bool
        \li See the client side documentation of Notification::showActionsAsIcons
    \row
        \li \c dismissOnAction
        \li \c bool
        \li See the client side documentation of Notification::dismissOnAction
    \row
        \li \c isClickable
        \li \c bool
        \li See the client side documentation of Notification::clickable
    \row
        \li \c isSytemNotification
        \li \c url
        \li Set to \c true for notifications not originating from an application, but some system
            service. Always set to \c false for notifications coming from UI applications.
    \row
        \li \c isShowingProgress
        \li \c bool
        \li See the client side documentation of Notification::showProgress
    \row
        \li \c progress
        \li \c qreal
        \li See the client side documentation of Notification::progress
    \row
        \li \c isSticky
        \li \c bool
        \li See the client side documentation of Notification::sticky
    \row
        \li \c timeout
        \li \c int
        \li See the client side documentation of Notification::timeout
    \row
        \li \c extended
        \li \c object
        \li See the client side documentation of Notification::extended.

    \endtable

    The QML import for this singleton is

    \c{import QtApplicationManager 1.0}

    The actual backend implementation that is receiving the notifications from other process is
    fully compliant to the D-Bus interface of the freedesktop.org notification specification
    (https://developer.gnome.org/notification-spec/).
    For testing purposes, the notify-send tool from the \c libnotify package can be used to create
    notifications.
*/


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

    IsSticky, // no timeout == sticky
    Timeout, // in msec

    Extended // QVariantMap
};

struct NotificationData
{
    uint id;
    const Application *application;
    uint priority;
    QString summary;
    QString body;
    QString category;
    QString iconUrl;
    QString imageUrl;
    bool showActionIcons;
    QVariantMap actions; // id (as string) --> text (as string)
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

}

class NotificationManagerPrivate
{
public:
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

NotificationManager *NotificationManager::s_instance = 0;


NotificationManager *NotificationManager::createInstance()
{
    if (s_instance)
        qFatal("NotificationManager::createInstance() was called a second time.");

    qmlRegisterSingletonType<NotificationManager>("QtApplicationManager", 1, 0, "NotificationManager",
                                                 &NotificationManager::instanceForQml);
    return s_instance = new NotificationManager();
}

NotificationManager *NotificationManager::instance()
{
    if (!s_instance)
        qFatal("NotificationManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *NotificationManager::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
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
         if (n->application && !n->application->icon().isEmpty())
             return n->application->icon();
         return n->iconUrl;
    case Image:
        return n->imageUrl;
    case ShowActionsAsIcons:
        return n->showActionIcons;
    case Actions: {
        QVariantMap actions = n->actions;
        actions.remove(qSL("default"));
        return actions;
    }
    case DismissOnAction:
        return n->dismissOnAction;
    case IsClickable:
        return n->actions.contains(qSL("default"));
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

    This property holds the number of active notifications in the model.
*/
int NotificationManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object NotificationManager::get(int index) const

    Retrieves the model data at \a index as a JavaScript object. Please see the \l {NotificationManager
    Roles}{role names} for the expected object fields.

    Will return an empty object, if the specified \a index is invalid.
*/
QVariantMap NotificationManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogNotifications) << "invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(it.value(), data(QAbstractListModel::index(index), it.key()));
    return map;
}

/*!
    \qmlmethod NotificationManager::acknowledgeNotification(int id)

    This function needs to be called by the system-ui, when the user acknowledged the notification
    identified by \a id (most likely by clicking on it).
*/
void NotificationManager::acknowledgeNotification(int id)
{
    triggerNotificationAction(id, qSL("default"));
}

/*!
    \qmlmethod NotificationManager::triggerNotificationAction(int id, string actionId)

    This function needs to be called by the system-ui, when the user triggered a notification action.

    The notification is identified by \a id and the action by \a actionId.
    \note You should only use action-ids that have been set for the the given notification (see the
          \c actions role), but the application-manager will even accept and forward an arbitray string.
          Be aware that this string is broadcast on the session D-Bus when running in multi-process mode.
*/
void NotificationManager::triggerNotificationAction(int id, const QString &actionId)
{
    int i = d->findNotificationById(id);

    if (i >= 0) {
        NotificationData *n = d->notifications.at(i);
        if (!n->actions.contains(actionId)) {
            qCDebug(LogNotifications) << "Requested to trigger a notification action, but the action is not registered:"
                                      << (actionId.length() > 20 ? (actionId.left(20) + qSL("...")) : actionId);
        }
        emit ActionInvoked(id, actionId);
    }
}

/*!
    \qmlmethod NotificationManager::dismissNotification(int id)

    This function needs to be called by the system-ui, when the notification identified by \a id is
    not needed anymore.

    The creator of the notification will be notified about this dismissal.
*/
void NotificationManager::dismissNotification(int id)
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

    NotificationData *n = 0;
    uint id;

    if (replaces_id) {
       int i = d->findNotificationById(replaces_id);

       if (i < 0)
           return 0;

       id = replaces_id;
       n = d->notifications.at(i);
    } else {
        id = ++idCounter;

        n = new NotificationData;
        n->id = id;

        beginInsertRows(QModelIndex(), rowCount(), rowCount());
    }

    const Application *app = ApplicationManager::instance()->fromId(app_name);

    if (replaces_id && app != n->application) {
        // no hijacking allowed
        return 0;
    }
    n->application = app;
    n->priority = hints.value(qSL("urgency"), QVariant(0)).toInt();
    n->summary = summary;
    n->body = body;
    n->category = hints.value(qSL("category")).toString();
    n->iconUrl = app_icon;

    if (hints.contains(qSL("image-data"))) {
        //TODO: how can we parse this - the dbus sig of value is "(iiibiiay)"
    } else if (hints.contains(qSL("image-path"))) {
        n->imageUrl = hints.value(qSL("image-data")).toString();
    }

    n->showActionIcons = hints.value(qSL("action-icons")).toBool();
    for (int ai = 0; ai != (actions.size() & ~1); ai += 2)
        n->actions.insert(actions.at(ai), actions.at(ai + 1));
    n->dismissOnAction = !hints.value(qSL("resident")).toBool();

    n->isSystemNotification = hints.value(qSL("x-pelagicore-system-notification")).toBool();
    n->isShowingProgress = hints.value(qSL("x-pelagicore-show-progress")).toBool();
    n->progress = hints.value(qSL("x-pelagicore-progress")).toReal();
    n->timeout = timeout;
    n->extended = hints.value(qSL("x-pelagicore-extended")).toMap();

    if (replaces_id) {
        QModelIndex idx = index(d->notifications.indexOf(n), 0);
        emit dataChanged(idx, idx);
    } else {
        d->notifications << n;
        endInsertRows();
    }

    if (timeout >= 0) {
        delete n->timer;
        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, [this,id,t]() { d->closeNotification(id, TimeoutExpired); t->deleteLater(); });
        t->start(timeout);
        n->timer = t;
    }

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
        q->beginRemoveRows(QModelIndex(), i, i);
        notifications.removeAt(i);
        q->endRemoveRows();

        emit q->NotificationClosed(id, int(reason));
    }
}
