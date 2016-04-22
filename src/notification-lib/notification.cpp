/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "notification.h"
#include "global.h"

/*!
    \qmltype Notification
    \inqmlmodule QtApplicationManager 1.0
    \brief An abstraction layer to enable QML applications to issue notifications to the system-ui.

    This item is available for QML applications by either creating a Notification item
    statically or by dynamically calling ApplicationInterface::createNotification.
    For all other applications and/or services, the notification service of the application-manager
    is available via a freedesktop.org compliant \l{https://developer.gnome.org/notification-spec/}
    {org.freedesktop.Notifications} D-Bus interface.

    \note Most of the property documentation text is copied straight from the
          \l{https://developer.gnome.org/notification-spec/} {org.freedesktop.Notifications specification}
          because it is not possible to directly link to the documentation of a specific property.

    The server/system-ui side of the notification infrastructure is implemented by NotificationManager.
*/


/*!
    \qmlsignal Notification::acknowledged()

    This signal is emitted when this notification was acknowledged on the server side - most likely
    due to the user clicking on the notification.
*/

/*!
    \qmlsignal Notification::actionTriggered(string actionId)

    This signal is emitted when action identified by \a actionId of this notification was triggered
    on the server side.
    \note For maximum flexibility, the \a actionId can be an arbitrary string: you have to check
          yourself if it represents one of the registered \l actions.
*/

Notification::Notification(QObject *parent, Notification::ConstructionMode mode)
    : QObject(parent)
{
    if (mode == Dynamic)
        componentComplete();
}

/*!
    \qmlproperty int Notification::notificationId

    The system-wide unique \c id of this notification.

    This is \c 0 until this notification is made \l visible (and thus published to the server). The
    id is read-only, since it will be allocated by the server.
*/
uint Notification::notificationId() const
{
    return m_id;
}

/*!
    \qmlproperty string Notification::summary

    This is a single line overview of the notification.

    For instance, "You have mail" or "A friend has come online". It should generally not be longer
    than 40 characters, though this is not a requirement, and server implementations should word
    wrap if necessary.
*/
QString Notification::summary() const
{
    return m_summary;
}

/*!
    \qmlproperty string Notification::body

    This is a multi-line body of text. Each line is a paragraph, server implementations are free to
    word wrap them as they see fit.

    The body may contain simple HTML markup. If the body is omitted, just the \l summary is displayed.
*/
QString Notification::body() const
{
    return m_body;
}

/*!
    \qmlproperty url Notification::icon

    Notifications can optionally have an associated icon.

    This icon should identify the application which created this notification.
*/
QUrl Notification::icon() const
{
    return m_icon;
}

/*!
    \qmlproperty url Notification::image

    A notification can optionally have an associated image.

    This image should not be used to identify the applicaton (see \l icon for this), but rather if
    the nature of the notification is image based (e.g. showing an album cover in a "now playing"
    notification).
*/
QUrl Notification::image() const
{
    return m_image;
}

/*!
    \qmlproperty string Notification::category

    The type of notification this is.

    Notifications can optionally have a category indicator. Although neither client or server must
    support this, some may choose to. Those servers implementing categories may use them to
    intelligently display the notification in a certain way, or group notifications of similar
    types.
*/
QString Notification::category() const
{
    return m_category;
}

/*!
    \qmlproperty int Notification::priority

    The priority of this notification. The actual value is implementation specific, but ideally any
    implementation should use the defined values from the \c freedesktop.org specification,
    available via the Priority enum:
    \table
    \header
        \li Name
        \li Value
    \row
        \li Low
        \li \c 0
    \row
        \li Normal
        \li \c 1
    \row
        \li Critical
        \li \c 2
    \endtable
*/
int Notification::priority() const
{
    return m_priority;
}

/*!
    \qmlproperty bool Notification::acknowledgeable

    Request that the notification can be acknowledged by the user - most likely by clicking on it.

    This will be reported via the \l acknowledged signal.
*/
bool Notification::isAcknowledgeable() const
{
    return m_acknowledgeable;
}

/*!
    \qmlproperty int Notification::timeout

    In case of non-sticky notifications, this value specifies after how many milliseconds the
    notification should be removed from the screen.

    \sa sticky
*/
int Notification::timeout() const
{
    return m_timeout;
}

/*!
    \qmlproperty bool Notification::sticky

    If this property is set to \c false, then the notification should be removed after \l timeout
    milliseconds. Otherwise the notification is sticky and should stay visible until the user
    acknowledges it.
*/
bool Notification::isSticky() const
{
    return m_timeout == 0;
}

