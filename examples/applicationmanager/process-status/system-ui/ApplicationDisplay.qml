/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
import QtApplicationManager.SystemUI 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

Frame {
    id: root
    property string name
    property var application

    width: 450

    RowLayout {
        width: parent.width

        Column {
            id: iconAndText

            Image {
                source: root.application.icon
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.application.runState === Am.Running ? application.stop() : application.start()
                }
            }
            Label {
                font.pixelSize: 18
                text: root.name
            }

        }

        Frame {
            opacity: root.application.runState === Am.Running ? 1 : 0
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    TabButton {
                        text: "Stats"
                        font.pixelSize: 15
                    }
                    TabButton {
                        text: "CPU Graph"
                        font.pixelSize: 15
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    currentIndex: tabBar.currentIndex
                    Stats {
                        application: root.application
                    }
                    CpuGraph {
                        application: root.application
                    }
                }
            }
        }

    }
}
