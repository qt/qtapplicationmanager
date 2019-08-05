/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QQmlParserStatus>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Notification : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/Notification 2.0")
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(uint notificationId READ notificationId NOTIFY notificationIdChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(QString summary READ summary WRITE setSummary NOTIFY summaryChanged)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY bodyChanged)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QUrl image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(int priority READ priority WRITE setPriority NOTIFY priorityChanged)
    Q_PROPERTY(bool acknowledgeable READ isAcknowledgeable WRITE setAcknowledgeable NOTIFY acknowledgeableChanged)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout NOTIFY timeoutChanged)
    Q_PROPERTY(bool sticky READ isSticky WRITE setSticky NOTIFY stickyChanged)
    Q_PROPERTY(bool showProgress READ isShowingProgress WRITE setShowProgress NOTIFY showProgressChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList actions READ actions WRITE setActions NOTIFY actionsChanged)
    Q_PROPERTY(bool showActionsAsIcons READ showActionsAsIcons WRITE setShowActionsAsIcons NOTIFY showActionsAsIconsChanged)
    Q_PROPERTY(bool dismissOnAction READ dismissOnAction WRITE setDismissOnAction NOTIFY dismissOnActionChanged)
    Q_PROPERTY(QVariantMap extended READ extended WRITE setExtended NOTIFY extendedChanged)

public:
    enum Priority { Low, Normal, Critical };
    Q_ENUM(Priority)

    enum ConstructionMode { Declarative, Dynamic };

    Notification(QObject *parent = nullptr, ConstructionMode mode = Declarative);

    uint notificationId() const;
    QString summary() const;
    QString body() const;
    QUrl icon() const;
    QUrl image() const;
    QString category() const;
    int priority() const;
    bool isAcknowledgeable() const;
    int timeout() const;
    bool isSticky() const;
    bool isShowingProgress() const;
    qreal progress() const;
    QVariantList actions() const;
    bool showActionsAsIcons() const;
    bool dismissOnAction() const;
    QVariantMap extended() const;
    bool isVisible() const;

    Q_INVOKABLE void show();
    Q_INVOKABLE void update();
    Q_INVOKABLE void hide();

public slots:
    void setSummary(const QString &summary);
    void setBody(const QString &boy);
    void setIcon(const QUrl &icon);
    void setImage(const QUrl &image);
    void setCategory(const QString &category);
    void setPriority(int priority);
    void setAcknowledgeable(bool acknowledgeable);
    void setTimeout(int timeout);
    void setSticky(bool sticky);
    void setShowProgress(bool showProgress);
    void setProgress(qreal progress);
    void setActions(const QVariantList &actions);
    void setShowActionsAsIcons(bool showActionsAsIcons);
    void setDismissOnAction(bool dismissOnAction);
    void setExtended(QVariantMap extended);
    void setVisible(bool visible);

signals:
    void notificationIdChanged(uint notificationId);
    void summaryChanged(const QString &summary);
    void bodyChanged(const QString &body);
    void iconChanged(const QUrl &icon);
    void imageChanged(const QUrl &image);
    void categoryChanged(const QString &category);
    void priorityChanged(int priority);
    void acknowledgeableChanged(bool clickable);
    void timeoutChanged(int timeout);
    void stickyChanged(bool sticky);
    void showProgressChanged(bool showProgress);
    void progressChanged(qreal progress);
    void actionsChanged(const QVariantList &actions);
    void showActionsAsIconsChanged(bool showActionsAsIcons);
    void dismissOnActionChanged(bool dismissOnAction);
    void extendedChanged(QVariantMap extended);
    void visibleChanged(bool visible);

    void acknowledged();
    void actionTriggered(const QString &actionId);

public:
    // not public API, but QQmlParserStatus pure-virtual overrides
    void classBegin() override;
    void componentComplete() override;

    void setId(uint notificationId);

protected:
    virtual uint libnotifyShow() = 0;
    virtual void libnotifyClose() = 0;
    void libnotifyActionInvoked(const QString &actionId);
    void libnotifyNotificationClosed(uint reason);

    QVariantMap libnotifyHints() const;
    QStringList libnotifyActionList() const;

private slots:
    bool updateNotification();

private:
    uint m_id = 0;
    QString m_summary;
    QString m_body;
    QUrl m_icon;
    QUrl m_image;
    QString m_category;
    int m_priority = Normal;
    bool m_acknowledgeable = false;
    int m_timeout = defaultTimeout;
    bool m_showProgress = false;
    qreal m_progress = -1;
    QVariantList m_actions;
    bool m_showActionsAsIcons = false;
    bool m_dismissOnAction = false;
    QVariantMap m_extended;
    bool m_visible = false;

    static const int defaultTimeout = 2000;

private:
    Q_DISABLE_COPY(Notification)
};

QT_END_NAMESPACE_AM