/*!
    \qmlproperty bool Notification::showProgress

    A boolean value describing whether a progress-bar/busy-indicator should be shown as part of the
    notification.

    \note This is an application-manager specific extension to the protocol: it uses the
          \c{x-pelagicore-show-progress} hint to communicate this value.
*/
bool Notification::isShowingProgress() const
{
    return m_showProgress;
}

/*!
    \qmlproperty qreal Notification::progress

    A floating-point value between \c{[0.0 ... 1.0]} which can be used to show a progress-bar on
    the notification. The special value \c -1 can be used to request a busy indicator.

    \note This is an application-manager specific extension to the protocol: it uses the
          \c{x-pelagicore-progress} hint to communicate this value.
*/
qreal Notification::progress() const
{
    return m_progress;
}

/*!
    \qmlproperty list<object> Notification::actions

    This is an object describing the possible actions that the user can choose from. Every key in
    this map is an \c actionId and its corresponding value is an \c actionText. The
    notification-manager should eiher display the \c actionText or an icon, depending on the
    showActionsAsIcons property.
*/
QVariantList Notification::actions() const
{
    return m_actions;
}

/*!
    \qmlproperty bool Notification::showActionsAsIcons

    A hint supplied by the client on how to present the \c actions. If this property is \c false,
    the notification actions should be shown in text form. Otherwise the \c actionText should be
    taken as an icon name conforming to the \c freedesktop.org icon naming specification (in a
    closed system, these could also be any icon specification string that the notification server
    understands).
*/
bool Notification::showActionsAsIcons() const
{
    return m_showActionsAsIcons;
}

/*!
    \qmlproperty bool Notification::visible
*/
bool Notification::isVisible() const
{
    return m_visible;
}

/*!
    \qmlproperty bool Notification::dismissOnAction

    Tells the notification manager, whether clicking one of the supplied action texts or images
    will dismiss the notification.
*/
bool Notification::dismissOnAction() const
{
    return m_dismissOnAction;
}

/*!
    \qmlproperty object Notification::extended

    A custom variant property that lets the user attach an arbitrary meta-data to this notification.

    \note This is an application-manager specific extension to the protocol: it uses the
          \c{x-pelagicore-extended} hint to communicate this value.
*/
QVariantMap Notification::extended() const
{
    return m_extended;
}

/*!
    \qmlmethod Notification::show()

    An alias for \c{visible = true}

    \sa visible
*/
void Notification::show()
{
    setVisible(true);
}

/*!
    \qmlmethod Notification::hide()

    An alias for \c{visible = false}

    \sa visible
*/
void Notification::hide()
{
    setVisible(false);
}

void Notification::setSummary(const QString &summary)
{
    if (m_summary != summary) {
        m_summary = summary;
        emit summaryChanged(summary);
    }
}

void Notification::setBody(const QString &body)
{
    if (m_body != body) {
        m_body = body;
        emit bodyChanged(body);
    }
}

void Notification::setIcon(const QUrl &icon)
{
    if (m_icon != icon) {
        m_icon = icon;
        emit iconChanged(icon);
    }
}

void Notification::setImage(const QUrl &image)
{
    if (m_image != image) {
        m_image = image;
        emit imageChanged(image);
    }
}

void Notification::setCategory(const QString &category)
{
    if (m_category != category) {
        m_category = category;
        emit categoryChanged(category);
    }
}

void Notification::setPriority(int priority)
{
    if (m_priority != priority) {
        m_priority = priority;
        emit priorityChanged(priority);
    }
}

void Notification::setAcknowledgeable(bool acknowledgeable)
{
    if (m_acknowledgeable != acknowledgeable) {
        m_acknowledgeable = acknowledgeable;
        emit acknowledgeableChanged(acknowledgeable);
    }
}

void Notification::setTimeout(int timeout)
{
    if (m_timeout != timeout) {
        bool isOrWasSticky = (!m_timeout || !timeout);

        m_timeout = timeout;
        emit timeoutChanged(timeout);
        if (isOrWasSticky)
            emit stickyChanged(isSticky());
    }
}

void Notification::setSticky(bool sticky)
{
    if ((m_timeout == 0) != sticky) {
        m_timeout = sticky? 0 : defaultTimeout;
        emit stickyChanged(sticky);
        emit timeoutChanged(m_timeout);
    }
}

void Notification::setShowProgress(bool showProgress)
{
    if (m_showProgress != showProgress) {
        m_showProgress = showProgress;
        emit showProgressChanged(showProgress);
    }
}

