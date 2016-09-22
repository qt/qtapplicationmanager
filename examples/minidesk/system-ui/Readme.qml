/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

import QtQuick 2.4

Item {
    anchors.fill: parent

    Column {
        width: paragraph.implicitWidth
        height: heading.implicitHeight + paragraph.implicitHeight + 80
        anchors.centerIn: parent
        spacing: 10

        Text {
            id: heading
            color: "cornflowerblue"
            font { pixelSize: 20; weight: Font.Bold }
            text: "Minimal Desktop System UI in pure QML"
        }

        Text {
            id: paragraph
            color: "grey"
            font.pixelSize: 16
            text: "The following features are supported:\n" +
                  "\u2022 Start applications by clicking an icon on the top left\n" +
                  "\u2022 Stop applications by clicking on the top/left window decoration rectangle\n" +
                  "\u2022 Raise applications by clicking on the decoration\n" +
                  "\u2022 Drag windows by pressing on the window decoration and moving\n" +
                  "\u2022 System UI sends 'propA' change when an app is started\n" +
                  "\u2022 System UI and App2 react on window property changes with a debug message\n" +
                  "\u2022 App1 animation can be stopped and restarted by a click\n" +
                  "\u2022 App1 sends 'rotation' property to system UI on stop\n" +
                  "\u2022 App1 shows a pop-up on the system UI while it is paused\n" +
                  "\u2022 App2 will make use of an IPC extension when it is started\n" +
                  "\u2022 App2 logs the document URL it has been started with\n" +
                  "\u2022 App2 triggers a notification in the system UI when the bulb icon is clicked\n" +
                  "\u2022 A separate (\"wayland\") process started outside of appman will be shown as App1"
        }
    }
}
