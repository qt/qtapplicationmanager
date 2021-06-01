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

import QtQuick 2.11
import QtQuick.Controls 2.4

Frame {
    id: root

    property alias sourceModel: listView.model
    property string role

    signal removeClicked()

    ListView {
        id: listView
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: (root.width / root.sourceModel.count) * 0.2
        clip: true
        interactive: false

        property real maxValue: 0

        delegate: Rectangle {
            width: (root.width / root.sourceModel.count) * 0.8
            height: scaledValue * root.height
            property real scaledValue: listView.maxValue > 0 ? value / listView.maxValue : 0
            readonly property real value: model[root.role]
            onValueChanged: {
                if (value > listView.maxValue)
                    listView.maxValue = value * 1.3;
            }
            y: root.height - height
            color: root.palette.highlight
        }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        text: root.role
    }

    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        text: "✖"
        flat: true
        onClicked: root.removeClicked()
    }
}
