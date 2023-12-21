// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QApplication>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>

#include <QtAppManCommon/logging.h>
#include <QtAppManApplicationMain/applicationmain.h>
#include <QtAppManSharedMain/notification.h>


int main(int argc, char *argv[])
{
    QtAM::Logging::initialize(argc, argv);
    QtAM::Logging::setApplicationId("widgets");

    try {
        QtAM::ApplicationMain am(argc, argv);

        am.setup();

        QWidget window;
        QVBoxLayout layout(&window);

        // Popup using application manager window property
        QPushButton button1(QStringLiteral("Click to open/close a popup"));
        button1.setStyleSheet(QStringLiteral("QPushButton { background-color : limegreen; font-size: 36px; }"));
        layout.addWidget(&button1);

        QDialog *popup1 = new QDialog(&window);
        (new QPushButton(QStringLiteral("I'm a popup!"), popup1))->resize(340, 140);
        popup1->setStyleSheet(QStringLiteral("QPushButton { background-color : limegreen; color : white; font-size: 24px; }"));
        QObject::connect(&button1, &QPushButton::clicked, popup1, [&popup1, &am] () {
            popup1->setVisible(!popup1->isVisible());
            am.setWindowProperty(popup1->windowHandle(), QStringLiteral("type"), QStringLiteral("pop-up"));
        });

        // Notification
        QPushButton button2(QStringLiteral("Click to open a notification"));
        button2.setStyleSheet(QStringLiteral("QPushButton { background-color : darkgrey; font-size: 36px; }"));
        layout.addWidget(&button2);

        QtAM::Notification *notification = am.createNotification(&am);
        notification->setSummary(QStringLiteral("I'm a notification"));
        QObject::connect(&button2, &QPushButton::clicked, notification, &QtAM::Notification::show);

        // Application interface for handling quit
        QObject::connect(&am, &QtAM::ApplicationMain::quit, &am, &QCoreApplication::quit);

        am.processEvents();
        window.showNormal();

        return am.exec();

    } catch (const std::exception &e) {
        qWarning() << "ERROR:" << e.what();
    }
}
