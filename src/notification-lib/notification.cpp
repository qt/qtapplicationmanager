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

#include "notification.h"

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
        actionList << "default" << QString();
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
    hints.insert("urgency", int(priority()));
    if (!category().isEmpty())
        hints.insert("category", category());
    if (!image().isEmpty())
        hints.insert("image-path", image().toString());
    if (isShowingProgress()) {
        hints.insert("x-pelagicore-show-progress", true);
        hints.insert("x-pelagicore-progress", progress());
    }

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
    if (actionId == "default")
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