void Notification::setProgress(qreal progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged(progress);
    }
}

void Notification::setActions(const QVariantList &actions)
{
    if (m_actions != actions) {
        m_actions = actions;
        emit actionsChanged(actions);
    }
}

void Notification::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged(visible);
    }
}

void Notification::setDismissOnAction(bool dismissOnAction)
{
    if (m_dismissOnAction != dismissOnAction) {
        m_dismissOnAction = dismissOnAction;
        emit dismissOnActionChanged(dismissOnAction);
    }
}

void Notification::setShowActionsAsIcons(bool showActionsAsIcons)
{
    if (m_showActionsAsIcons != showActionsAsIcons) {
        m_showActionsAsIcons = showActionsAsIcons;
        emit showActionsAsIconsChanged(showActionsAsIcons);
    }
}

void Notification::setExtended(QVariantMap extended)
{
    if (m_extended != extended) {
        m_extended = extended;
        emit extendedChanged(extended);
    }
}

void Notification::classBegin()
{ }

void Notification::componentComplete()
{
    connect(this, &Notification::summaryChanged, this, &Notification::updateNotification);
    connect(this, &Notification::bodyChanged, this, &Notification::updateNotification);
    connect(this, &Notification::iconChanged, this, &Notification::updateNotification);
    connect(this, &Notification::imageChanged, this, &Notification::updateNotification);
    connect(this, &Notification::categoryChanged, this, &Notification::updateNotification);
    connect(this, &Notification::priorityChanged, this, &Notification::updateNotification);
    connect(this, &Notification::acknowledgeableChanged, this, &Notification::updateNotification);
    connect(this, &Notification::timeoutChanged, this, &Notification::updateNotification);
    connect(this, &Notification::stickyChanged, this, &Notification::updateNotification);
    connect(this, &Notification::showProgressChanged, this, &Notification::updateNotification);
    connect(this, &Notification::progressChanged, this, &Notification::updateNotification);
    connect(this, &Notification::actionsChanged, this, &Notification::updateNotification);
    connect(this, &Notification::showActionsAsIconsChanged, this, &Notification::updateNotification);
    connect(this, &Notification::dismissOnActionChanged, this, &Notification::updateNotification);
    connect(this, &Notification::extendedChanged, this, &Notification::updateNotification);
    connect(this, &Notification::visibleChanged, this, &Notification::updateNotification);
}

QStringList Notification::libnotifyActionList() const
{
    QStringList actionList;
    if (isAcknowledgeable())
        actionList << qSL("default") << QString();
    foreach (const QVariant &action, actions()) {
        if (action.type() == QVariant::String) {
            actionList << action.toString() << QString();
        } else {
            QVariantMap map = action.toMap();
            if (map.size() != 1) {
                qWarning("WARNING: an entry in a Notification.actions list needs to be eihter a string or a variant-map with a single entry");
                continue; // skip
            }
            actionList << map.firstKey() << map.first().toString();
        }
    }
    return actionList;
}

QVariantMap Notification::libnotifyHints() const
{
    QVariantMap hints;
    hints.insert(qSL("urgency"), int(priority()));
    if (!category().isEmpty())
        hints.insert(qSL("category"), category());
    if (!image().isEmpty())
        hints.insert(qSL("image-path"), image().toString());
    if (isShowingProgress()) {
        hints.insert(qSL("x-pelagicore-show-progress"), true);
        hints.insert(qSL("x-pelagicore-progress"), progress());
    }
    if (!extended().isEmpty())
        hints.insert(qSL("x-pelagicore-extended"), extended());

    return hints;
}

bool Notification::updateNotification()
{
    if (isVisible()) {
        // show first time or update existing

        uint newId = libnotifyShow();
        if (!newId)
            return false;

        if (notificationId() != newId)
            setId(newId);
    } else if (notificationId() && !isVisible()) {
        // close existing
        libnotifyClose();
    }
    return true;
}

void Notification::setId(uint notificationId)
{
    if (m_id != notificationId) {
        m_id = notificationId;
        emit notificationIdChanged(notificationId);
    }
}

void Notification::libnotifyActionInvoked(const QString &actionId)
{
    if (actionId == qL1S("default"))
        emit acknowledged();
    else
        emit actionTriggered(actionId);
}

void Notification::libnotifyNotificationClosed(uint reason)
{
    Q_UNUSED(reason)

    hide();
    setId(0);
}

