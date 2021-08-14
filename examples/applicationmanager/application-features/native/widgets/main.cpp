/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>

#include <QtAppManCommon/logging.h>
#include <QtAppManLauncher/launchermain.h>
#include <QtAppManLauncher/dbusapplicationinterface.h>
#include <QtAppManLauncher/dbusnotification.h>


int main(int argc, char *argv[])
{
    QtAM::Logging::initialize(argc, argv);
    QtAM::Logging::setApplicationId("widgets");

    QtAM::LauncherMain::initialize();
    QApplication app(argc, argv);
    QtAM::LauncherMain launcher;

    launcher.registerWaylandExtensions();
    launcher.loadConfiguration();
    launcher.setupLogging(false, launcher.loggingRules(), QString(), launcher.useAMConsoleLogger());
    launcher.setupDBusConnections();

    QWidget window;
    QVBoxLayout layout(&window);

    // Popup using application manager window property
    QPushButton button1(QStringLiteral("Click to open/close a popup"));
    button1.setStyleSheet(QStringLiteral("QPushButton { background-color : limegreen; font-size: 36px; }"));
    layout.addWidget(&button1);

    QDialog *popup1 = new QDialog(&window);
    (new QPushButton(QStringLiteral("I'm a popup!"), popup1))->resize(340, 140);
    popup1->setStyleSheet(QStringLiteral("QPushButton { background-color : limegreen; color : white; font-size: 24px; }"));
    QObject::connect(&button1, &QPushButton::clicked, [&popup1, &launcher] () {
        popup1->setVisible(!popup1->isVisible());
        launcher.setWindowProperty(popup1->windowHandle(), QStringLiteral("type"), QStringLiteral("pop-up"));
    });

    // Notification
    QPushButton button2(QStringLiteral("Click to open a notification"));
    button2.setStyleSheet(QStringLiteral("QPushButton { background-color : darkgrey; font-size: 36px; }"));
    layout.addWidget(&button2);

    QtAM::DBusNotification *notification = QtAM::DBusNotification::create(&app);
    notification->setSummary(QStringLiteral("I'm a notification"));
    QObject::connect(&button2, &QPushButton::clicked, notification, &QtAM::DBusNotification::show);

    // Application interface for handling quit
    QtAM::DBusApplicationInterface iface(launcher.p2pDBusName(), launcher.notificationDBusName());
    iface.initialize();
    QObject::connect(&iface, &QtAM::DBusApplicationInterface::quit, [&iface] () {
        iface.acknowledgeQuit();
    });

    app.processEvents();
    window.showNormal();

    return app.exec();
}
