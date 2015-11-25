/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
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
    \inqmlmodule com.pelagicore.ApplicationManager 0.1
    \brief The NotificationManager singleton

    This singleton class is the window managing part of the application manager. It provides a QML
    API only.

    To make QML programmers lifes easier, the class is derived from \c QAbstractListModel,
    so you can directly use this singleton as a model in your notification views.

    Each item in this model corresponds to an active notification. Please not that a single
    application can have multiple notifications and system-notifications have no application
    context at all: the \c applicationId role is neither unique within this model, nor is
    it guaranteed to be valid at all.

    \keyword NotificationManager Roles

    The following roles are available in this model - please also take a look at the freedesktop.org specification for an in-depth explanation of these fields and how clients should populate them:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c id
        \li \c string
        \li The unique id of this notification
    \row
        \li \c applicationId
        \li \c string
        \li The unique id of an application represented as a string in reverse-dns form (e.g.
            \c com.pelagicore.foo). This can be used to look up information about the application
            in the ApplicationManager model.
    \row
        \li \c priority
        \li \c int
        \li The priority of this notification. The actual value is implementation dependent, but
            ideally any implementation should use the defined values from the freedesktop.org specification:
            (\c 0 - Low, \c 1 - Normal, and \c 2 - Critical)
    \row
        \li \c summary
        \li \c string
        \li The summary text.
    \row
        \li \c body
        \li \c string
        \li The body text.
    \row
        \li \c category
        \li \c string
        \li The category - can be empty.
    \row
        \li \c icon
        \li \c url
        \li The URL to an optional icon.
    \row
        \li \c image
        \li \c url
        \li The URL to optional image.
    \row
        \li \c actions
        \li \c object
        \li This is a variant-map describing the possible actions that the user can choose from.
            Every key in this map is an \c actionId and its corresponding value is an \c actionText.
            The notification should eiher display this \c actionText or an icon, depending on the \c
            ShowActionsAsIcons property.
    \row
        \li \c ShowActionsAsIcons
        \li \c bool
        \li A hint supplied by the client on how to present the \c actions. If this property is
            false, the notification actions should be shown in text form. Otherwise the \c
            actionText should be taken as an icon name conforming to the freedesktop.org icon
            naming specification (in a closed system, these could also be any icon specification
            string that the notification server understands).
    \row
        \li \c dismissOnAction
        \li \c bool
        \li Tells the notification manager, whether clicking one of the supplied action texts or
            images will dismiss the notification.
    \row
        \li \c isClickable
        \li \c bool
        \li A boolean value describing whether the notification will react to clicking on it. If this
            property is set to \c true, the \c default action will be triggered in the client.
    \row
        \li \c isSytemNotification
        \li \c url
        \li Set to \c true for notifications not originating from an application, but some system
            service. Always set to \c false for notifications coming from UI applications.
    \row
        \li \c isShowingProgress
        \li \c bool
        \li A boolean value describing whether a progress-bar/busy-indicator should be shown as part
            of the notification.
    \row
        \li \c progress
        \li \c qreal
        \li A floating-point value between \c{[0.0 ... 1.0]} which can be used to show a progress-bar
            on the notification. The special value \c -1 can be used to request a busy indicator.
    \row
        \li \c isSticky
        \li \c bool
        \li If this property is set to \c false, then the notification should be removed after
            an \c timeout milliseconds. Otherwise the notification is sticky and should stay visible
            until the user acknowledges it.
    \row
        \li \c timeout
        \li \c int
        \li In case of non-sticky notifications, this value specifies after how many milliseconds
            the notification should be removed from the screen.
    \endtable

    The QML import for this singleton is

    \c{import com.pelagicore.ApplicationManager 0.1}

    The actual backend implementation that is receiving the notifications from other process is
    fully compliant to the D-Bus interface of the freedesktop.org notification specification
    (https://developer.gnome.org/notification-spec/).
    For testing purposes, the notify-send tool from the libnotify package can be used to create
    notifications.
*/

/*!
    \qmlproperty int NotificationManager::count

    This property holds the number of applications available.
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
    Timeout // in msec
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
         if (n->application && !n->application->displayIcon().isEmpty())
             return n->application->displayIcon();
         return n->iconUrl;
    case Image:
        return n->imageUrl;
    case ShowActionsAsIcons:
        return n->showActionIcons;
    case Actions: {
        QVariantMap actions = n->actions;
        actions.remove("default");
        return actions;
    }
    case DismissOnAction:
        return n->dismissOnAction;
    case IsClickable:
        return n->actions.contains("default");
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
    }
    return QVariant();
}

QHash<int, QByteArray> NotificationManager::roleNames() const
{
    return d->roleNames;
}

/*!
    \qmlmethod object NotificationManager::get(int row) const

    Retrieves the model data at \a row as a JavaScript object. Please see the \l {NotificationManager
    Roles}{role names} for the expected object fields.

    Will return an empty object, if the specified \a row is invalid.
*/
QVariantMap NotificationManager::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogNotifications) << "invalid row:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(it.value(), data(index(row), it.key()));
    return map;
}

void NotificationManager::notificationWasClicked(int id)
{
    notificationActionWasActivated(id, "default");
}

void NotificationManager::notificationActionWasActivated(int id, const QString &actionId)
{
    int i = d->findNotificationById(id);

    if (i >= 0) {
        NotificationData *n = d->notifications.at(i);
        if (n->actions.contains(actionId))
            emit ActionInvoked(id, actionId);
    }
}

void NotificationManager::dismissNotification(int id)
{
    d->closeNotification(id, UserDismissed);
}

QString NotificationManager::GetServerInformation(QString &vendor, QString &version, QString &spec_version)
{
    //qCDebug(LogNotifications) << "GetServerInformation";
    vendor = qApp->organizationName();
    version = QLatin1String("0.1");
    spec_version = QLatin1String("1.2");
    return qApp->applicationName();
}

QStringList NotificationManager::GetCapabilities()
{
    //qCDebug(LogNotifications) << "GetCapabilities";
    return QStringList() << "action-icons"
                         << "actions"
                         << "body"
                         << "body-hyperlinks"
                         << "body-images"
                         << "body-markup"
                         << "icon-static"
                         << "persistence";
}

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
    n->priority = hints.value("urgency", QVariant(0)).toInt();
    n->summary = summary;
    n->body = body;
    n->category = hints.value("category").toString();
    n->iconUrl = app_icon;

    if (hints.contains("image-data")) {
        //TODO: how can we parse this - the dbus sig of value is "(iiibiiay)"
    } else if (hints.contains("image-path")) {
        n->imageUrl = hints.value("image-data").toString();
    }

    n->showActionIcons = hints.value("action-icons").toBool();
    for (int ai = 0; ai != (actions.size() & ~1); ai += 2)
        n->actions.insert(actions.at(ai), actions.at(ai + 1));
    n->dismissOnAction = !hints.value("resident").toBool();

    n->isSystemNotification = hints.value("x-pelagicore-system-notification").toBool();
    n->isShowingProgress = hints.value("x-pelagicore-show-progress").toBool();
    n->progress = hints.value("x-pelagicore-progress").toReal();
    n->timeout = timeout;

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
