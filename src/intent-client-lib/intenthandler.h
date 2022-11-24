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

#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QUuid>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentClientRequest;

class IntentHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.Application/IntentHandler 2.0")

    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QStringList intentIds READ intentIds WRITE setIntentIds NOTIFY intentIdsChanged)

public:
    IntentHandler(QObject *parent = nullptr);
    ~IntentHandler() override;

    QStringList intentIds() const;
    void setIntentIds(const QStringList &intentId);

signals:
    void intentIdsChanged(const QStringList &intentId);

    void requestReceived(QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *request);

protected:
    void componentComplete() override;
    void classBegin() override;

    bool isComponentCompleted() const;

private:
    Q_DISABLE_COPY(IntentHandler)

    QStringList m_intentIds;
    bool m_completed = false;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(IntentHandler) *)
