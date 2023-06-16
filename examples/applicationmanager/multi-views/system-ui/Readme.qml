// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick

Item {
    anchors.fill: parent

    Column {
        width: heading.implicitWidth
        height: heading.implicitHeight + 80
        anchors.centerIn: parent
        spacing: 10

        Text {
            id: heading
            color: "cornflowerblue"
            font { pixelSize: 20; weight: Font.Bold }
            text: "Multiple WindowItems displaying the same WindowObject"
        }
    }
}
