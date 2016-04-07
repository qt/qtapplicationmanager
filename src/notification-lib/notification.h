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

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QQmlParserStatus>


class Notification : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(Priority)

    Q_PROPERTY(uint notificationId READ notificationId NOTIFY notificationIdChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(QString summary READ summary WRITE setSummary NOTIFY summaryChanged)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY bodyChanged)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QUrl image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority NOTIFY priorityChanged)
    Q_PROPERTY(bool clickable READ isClickable WRITE setClickable NOTIFY clickableChanged)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout NOTIFY timeoutChanged)
    Q_PROPERTY(bool sticky READ isSticky WRITE setSticky NOTIFY stickyChanged)
    Q_PROPERTY(bool showProgress READ isShowingProgress WRITE setShowProgress NOTIFY showProgressChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList actions READ actions WRITE setActions NOTIFY actionChanged)
    Q_PROPERTY(bool dismissOnAction READ dismissOnAction WRITE setDismissOnAction NOTIFY dismissOnActionChanged)

public:
    enum Priority { Low, Normal, Critical };

    enum ConstructionMode { Declarative, Dynamic };

    Notification(QObject *parent = 0, ConstructionMode mode = Declarative);

    uint notificationId() const;
    QString summary() const;
    QString body() const;
    QUrl icon() const;
    QUrl image() const;
    QString category() const;
    Priority priority() const;
    bool isClickable() const;
    int timeout() const;
    bool isSticky() const;
    bool isShowingProgress() const;
    qreal progress() const;
    QVariantList actions() const;
    bool isVisible() const;
    bool dismissOnAction() const;

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

public slots:
    void setSummary(const QString &summary);
    void setBody(const QString &boy);
    void setIcon(const QUrl &icon);
    void setImage(const QUrl &image);
    void setCategory(const QString &category);
    void setPriority(Priority priority);
    void setClickable(bool clickable);
    void setTimeout(int timeout);
    void setSticky(bool sticky);
    void setShowProgress(bool showProgress);
    void setProgress(qreal progress);
    void setActions(const QVariantList &actions);
    void setVisible(bool visible);
    void setDismissOnAction(bool dismissOnAction);

signals:
    void summaryChanged(const QString &summary);
    void bodyChanged(const QString &body);
    void iconChanged(const QUrl &icon);
    void imageChanged(const QUrl &image);
    void categoryChanged(const QString &category);
    void priorityChanged(Priority priority);
    void clickableChanged(bool clickable);
    void timeoutChanged(int timeout);
    void stickyChanged(bool sticky);
    void showProgressChanged(bool showProgress);
    void progressChanged(qreal progress);
    void actionChanged(const QVariantList &actions);
    void dismissOnActionChanged(bool dismissOnAction);
    void visibleChanged(bool visible);
    void notificationIdChanged(int notificationId);

    void clicked();
    void actionActivated(const QString &actionId);

public:
    // not public API, but QQmlParserStatus pure-virtual overrides
    void classBegin() override;
    void componentComplete() override;

    void setId(uint notificationId);

protected:
    virtual uint libnotifyShow() = 0;
    virtual void libnotifyClose() = 0;
    void libnotifyActivated(const QString &actionId);
    void libnotifyClosed(uint reason);

    QVariantMap libnotifyHints() const;
    QStringList libnotifyActionList() const;

private slots:
    bool updateNotification();

private:
    uint m_id = 0;
    bool m_visible = false;

    QString m_summary;
    QString m_body;
    QUrl m_icon;
    QUrl m_image;
    QString m_category;
    Priority m_priority;
    bool m_clickable = false;
    int m_timeout = defaultTimeout;
    bool m_showProgress = false;
    qreal m_progress = -1;
    QVariantList m_actions;

    static const int defaultTimeout = 2000;
    bool m_dismissOnAction;

private:
    Q_DISABLE_COPY(Notification)
};
