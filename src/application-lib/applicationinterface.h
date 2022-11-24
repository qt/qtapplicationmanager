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
#include <QUrl>
#include <QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager.ApplicationInterface")
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true)
    Q_PROPERTY(QVariantMap name READ name CONSTANT)
    Q_PROPERTY(QUrl icon READ icon CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QVariantMap systemProperties READ systemProperties CONSTANT SCRIPTABLE true)
    Q_PROPERTY(QVariantMap applicationProperties READ applicationProperties CONSTANT SCRIPTABLE true)

public:
    virtual QString applicationId() const = 0;
    virtual QVariantMap name() const = 0;
    virtual QUrl icon() const = 0;
    virtual QString version() const = 0;
    virtual QVariantMap systemProperties() const = 0;
    virtual QVariantMap applicationProperties() const = 0;

#ifdef Q_QDOC
    Q_INVOKABLE Notification *createNotification();
    Q_INVOKABLE virtual void acknowledgeQuit() const;
#endif
    Q_SCRIPTABLE virtual void finishedInitialization() = 0;

signals:
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void memoryLowWarning();
    Q_SCRIPTABLE void memoryCriticalWarning();

    Q_SCRIPTABLE void openDocument(const QString &documentUrl, const QString &mimeType);
    Q_SCRIPTABLE void interfaceCreated(const QString &interfaceName);

    Q_SCRIPTABLE void slowAnimationsChanged(bool isSlow);

protected:
    ApplicationInterface(QObject *parent)
        : QObject(parent)
    { }

private:
    Q_DISABLE_COPY(ApplicationInterface)
};

QT_END_NAMESPACE_AM
