/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>

#include "launchermain.h"
#include "logging.h"
#include "dbusapplicationinterface.h"
#include "dbusnotification.h"


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
        launcher.setWindowProperty(popup1->windowHandle(), "type", QStringLiteral("pop-up"));
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
