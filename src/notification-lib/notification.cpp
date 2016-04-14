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
    \inqmlmodule io.qt.ApplicationManager 1.0
    \brief An abstraction layer to enable QML applications to issue notifications.

    This item is available for QML applications by either creating a Notification item
    statically or by dynamically calling ApplicationInterface::createNotification.
    For all other applications, the notification service of the application-manager is
    available via the freedesktop.org compliant \l{https://developer.gnome.org/notification-spec/}
    {org.freedesktop.Notifications} D-Bus interface.
*/


Notification::Notification(QObject *parent, Notification::ConstructionMode mode)
    : QObject(parent)
{
    if (mode == Dynamic)
        componentComplete();
}

uint Notification::notificationId() const
{
    return m_id;
}

QString Notification::summary() const
{
    return m_summary;
}

QString Notification::body() const
{
    return m_body;
}

QUrl Notification::icon() const
{
    return m_icon;
}

QUrl Notification::image() const
{
    return m_image;
}

QString Notification::category() const
{
    return m_category;
}

Notification::Priority Notification::priority() const
{
    return m_priority;
}

bool Notification::isClickable() const
{
    return m_clickable;
}

int Notification::timeout() const
{
    return m_timeout;
}

bool Notification::isSticky() const
{
    return m_timeout == 0;
}

bool Notification::isShowingProgress() const
{
    return m_showProgress;
}

qreal Notification::progress() const
{
    return m_progress;
}

QVariantList Notification::actions() const
{
    return m_actions;
}

bool Notification::isVisible() const
{
    return m_visible;
}

bool Notification::dismissOnAction() const
{
    return m_dismissOnAction;
}

QVariantMap Notification::extended() const
{
    return m_extended;
}

void Notification::show()
{
    setVisible(true);
}

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

void Notification::setPriority(Notification::Priority priority)
{
    if (m_priority != priority) {
        m_priority = priority;
        emit priorityChanged(priority);
    }
}

void Notification::setClickable(bool clickable)
{
    if (m_clickable != clickable) {
        m_clickable = clickable;
        emit clickableChanged(clickable);
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
        emit actionChanged(actions);
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
    connect(this, &Notification::visibleChanged, this, &Notification::updateNotification);
    connect(this, &Notification::summaryChanged, this, &Notification::updateNotification);
    connect(this, &Notification::bodyChanged, this, &Notification::updateNotification);
    connect(this, &Notification::iconChanged, this, &Notification::updateNotification);
    connect(this, &Notification::imageChanged, this, &Notification::updateNotification);
    connect(this, &Notification::categoryChanged, this, &Notification::updateNotification);
    connect(this, &Notification::priorityChanged, this, &Notification::updateNotification);
    connect(this, &Notification::clickableChanged, this, &Notification::updateNotification);
    connect(this, &Notification::timeoutChanged, this, &Notification::updateNotification);
    connect(this, &Notification::stickyChanged, this, &Notification::updateNotification);
    connect(this, &Notification::showProgressChanged, this, &Notification::updateNotification);
    connect(this, &Notification::progressChanged, this, &Notification::updateNotification);
    connect(this, &Notification::extendedChanged, this, &Notification::updateNotification);
}

//void Notification::setCallbacks(uint (*notifyCallback)(uint, const QString &, const QString &, const QString &, const QStringList &, const QVariantMap &, int), void (*closeCallback)(uint))
//{
//    s_notifyCallback = notifyCallback;
//    s_closeCallback = closeCallback;
//}

QStringList Notification::libnotifyActionList() const
{
    QStringList actionList;
    if (isClickable())
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
//        if (s_notifyCallback) {
//            newId = (*s_notifyCallback)(notificationId(), icon().toString(), summary(),
//                                        body(), actionList, hints, timeout());
//        }

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

void Notification::libnotifyActivated(const QString &actionId)
{
    if (actionId == qL1S("default"))
        emit clicked();
    else
        emit actionActivated(actionId);
}

void Notification::libnotifyClosed(uint reason)
{
    Q_UNUSED(reason)

    hide();
    setId(0);
}

