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

#include "launchermain.h"
#include "logging.h"


int main(int argc, char *argv[])
{
    QtAM::Logging::initialize(argc, argv);

    QtAM::LauncherMain::initialize();
    QApplication app(argc, argv);
    QtAM::LauncherMain launcher;

    launcher.registerWaylandExtensions();
    launcher.loadConfiguration();
    launcher.setupDBusConnections();
    launcher.setupLogging(false, launcher.loggingRules(), QString(), launcher.useAMConsoleLogger());

    QPushButton window("I'm a top-level window!\nClick to open/close a popup.");
    QDialog *popup = new QDialog(&window);
    (new QPushButton("I'm a popup!", popup))->resize(300, 100);

    window.setStyleSheet("QPushButton { background-color : lightgrey; font-size: 36px; }");
    popup->setStyleSheet("QPushButton { background-color : green; color : white; font-size: 24px; }");

    QObject::connect(&window, &QPushButton::clicked, [&popup, &launcher] () {
        popup->setVisible(!popup->isVisible());
        launcher.setWindowProperty(popup->windowHandle(), "type", QVariant::fromValue(QStringLiteral("pop-up")));
    });

    app.processEvents();
    window.showNormal();

    return app.exec();
}
